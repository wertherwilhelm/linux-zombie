#pragma once

#include <cstddef>

namespace memory {

/**
 * @file IRootManager.h
 * @brief Optional root management interface for garbage collectors
 *
 * This is an OPTIONAL capability for memory managers that use tracing garbage
 * collection. Roots are the starting points for GC tracing (objects that are
 * always considered reachable).
 *
 * What are Roots?
 * - Stack variables (local variables in functions)
 * - Global variables
 * - Thread-local storage
 * - Static variables
 * - CPU registers (during conservative GC)
 *
 * Why Root Management?
 * - GC traces from roots to find all reachable objects
 * - Unreachable objects (not traceable from roots) are garbage
 * - Roots must be explicitly registered (GC can't auto-discover them)
 *
 * When Needed:
 * - Tracing GC (mark-sweep, copying, generational)
 * - Conservative GC (scans stack/registers as roots)
 *
 * When NOT Needed:
 * - Reference counting (no tracing)
 * - Manual memory management (malloc/free)
 * - Allocators without collection (arenas, pools)
 *
 * Capability Detection:
 *   template<typename T>
 *   concept SupportsRooting = requires(T& mgr, void** root) {
 *       { mgr.add_root(root) } -> std::same_as<void>;
 *       { mgr.remove_root(root) } -> std::same_as<void>;
 *   };
 */

/**
 * @brief Root management interface for garbage collectors
 *
 * Roots are the entry points for GC tracing. Any object reachable from roots
 * (directly or indirectly) is considered live and must not be collected.
 *
 * Root Types:
 *
 * 1. Stack Roots (local variables):
 *    - Short-lived (function scope)
 *    - High churn (push/pop on call/return)
 *    - Use gc_local<T*> or gc_root<T*> for RAII management
 *
 * 2. Global Roots (static/global variables):
 *    - Long-lived (program lifetime)
 *    - Low churn (rarely added/removed)
 *    - Use gc_persistent_root<T*> for heap-allocated roots
 *
 * 3. Thread-Local Roots (thread_local variables):
 *    - Per-thread lifetime
 *    - Medium churn (thread creation/destruction)
 *    - Each thread's GC context manages its own roots
 *
 * Slot-Based API:
 * - add_root() takes **slot** (address of pointer), not value
 * - Allows moving GC to update the pointer after relocation
 * - Critical for copying/generational GC correctness
 *
 * Example (correct - slot-based):
 *   Node* my_node = gc->allocate<Node>();
 *   gc->add_root(reinterpret_cast<void**>(&my_node));  // Pass address!
 *
 *   gc->collect();  // Moving GC may relocate my_node
 *
 *   // my_node now points to new location (updated by GC)
 *   my_node->value = 42;  // Safe!
 *
 * Example (WRONG - value-based):
 *   Node* my_node = gc->allocate<Node>();
 *   gc->add_root_value(my_node);  // Stores pointer value
 *
 *   gc->collect();  // Moving GC relocates my_node
 *
 *   // my_node still points to OLD location (stale!)
 *   my_node->value = 42;  // CRASH or corruption!
 *
 * Thread Safety:
 * - add_root/remove_root must be thread-safe if GC is shared
 * - Thread-local GC: Each thread has independent root set (no locking)
 * - Shared GC: Root set must use concurrent data structure
 */
class IRootManager {
public:
    virtual ~IRootManager() = default;

    /**
     * @brief Register a root (slot-based for moving GC)
     * @param root_slot Address of pointer to protect (&ptr, not ptr!)
     *
     * Registers a pointer slot as a GC root. The GC will:
     * 1. Trace from *root_slot during collections
     * 2. Update *root_slot if object moves (copying/generational GC)
     *
     * CRITICAL: Pass ADDRESS of pointer (&my_ptr), not value (my_ptr)!
     *
     * Lifetime:
     * - Root must remain valid until remove_root() called
     * - For stack variables, use RAII (gc_local, gc_root)
     * - For globals, register once at startup
     *
     * Example (stack root with RAII):
     *   {
     *       Node* node = gc->allocate<Node>();
     *       gc_local root{node};  // Calls add_root(&node)
     *
     *       // ... use node ...
     *
     *   }  // gc_local destructor calls remove_root(&node)
     *
     * Example (global root):
     *   Node* global_node = nullptr;
     *
     *   void init() {
     *       global_node = gc->allocate<Node>();
     *       gc->add_root(reinterpret_cast<void**>(&global_node));
     *   }
     *
     *   void shutdown() {
     *       gc->remove_root(reinterpret_cast<void**>(&global_node));
     *   }
     *
     * nullptr Handling:
     * - root_slot == nullptr: Undefined behavior (invalid slot)
     * - *root_slot == nullptr: OK (root points to nothing, no tracing)
     *
     * Thread Safety: Implementation-defined
     */
    virtual void add_root(void** root_slot) = 0;

    /**
     * @brief Unregister a root
     * @param root_slot Address of pointer previously passed to add_root()
     *
     * Removes a root from the GC's root set. After this call:
     * - GC no longer traces from this slot
     * - Object pointed to may be collected (if no other roots)
     * - Slot can be deallocated (e.g., stack frame returns)
     *
     * MUST pass the SAME address as add_root():
     *   gc->add_root(&my_ptr);
     *   gc->remove_root(&my_ptr);  // Same address!
     *
     * Mismatched add/remove is undefined behavior:
     *   gc->add_root(&ptr1);
     *   gc->remove_root(&ptr2);  // WRONG! ptr1 stays rooted forever
     *
     * Multiple Roots to Same Object:
     * - OK to root same object multiple times from different slots:
     *     gc->add_root(&ptr1);  // Both point to same Node
     *     gc->add_root(&ptr2);
     *     gc->remove_root(&ptr1);  // Node still rooted via ptr2
     *     gc->remove_root(&ptr2);  // Now unreachable
     *
     * Thread Safety: Implementation-defined
     */
    virtual void remove_root(void** root_slot) = 0;

    /**
     * @brief Get number of registered roots (for debugging)
     * @return Number of active roots
     *
     * Optional: Default implementation returns 0 (unknown)
     */
    virtual size_t root_count() const {
        return 0;  // Unknown by default
    }
};

} // namespace memory
