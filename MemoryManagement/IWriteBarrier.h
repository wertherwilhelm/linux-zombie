#pragma once

#include <cstddef>

namespace memory {

/**
 * @file IWriteBarrier.h
 * @brief Optional write barrier interface for generational GC
 *
 * This is an OPTIONAL capability for memory managers that use generational
 * or incremental garbage collection. Write barriers track pointer modifications
 * to maintain GC correctness.
 *
 * Why Write Barriers?
 * - Generational GC only scans young generation (minor GC)
 * - If old-gen object points to young-gen object, young object is reachable
 * - Without barrier: Minor GC doesn't scan old-gen → young object collected!
 * - With barrier: Old→young edges recorded → young object survives
 *
 * When Needed:
 * - Generational GC (young/old generations)
 * - Incremental GC (concurrent marking)
 * - Real-time GC (bounded pause times)
 *
 * When NOT Needed:
 * - Mark-sweep GC (scans entire heap)
 * - Reference counting (no tracing)
 * - Manual memory management
 *
 * Capability Detection:
 *   template<typename T>
 *   concept SupportsWriteBarrier = requires(T& mgr, void* owner, void* val) {
 *       { mgr.write_barrier(owner, nullptr, val) } -> std::same_as<void>;
 *   };
 */

/**
 * @brief Write barrier interface for tracking pointer modifications
 *
 * Generational GC divides heap into generations (young/old). Minor GC only
 * scans young generation (fast!), but must handle old→young pointers.
 *
 * Algorithm:
 * 1. Application modifies old_obj->field = young_obj
 * 2. Write barrier records old_obj in "remembered set"
 * 3. Minor GC scans: roots + remembered set → finds young_obj
 * 4. young_obj survives minor GC ✓
 *
 * Without barrier:
 * 1. Application modifies old_obj->field = young_obj
 * 2. No barrier fired (missed!)
 * 3. Minor GC scans: roots only → misses young_obj
 * 4. young_obj collected (dangling pointer!) ✗
 *
 * Performance:
 * - Cost: ~5-10 cycles per pointer write
 * - Benefit: 10-100x faster minor GC
 * - Net: Huge win for allocation-heavy workloads
 *
 * Thread Safety:
 * - Barriers must be thread-safe if GC is shared
 * - Typically uses lock-free remembered set (CAS operations)
 * - Thread-local GC avoids synchronization entirely
 */
class IWriteBarrier {
public:
    virtual ~IWriteBarrier() = default;

    /**
     * @brief Record a pointer write for GC correctness
     * @param owner Object being modified (contains the field)
     * @param old_value Previous value of field (may be nullptr)
     * @param new_value New value being written (may be nullptr)
     *
     * Called when a pointer field is updated to track cross-generation edges.
     *
     * Typical Implementation (Generational GC):
     *   void write_barrier(void* owner, void* old_value, void* new_value) {
     *       // Only care about old→young edges
     *       if (is_in_old_gen(owner) && is_in_young_gen(new_value)) {
     *           remembered_set.insert(owner);  // Track old-gen object
     *       }
     *   }
     *
     * During minor GC:
     *   // Scan roots (stack, globals)
     *   for (auto* root : roots) {
     *       mark_young(root);
     *   }
     *
     *   // Scan remembered set (old→young edges)
     *   for (auto* old_obj : remembered_set) {
     *       old_obj->trace_references(tracer);  // Finds young children
     *   }
     *
     * Usage (automatic with gc_field):
     *   struct Node {
     *       gc_field<Node> left;  // Fires barrier automatically!
     *
     *       Node() {
     *           left.bind_owner(this);  // Enable automatic barriers
     *       }
     *   };
     *
     *   // Assignment fires write_barrier(this, old_left, new_left)
     *   node->left = child;
     *
     * Usage (manual):
     *   void set_child(Node* child) {
     *       Node* old_child = child_;
     *       child_ = child;
     *
     *       // Manually fire barrier
     *       gc->write_barrier(this, old_child, child);
     *   }
     *
     * Parameters:
     * - owner: Object containing the field being modified
     * - old_value: Previous pointer value (for some barrier types)
     * - new_value: New pointer value being written
     *
     * nullptr Handling:
     * - Passing nullptr is safe (treated as no-op)
     * - owner == nullptr: No-op (field not in GC-managed object)
     * - new_value == nullptr: No-op (clearing pointer, no edge)
     * - old_value == nullptr: Normal (first write to field)
     *
     * Thread Safety:
     * - Implementation-defined
     * - Thread-local GC: No synchronization needed
     * - Shared GC: Must use lock-free remembered set
     *
     * Performance Notes:
     * - Modern CPUs: ~5-10 cycles overhead
     * - Branch prediction helps (most writes don't cross generations)
     * - Inline for best performance (virtual call overhead acceptable)
     */
    virtual void write_barrier(void* owner, void* old_value, void* new_value) = 0;
};

} // namespace memory
