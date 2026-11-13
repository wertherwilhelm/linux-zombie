#pragma once

#include "gc_traits.h"
#include "gc_allocator.h"  // Need full definition for MallocAllocator
#include <cstddef>
#include <cstdlib>  // for malloc/free
#include <new>

// Forward declarations
class gc_object;

/**
 * @file gc_box.h
 * @brief Allocation wrapper that attaches GC metadata to objects
 *
 * This file implements the gc_box allocation strategy where each Object
 * is prefixed with a header containing its tracer function pointer.
 *
 * Memory layout:
 * [gc_header: TraceFn, size, flags] [Object Data...]
 *                                     ^
 *                                     Returned pointer
 *
 * This allows virtual-free tracing by storing the type-specific TraceFn
 * with each allocation instead of relying on virtual trace_references().
 */

/**
 * @brief Type-erased destructor function pointer
 *
 * Called before deallocating an Object. Invokes the proper derived class
 * destructor via virtual dispatch or direct call.
 *
 * @param obj Pointer to the Object to destroy
 */
using DestructFn = void (*)(void* obj);

/**
 * @brief Header stored before each GC-managed Object
 *
 * This header is allocated immediately before the Object data.
 * Due to alignment requirements, there may be padding between the header
 * and the Object. The padding field stores the number of padding bytes.
 *
 * Memory layout:
 * [gc_header] [padding bytes...] [Object]
 *             ^--- padding bytes
 */
struct gc_header {
    TraceFn tracer;        // Type-specific tracing function
    DestructFn destructor; // Type-specific destruction function
    size_t size;           // Size of the Object (for deallocation)
    size_t alignment;      // Alignment requirement for this Object
    size_t padding;        // Number of padding bytes between header and Object
    bool marked;           // Mark bit for mark-and-sweep
    void* forwarding_addr; // Forwarding pointer for copying collection (nullptr if not forwarded)

    gc_header() : tracer(nullptr), destructor(nullptr), size(0), alignment(0), padding(0), marked(false), forwarding_addr(nullptr) {}
};

/**
 * @brief Allocation wrapper namespace
 *
 * Provides functions to allocate objects with gc_header prefix
 * and recover headers from Object pointers.
 */
namespace gc_box {

/**
 * @brief Get the GC header for an Object
 *
 * Accounts for alignment padding between header and Object.
 * The padding amount is stored in a size_t immediately before the Object.
 *
 * @param obj Pointer to the Object
 * @return Pointer to the header (before the Object and padding)
 */
inline gc_header* get_header(void* obj) {
    if (!obj) return nullptr;

    // Read padding from size_t stored immediately before Object
    size_t* padding_ptr = reinterpret_cast<size_t*>(obj) - 1;
    size_t total_offset = *padding_ptr;

    // Header is total_offset bytes before the Object
    char* obj_bytes = reinterpret_cast<char*>(obj);
    return reinterpret_cast<gc_header*>(obj_bytes - total_offset);
}

/**
 * @brief Get the GC header for an Object (const version)
 */
inline const gc_header* get_header(const void* obj) {
    if (!obj) return nullptr;

    // Read padding from size_t stored immediately before Object
    const size_t* padding_ptr = reinterpret_cast<const size_t*>(obj) - 1;
    size_t total_offset = *padding_ptr;

    // Header is total_offset bytes before the Object
    const char* obj_bytes = reinterpret_cast<const char*>(obj);
    return reinterpret_cast<const gc_header*>(obj_bytes - total_offset);
}

// Forward declaration: defined later in this header. GCC's two-phase name
// lookup requires the declaration be visible before the dependent call in
// allocate() below (MSVC performed the lookup lazily).
template<typename T>
inline DestructFn get_destruct_fn();

/**
 * @brief Allocate an Object with GC header using custom allocator
 *
 * @tparam T Object type to allocate
 * @tparam Alloc Allocator type (must have allocate method)
 * @tparam Args Constructor argument types
 * @param alloc Allocator to use for memory
 * @param args Constructor arguments
 * @return Pointer to the allocated Object (header is before this)
 */
template<typename T, typename Alloc, typename... Args>
    requires requires(Alloc& a) { a.allocate(size_t{}, size_t{}); }
T* allocate(Alloc& alloc, Args&&... args) {
    // Memory layout: [gc_header] [padding] [size_t offset] [Object]
    //                                       ^-- stored here so get_header can find header

    constexpr size_t header_size = sizeof(gc_header);
    constexpr size_t obj_alignment = alignof(T);
    constexpr size_t obj_size = sizeof(T);
    constexpr size_t offset_size = sizeof(size_t);

    // We need to ensure that the Object is aligned to obj_alignment
    // and that the size_t offset before it is also properly placed

    // Calculate position after header where we'd like to put the offset
    size_t pos_after_header = header_size;

    // Align for the offset (size_t)
    size_t offset_pos = (pos_after_header + alignof(size_t) - 1) & ~(alignof(size_t) - 1);

    // Calculate position for Object (after offset)
    size_t pos_after_offset = offset_pos + offset_size;

    // Align for the Object
    size_t obj_pos = (pos_after_offset + obj_alignment - 1) & ~(obj_alignment - 1);

    // Total allocation size
    size_t total_size = obj_pos + obj_size;

    // Request allocation with alignment suitable for both header and Object
    constexpr size_t max_align = (alignof(gc_header) > obj_alignment)
        ? alignof(gc_header) : obj_alignment;

    void* memory = alloc.allocate(total_size, max_align);
    if (!memory) throw std::bad_alloc();

    // Initialize header
    gc_header* header = static_cast<gc_header*>(memory);
    new (header) gc_header();  // Placement new for header
    header->tracer = get_trace_fn<T>();
    header->destructor = get_destruct_fn<T>();
    header->size = obj_size;
    header->alignment = obj_alignment;
    header->padding = obj_pos - header_size;  // Total padding including offset
    header->marked = false;

    // Store offset immediately before Object so get_header can find the header
    char* Base = reinterpret_cast<char*>(memory);
    size_t* offset_ptr = reinterpret_cast<size_t*>(Base + obj_pos - offset_size);
    *offset_ptr = obj_pos;  // Distance from Object back to header

    // Get aligned Object position
    T* obj = reinterpret_cast<T*>(Base + obj_pos);

    // Initialize Object with exception safety
    try {
        new (obj) T(std::forward<Args>(args)...);
    } catch (...) {
        // Constructor threw - deallocate memory and rethrow
        alloc.deallocate(memory, total_size);
        throw;
    }

    return obj;
}

// Forward declare default allocator (defined in gc_box.cpp)
extern MallocAllocator default_allocator;

/**
 * @brief Allocate an Object with GC header using default MallocAllocator
 *
 * Convenience overload for backward compatibility.
 * Uses explicit template argument to prevent recursion.
 *
 * @tparam T Object type to allocate
 * @tparam Args Constructor argument types
 * @param args Constructor arguments
 * @return Pointer to the allocated Object (header is before this)
 */
template<typename T, typename... Args>
inline T* allocate(Args&&... args) {
    // Explicit template argument prevents recursion:
    // This calls allocate<T, MallocAllocator>(...), not allocate<T>(...)
    return allocate<T, MallocAllocator>(default_allocator, std::forward<Args>(args)...);
}

/**
 * @brief Deallocate an Object with GC header using custom allocator
 *
 * Uses the type-erased destructor stored in the header to properly
 * destroy the Object (with virtual dispatch if applicable).
 *
 * @tparam Alloc Allocator type
 * @param alloc Allocator to use for deallocation
 * @param obj Pointer to the Object (not the header)
 */
template<typename Alloc>
void deallocate(Alloc& alloc, void* obj) {
    if (!obj) return;

    // Get header before Object
    gc_header* header = get_header(obj);
    if (!header) return;

    // Call type-erased destructor (properly handles derived class dtors)
    if (header->destructor) {
        header->destructor(obj);
    }

    // Calculate total allocation size (header + padding + offset + Object)
    char* header_ptr = reinterpret_cast<char*>(header);
    char* obj_ptr = reinterpret_cast<char*>(obj);
    size_t offset_from_header = obj_ptr - header_ptr;
    size_t total_size = offset_from_header + header->size;

    // Deallocate the entire block
    alloc.deallocate(header, total_size);
}

/**
 * @brief Legacy deallocate function (backward compatibility)
 *
 * Uses default MallocAllocator. Prefer templated version with explicit allocator.
 *
 * @param obj Object to deallocate
 */
void deallocate(void* obj);

/**
 * @brief Trace an Object using its stored TraceFn
 *
 * @param tracer Tracer to use for visiting children
 * @param obj Object to trace
 */
inline void trace(Tracer& tracer, void* obj) {
    if (!obj) return;

    gc_header* header = get_header(obj);
    if (header && header->tracer) {
        header->tracer(tracer, obj);
    }
}

/**
 * @brief Check if an Object is marked
 */
inline bool is_marked(const void* obj) {
    if (!obj) return false;
    const gc_header* header = get_header(obj);
    return header ? header->marked : false;
}

/**
 * @brief Set the marked flag on an Object
 */
inline void set_marked(void* obj, bool marked) {
    if (!obj) return;
    gc_header* header = get_header(obj);
    if (header) {
        header->marked = marked;
    }
}

/**
 * @brief Get the size of an allocated Object
 */
inline size_t get_size(const void* obj) {
    if (!obj) return 0;
    const gc_header* header = get_header(obj);
    return header ? header->size : 0;
}

/**
 * @brief Get type-specific destructor function for type T
 *
 * Returns a function pointer that properly destroys an instance of T.
 * Uses virtual dispatch if T has a virtual destructor.
 *
 * @tparam T Type to get destructor for
 * @return Function pointer that destroys T instances
 */
template<typename T>
inline DestructFn get_destruct_fn() {
    return [](void* obj) {
        if (obj) {
            // Call the actual destructor (with virtual dispatch if T has virtual dtor)
            static_cast<T*>(obj)->~T();
        }
    };
}

} // namespace gc_box
