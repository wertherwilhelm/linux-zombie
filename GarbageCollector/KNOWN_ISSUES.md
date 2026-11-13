# GarbageCollector Known Issues and Limitations

This document describes known issues and architectural limitations in the GarbageCollector library that require design decisions before fixing.

## Critical Fixes Applied (December 31, 2025)

The following critical bugs have been FIXED and have comprehensive test coverage:

### ✅ Fixed: Non-virtual Destructor Call (CRITICAL)
**Issue**: `gc_box::deallocate()` explicitly called `~gc_object()`, bypassing virtual dispatch.
**Impact**: Derived class destructors never ran, causing resource leaks for objects with cleanup logic.
**Fix**: Added type-erased `DestructFn` in `gc_header` that captures and invokes the correct destructor with virtual dispatch.
**Test Coverage**: `GarbageCollectorDestructorTest::CallsDerivedDestructor`

### ✅ Fixed: Alignment Violations (HIGH)
**Issue**: `gc_box::allocate()` placed objects immediately after `gc_header` without padding, causing misalignment for over-aligned types.
**Impact**: Undefined behavior for types with `alignas(N)` where N > alignof(gc_header).
**Fix**:
- Calculate proper alignment padding in `gc_box::allocate()`
- Store offset before object so `get_header()` can navigate back through padding
- Updated `MallocAllocator` to honor alignment via platform-specific aligned allocation

**Test Coverage**: `GarbageCollectorAlignmentTest::RespectsOverAlignment` (tests alignas(64))

### ✅ Fixed: Constructor Exception Leak (MEDIUM)
**Issue**: If a constructor threw an exception, allocated memory leaked.
**Impact**: Memory leak on every constructor failure.
**Fix**: Added try-catch block in `gc_box::allocate()` to deallocate on exception and rethrow.
**Test Coverage**: `GarbageCollectorExceptionTest::HandlesConstructorThrow`

---

## Known Issues Requiring Design Decisions

### 1. ✅ RESOLVED: Reentrant Allocation During Collection

**Status**: FIXED (December 31, 2025)

**Location**: `GarbageCollector/mark_sweep_strategy.cpp:18-39`

**Previous Issue**: Allocations during `trace_references()` were silently dropped, causing memory leaks.

**Solution Implemented**:
- Added `pending_allocations_` vector to queue allocations during collection
- New objects are immediately added to the marking worklist for tracing
- Pending allocations are integrated into `all_objects_` after sweep completes

**Implementation**:
```cpp
void MarkSweepStrategy::on_allocate(gc_object* obj) {
    if (collecting_) {
        // Queue for immediate tracing (supports LOD node creation during traversal)
        pending_allocations_.push_back(obj);
        if (marking_worklist_) {
            marking_worklist_->push_back(obj);  // Trace immediately
        }
    } else {
        all_objects_.push_back(obj);
    }
}
```

**Use Case Supported**: On-demand LOD (Level of Detail) node creation during scene graph traversal.

**Test Status**: Core implementation complete. Additional test coverage pending (see "Test Coverage Needed" section below).

---

### 2. ✅ RESOLVED: Per-GC Persistent Root Registry

**Status**: FIXED (December 31, 2025)

**Location**:
- `GarbageCollector/gc_persistent_root.h` (captures owning GC)
- `GarbageCollector/gc_persistent_root.cpp` (registers with captured GC)
- `GarbageCollector/mark_sweep_strategy.h` (per-instance root list)
- `GarbageCollector/mark_sweep_strategy.cpp` (instance-specific marking)

**Previous Issue**: Global static registry caused cross-contamination between multiple GC instances.

**Solution Implemented**:
- `gc_persistent_root` captures `gc_context::current()` at construction
- Each GC instance maintains its own `persistent_roots_` vector
- Registration calls `gc->register_persistent_root(&obj)` on the captured GC
- Mark phase iterates instance-specific roots (no global static)

**Implementation Changes**:
```cpp
// gc_persistent_root.h
struct root_entry {
    gc_object* obj;
    int refcount;
    IGarbageCollector* gc;  // NEW: Capture owning GC

    explicit root_entry(gc_object* o, IGarbageCollector* g)
        : obj(o), refcount(1), gc(g) {}
};

// mark_sweep_strategy.h
std::vector<gc_object**> persistent_roots_;  // Per-instance

// IGarbageCollector interface
virtual void register_persistent_root(gc_object** root_ptr) = 0;
virtual void unregister_persistent_root(gc_object** root_ptr) = 0;
```

**Thread Safety**: Each thread can have its own GC instance (via `gc_context` thread-local storage). Persistent roots automatically register with the correct thread-local GC.

**Use Case Supported**: Multi-threaded scene graph with thread-local GC instances for parallel traversal.

**Test Status**: Existing tests pass (29/29). Multi-GC-instance test coverage pending.

---

### 3. ✅ RESOLVED: gc_root_guard Scope Safety

**Status**: FIXED (December 31, 2025)

**Location**: `GarbageCollector/gc_manager.h:184-215`

**Previous Issue**: `gc_root_guard` queried `gc_context::current()` at destruction, causing incorrect cleanup if GC context was swapped mid-lifetime.

**Solution Implemented**: Capture GC pointer at construction time.

**Implementation**:
```cpp
class gc_root_guard {
public:
    gc_root_guard(gc_object* obj) : _obj(obj), _gc(gc_context::current()) {
        if (_obj && _gc)
            _gc->add_root(_obj);
    }

    ~gc_root_guard() {
        if (_obj && _gc)
            _gc->remove_root(_obj);  // Uses captured GC, not current!
    }

private:
    gc_object* _obj;
    IGarbageCollector* _gc;  // NEW: Captured at construction
};
```

**Thread Safety**: Ensures proper cleanup even when `gc_scope` temporarily swaps the thread-local GC context.

**Use Case Supported**: RAII guards remain correct when GC context is temporarily changed for nested operations.

**Test Status**: Basic functionality verified. gc_scope swap test coverage pending.

---

## Test Coverage Summary

**Total Tests**: 38 (100% passing as of December 31, 2025)

**Coverage by Category**:
- ✅ Basic GC operations (7 tests)
- ✅ Cyclic references (4 tests)
- ✅ Stress testing (4 tests)
- ✅ Edge cases (5 tests)
- ✅ Metrics/stats (3 tests)
- ✅ Destructor correctness (2 tests)
- ✅ Alignment handling (2 tests)
- ✅ Exception safety (2 tests)
- ✅ **Reentrant allocation (3 tests)** - NEW
- ✅ **Multi-GC instances (3 tests)** - NEW
- ✅ **gc_scope/gc_root_guard interaction (3 tests)** - NEW

**New Test Details**:

**GarbageCollectorReentrantTest** (validates on-demand LOD node creation):
1. `AllocatesDuringTraceReferences` - Objects allocated during trace phase are properly tracked
2. `MultiLevelExpansionDuringGC` - Multi-level hierarchy expansion in single GC cycle
3. `NewObjectsSurviveCollection` - Reentrant allocations survive subsequent collections

**GarbageCollectorMultiGCTest** (validates thread-local GC isolation):
1. `SeparateInstancesDoNotInterfere` - Independent GC instances collect independently
2. `PersistentRootsIsolatedPerGC` - Persistent roots register with correct GC instance
3. `ThreadLocalContextIsolation` - Thread-local context switching works correctly

**GarbageCollectorScopeTest** (validates RAII correctness with context swapping):
1. `RootGuardSurvivesContextSwap` - gc_root_guard captures GC at construction
2. `NestedScopeSwaps` - Nested context swaps unwind correctly
3. `ScopeRAIIHelper` - gc_scope properly saves and restores context

---

## Usage Recommendations

### Safe Usage Patterns ✅

```cpp
// Single GC instance per process
default_gc gc;

// Type-erased destruction works correctly
struct MyType : gc_object {
    ~MyType() override { /* cleanup */ }
};

// Over-aligned types work correctly
struct alignas(64) AlignedType : gc_object { ... };

// Constructor exceptions are safe
try {
    gc.allocate<ThrowingType>();
} catch (...) {
    // Memory properly deallocated
}
```

### Patterns to Avoid ⚠️

```cpp
// AVOID: Multiple GC instances with persistent roots
default_gc gc1;
default_gc gc2;
gc_persistent_root root1(obj_from_gc1);  // May confuse gc2!

// AVOID: Allocation during trace_references()
void trace_references(Tracer& t) override {
    auto* new_obj = gc->allocate<Foo>();  // LEAKS!
    t.visit(child);
}

// AVOID: Swapping GC context while root guard is active
gc_root_guard guard(obj);
gc_scope scope(other_gc);  // guard will unregister from wrong GC!
```

---

## Architectural Improvements Implemented

**December 31, 2025 Session Summary**:

1. ✅ **Reentrant Allocation Support**
   - Objects can now be allocated during `trace_references()`
   - Immediately added to marking worklist for tracing
   - Enables on-demand LOD node creation during GC

2. ✅ **Per-GC Persistent Root Registry**
   - Eliminated global static registry
   - Each GC instance maintains its own root list
   - Enables thread-local GC instances for parallel traversal
   - `gc_persistent_root` captures owning GC at construction

3. ✅ **gc_root_guard Scope Safety**
   - Captures GC at construction (not at destruction)
   - Correct cleanup even when `gc_scope` swaps context
   - Thread-safe with nested GC operations

**Impact**: GarbageCollector library is now ready for multi-threaded scene graph with:
- Thread-local GC instances
- On-demand LOD hierarchy creation during traversal
- R-tree spatial indexing backend (objects converted on-the-fly)
- No cross-contamination between thread-specific GCs

## Future Work

1. **Testing**: Add comprehensive tests for architectural improvements (reentrant allocation, multi-GC, gc_scope)
2. **Concurrent GC**: Implement incremental or concurrent collection to avoid pausing render thread
3. **Documentation**: Update README with thread-local GC patterns and best practices

---

*Last Updated*: December 31, 2025
*Fixed Issues*: Destructor dispatch, alignment handling, exception safety, reentrant allocation, per-GC roots, gc_root_guard safety
*Status*: All critical architectural issues resolved; **38/38 tests passing (100%)**
*Test Coverage*: Comprehensive coverage including LOD pattern, multi-GC, and RAII interaction
