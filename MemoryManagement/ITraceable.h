#pragma once

#include "ITracer.h"

namespace memory {

/**
 * @file ITraceable.h
 * @brief Optional interface for objects participating in GC tracing
 *
 * This is an OPTIONAL interface for objects that contain pointers to other
 * GC-managed objects. Objects that don't reference other objects (e.g., strings,
 * numbers, POD data) do not need to implement this.
 *
 * Example:
 *   // Traceable object (has pointers)
 *   struct Node : ITraceable {
 *       Node* left;
 *       Node* right;
 *
 *       void trace_references(ITracer& t) override {
 *           t.visit_field(reinterpret_cast<void**>(&left));
 *           t.visit_field(reinterpret_cast<void**>(&right));
 *       }
 *   };
 *
 *   // Non-traceable object (no pointers)
 *   struct Number {  // No ITraceable needed!
 *       double value;
 *   };
 *
 * Capability Detection:
 *   template<typename T>
 *   concept Traceable = std::is_base_of_v<ITraceable, T>;
 */

/**
 * @brief Interface for objects that participate in GC tracing
 *
 * Objects that contain pointers to other GC-managed objects must implement
 * this interface. The trace_references() method is called during garbage
 * collection to discover reachable objects.
 *
 * Design Pattern:
 * 1. Derive from ITraceable
 * 2. Implement trace_references()
 * 3. Call tracer.visit() or tracer.visit_field() for each pointer field
 * 4. Don't trace non-pointer fields (ints, floats, etc.)
 *
 * Moving vs Non-Moving GC:
 * - Non-moving (mark-sweep): Use tracer.visit(ptr)
 * - Moving (copying, generational): Use tracer.visit_field(&ptr)
 *
 * Best Practice: Always use visit_field() for future-proofing.
 * If GC upgrades from mark-sweep to generational, visit_field() keeps working.
 *
 * Thread Safety:
 * - trace_references() is called during GC (stop-the-world or incremental)
 * - Must not allocate new objects during tracing
 * - Must not modify object graph structure during tracing
 * - Read-only traversal of object graph
 */
class ITraceable {
public:
    virtual ~ITraceable() = default;

    /**
     * @brief Trace references to other objects
     * @param tracer Tracer to report child object pointers
     *
     * Called by GC during mark/trace phase. Object must report ALL pointers
     * to other GC-managed objects.
     *
     * Rules:
     * 1. Trace ALL pointer fields (missing one = leak or dangling pointer)
     * 2. Don't trace non-pointer fields (ints, floats, etc.)
     * 3. Don't allocate during tracing (GC is in progress)
     * 4. Don't modify object graph during tracing (read-only)
     *
     * Example (slot-based, recommended):
     *   struct BinaryTree : ITraceable {
     *       BinaryTree* left;
     *       BinaryTree* right;
     *       int value;  // Not a pointer, don't trace
     *
     *       void trace_references(ITracer& t) override {
     *           // Pass ADDRESS of pointer (&left, not left!)
     *           if (left) {
     *               t.visit_field(reinterpret_cast<void**>(&left));
     *           }
     *           if (right) {
     *               t.visit_field(reinterpret_cast<void**>(&right));
     *           }
     *           // Don't trace 'value' - it's not a pointer
     *       }
     *   };
     *
     * Example (value-based, for non-moving GC):
     *   void trace_references(ITracer& t) override {
     *       t.visit(left);   // Pass pointer value
     *       t.visit(right);  // GC won't update these
     *   }
     *
     * nullptr Handling:
     * - Safe to call visit/visit_field with nullptr
     * - Tracer treats as no-op
     * - Can skip null check if tracer is guaranteed to handle it
     *
     * Inheritance:
     *   struct Derived : Base, ITraceable {
     *       void trace_references(ITracer& t) override {
     *           // Trace base class fields
     *           if constexpr (std::is_base_of_v<ITraceable, Base>) {
     *               Base::trace_references(t);
     *           }
     *
     *           // Trace derived class fields
     *           t.visit_field(reinterpret_cast<void**>(&my_field));
     *       }
     *   };
     */
    virtual void trace_references(ITracer& tracer) = 0;
};

} // namespace memory
