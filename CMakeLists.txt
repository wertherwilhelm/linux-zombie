cmake_minimum_required(VERSION 3.16)
project(ZombieScheme LANGUAGES CXX)

# ---------------------------------------------------------------------------
# ZombieScheme — Linux/CMake port of the original Visual Studio solution.
#
# Dependency graph (mirrors the old .sln ProjectReferences):
#   GarbageCollector
#     -> ZombieRuntime
#          -> ZombieBytecode
#               -> ZombieCompilerIR
#                    -> ZombieLib  (embedding facade)
#                         -> REPL-test (executable)
#                         -> LispTest (GoogleTest suite, optional)
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(ZOMBIE_BUILD_TESTS "Build the LispTest GoogleTest suite" ON)

# The original code uses quoted cross-project includes such as
# "gc_manager.h", "scheme_engine.h", "../ZombieBytecode/bytecode_opcodes.h".
# Make every project directory (and the repo root) visible to all targets.
#
# Ordering matters: every per-project pch.h shares the same `#ifndef PCH_H`
# guard. Top-level .cpp files resolve "pch.h" from their own directory first,
# but files in the ZombieRuntime/primitives/ subdirectory have no local pch.h
# and fall through to the search path. ZombieRuntime must therefore precede
# GarbageCollector, so those files pick up ZombieRuntime/pch.h (which pulls in
# the zombie::runtime namespace bridging) rather than GarbageCollector/pch.h.
set(ZOMBIE_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/ZombieRuntime
  ${CMAKE_CURRENT_SOURCE_DIR}/ZombieBytecode
  ${CMAKE_CURRENT_SOURCE_DIR}/ZombieCompilerIR
  ${CMAKE_CURRENT_SOURCE_DIR}/ZombieLib
  ${CMAKE_CURRENT_SOURCE_DIR}/GarbageCollector
)

add_compile_options(-Wno-deprecated-declarations)

# Helper: collect sources for a component, optionally excluding files.
function(zombie_glob out_var dir)
  file(GLOB_RECURSE _srcs CONFIGURE_DEPENDS "${dir}/*.cpp")
  if(ARGN)
    foreach(_ex IN LISTS ARGN)
      list(FILTER _srcs EXCLUDE REGEX "${_ex}")
    endforeach()
  endif()
  set(${out_var} "${_srcs}" PARENT_SCOPE)
endfunction()

# --- GarbageCollector -------------------------------------------------------
# Exclude the standalone demo (defines its own main()).
zombie_glob(GC_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/GarbageCollector"
  "gc_ergonomics_demo_v2\\.cpp$")
add_library(GarbageCollector STATIC ${GC_SRCS})
target_include_directories(GarbageCollector PUBLIC ${ZOMBIE_INCLUDE_DIRS})

# --- ZombieRuntime (includes primitives/ subdirectory) ----------------------
zombie_glob(RUNTIME_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/ZombieRuntime")
add_library(ZombieRuntime STATIC ${RUNTIME_SRCS})
target_include_directories(ZombieRuntime PUBLIC ${ZOMBIE_INCLUDE_DIRS})
target_link_libraries(ZombieRuntime PUBLIC GarbageCollector)

# --- ZombieBytecode ---------------------------------------------------------
zombie_glob(BYTECODE_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/ZombieBytecode")
add_library(ZombieBytecode STATIC ${BYTECODE_SRCS})
target_include_directories(ZombieBytecode PUBLIC ${ZOMBIE_INCLUDE_DIRS})
target_link_libraries(ZombieBytecode PUBLIC ZombieRuntime)

# --- ZombieCompilerIR -------------------------------------------------------
zombie_glob(COMPILERIR_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/ZombieCompilerIR")
add_library(ZombieCompilerIR STATIC ${COMPILERIR_SRCS})
target_include_directories(ZombieCompilerIR PUBLIC ${ZOMBIE_INCLUDE_DIRS})
target_link_libraries(ZombieCompilerIR PUBLIC ZombieBytecode)

# ZombieBytecode's VM (bytecode_vm.cpp) calls zombie::bytecode::Compiler, which
# lives in ZombieCompilerIR — a genuine cycle between the two static libs.
# MSVC resolved it implicitly; declaring both directions lets CMake/GNU ld
# repeat the archives on the link line to satisfy the cross-references.
target_link_libraries(ZombieBytecode PUBLIC ZombieCompilerIR)

# --- ZombieLib (embedding facade) ------------------------------------------
zombie_glob(ZOMBIELIB_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/ZombieLib")
add_library(ZombieLib STATIC ${ZOMBIELIB_SRCS})
target_include_directories(ZombieLib PUBLIC ${ZOMBIE_INCLUDE_DIRS})
target_link_libraries(ZombieLib PUBLIC ZombieCompilerIR)

# --- REPL executable --------------------------------------------------------
zombie_glob(REPL_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/REPL-test")
add_executable(repl ${REPL_SRCS})
target_link_libraries(repl PRIVATE ZombieLib)

# --- Tests (GoogleTest) -----------------------------------------------------
if(ZOMBIE_BUILD_TESTS)
  include(FetchContent)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
  )
  # Allow an offline/system GoogleTest as a fallback.
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    FetchContent_MakeAvailable(googletest)
  endif()

  # GCBoxTests.cpp is marked <ExcludedFromBuild> in the original LispTest
  # vcxproj (it uses a stale add_root(object*) API); keep it excluded here.
  zombie_glob(TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/LispTest"
    "GCBoxTests\\.cpp$")
  add_executable(LispTest ${TEST_SRCS})
  target_link_libraries(LispTest PRIVATE ZombieLib)
  # Absolute path to the test fixtures so file-loading tests work regardless of the
  # process CWD (e.g. running ./LispTest from the build dir, not just via ctest's
  # WORKING_DIRECTORY). Replaces the old hardcoded Windows paths in the tests.
  target_compile_definitions(LispTest PRIVATE
    "ZOMBIE_TEST_LIBS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/test-libs\"")
  if(TARGET gtest)
    target_link_libraries(LispTest PRIVATE gtest gtest_main)
  else()
    target_link_libraries(LispTest PRIVATE GTest::gtest GTest::gtest_main)
  endif()

  enable_testing()
  include(GoogleTest)
  # Register every gtest case as its own CTest entry so `ctest` lists and
  # reports all tests individually (not one bundled entry).
  #
  # The experimental templated-GC tests (gc_manager_t, GCAllocatorTest.*) used to
  # crash on Linux/glibc with "pure virtual method called". Root cause was a
  # member-destruction-order bug in gc_manager_template.h: the allocator was
  # destroyed before the strategy, so MarkSweepStrategy::shutdown() reclaimed
  # surviving objects through an already-destroyed allocator (virtual deallocate()
  # faulted as a pure-virtual call). Fixed by declaring m_alloc before m_strategy.
  # The tests now pass under both -O0 and -fsanitize=address, so they run in CI.
  #
  # WORKING_DIRECTORY is the repo root so the R7RSLibraryTest cases find test-libs/.
  # The file-IO suites share fixed scratch files (e.g. test_io_read.txt,
  # test_bytevector_io.bin) across their own test cases. When ctest runs cases
  # as separate processes under -j they race on those files. A shared
  # RESOURCE_LOCK makes ctest never run two of them at once, while the ~1200
  # other cases still parallelize. (In-process `./LispTest` is unaffected — gtest
  # runs serially there.)
  set(ZOMBIE_FILEIO_FILTER
      "R7RSIOTest.*:R7RSBytevectorIOTest.*:R7RSBytevectorBlockIOTest.*:LoadTest.*")

  # Parallel-safe cases: everything except the file-IO suites.
  gtest_discover_tests(LispTest
    TEST_FILTER "-${ZOMBIE_FILEIO_FILTER}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DISCOVERY_TIMEOUT 60)

  # File-IO cases: registered individually but serialized via a shared lock.
  gtest_discover_tests(LispTest
    TEST_FILTER "${ZOMBIE_FILEIO_FILTER}"
    PROPERTIES RESOURCE_LOCK zombie_fileio
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DISCOVERY_TIMEOUT 60)
endif()
