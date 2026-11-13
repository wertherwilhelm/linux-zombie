#pragma once

#include <cstddef>  // for size_t

/**
 * @file IMemoryManager.h
 * @brief Core memory management interface (REQUIRED for all allocators)
 *
 * This is the minimal interface that ALL memory management strategies must implement.
 * Whether using GC, malloc, custom pools, or smart pointers - all must provide
 * allocation and deallocation.
 *
 * Design Philosophy:
 * - Minimal required interface (just allocate/deallocate)
 * - Optional capabilities detected via concepts (see concepts.h)
 * - No GC-specific requirements at this layer
 * - Thread-safety is implementation-defined (document in concrete classes)
 *
 * Thread-Safety Pattern:
 * - Use thread_local storage for per-thread allocators
 * - Access via thread-local context (e.g., gc_context::current())
 * - No shared state = no locks needed
 * - Scales perfectly to N threads
 */

namespace memory {

/**
 * @brief Abstract memory manager interface
 *
 * This is the foundation of the memory management abstraction hierarchy.
 * All allocators (GC, malloc, pools, arenas) must implement this interface.
 *
 * Implementations may provide additional capabilities:
 * - ITraceable (GC tracing)
 * - IWriteBarrier (generational GC)
 * - IRootManager (GC roots)
 *
 * Use concepts (SupportsTracing, SupportsWriteBarrier, etc.) to detect
 * capabilities at compile time.
 *
 * Thread Safety:
 * - Each implementation defines its own thread-safety guarantees
 * - Recommended pattern: Thread-local instances (one per thread)
 * - Alternative: Thread-safe with internal locking (slower)
 * - Document thread-safety in derived classes
 *
 * Example:
 *   // Thread-local GC (recommended)
 *   thread_local MyGC gc;
 *   void* ptr = gc.allocate(100, 8);
 *
 *   // Shared GC (requires locking)
 *   SharedGC global_gc;  // Has internal mutex
 *   void* ptr = global_gc.allocate(100, 8);
 */
class IMemoryManager {
public:
    virtual ~IMemoryManager() = default;

    /**
     * @brief Allocate memory
     * @param size Size in bytes to allocate
     * @param alignment Alignment requirement (must be power of 2)
     * @return Pointer to allocated memory, or nullptr on failure
     *
     * Guarantees:
     * - Returned pointer is aligned to `alignment`
     * - Memory is uninitialized (caller must construct objects)
     * - nullptr indicates allocation failure (out of memory)
     *
     * Thread Safety: Implementation-defined
     *
     * Performance Notes:
     * - GC allocators: Fast (bump-pointer in young gen)
     * - Pool allocators: O(1) for fixed-size blocks
     * - Malloc: Variable (depends on heap fragmentation)
     */
    virtual void* allocate(size_t size, size_t alignment) = 0;

    /**
     * @brief Deallocate memory
     * @param ptr Pointer to free (must have been allocated by this manager)
     * @param size Size of the allocation (hint for pool allocators)
     *
     * Notes:
     * - GC allocators: Often a no-op (GC handles collection)
     * - Manual allocators: Must free memory immediately
     * - Size parameter is a hint (pool allocators can bucket by size)
     * - Passing nullptr is a no-op (like C++ delete)
     *
     * Thread Safety: Implementation-defined
     *
     * IMPORTANT: Only call on memory allocated by THIS manager.
     * Mixing allocators (allocate from A, deallocate to B) is undefined behavior.
     */
    virtual void deallocate(void* ptr, size_t size) = 0;

    /**
     * @brief Get human-readable name of allocator (for debugging)
     * @return Name of allocator (e.g., "MarkSweepGC", "MallocAllocator")
     *
     * Optional: Default implementation returns type name via typeid
     */
    virtual const char* name() const {
        return "IMemoryManager";
    }

    /**
     * @brief Get current bytes allocated (for profiling/debugging)
     * @return Approximate bytes currently allocated
     *
     * Optional: Default implementation returns 0 (unknown)
     * Useful for memory usage tracking and GC heuristics.
     */
    virtual size_t bytes_allocated() const {
        return 0;  // Unknown by default
    }
};

} // namespace memory
