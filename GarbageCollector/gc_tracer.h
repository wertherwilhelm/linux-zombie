#pragma once

/**
 * @file gc_tracer.h
 * @brief Tracer interface for GC graph traversal.
 */

// ============================================================================
// GC Tracer Interface
// ============================================================================
//
// This file defines the abstract interface between garbage-collected objects
// and the garbage collector itself. It enables complete decoupling:
//
// - The GC owns the Tracer implementation and decides what "visiting" means
// - Objects just enumerate their children via visit() calls
// - No dependency on gc_manager or concrete GC implementation
// - Non-Scheme C++ objects can participate by implementing trace_references()
//
// Design Benefits:
// - GC becomes a reusable C++ component, not Scheme-specific
// - Objects don't know about marking, sweeping, or collection strategy
// - Easy to add new GC algorithms (copying, generational, concurrent)
// - Other C++ types can be GC'd without inheriting from 'Object'
//
// Example Usage:
//
//   class MyObject {
//       MyObject* child_;
//   public:
//       void trace_references(Tracer& tracer) {
//           tracer.visit_field(child_);  // Use visit_field for moving GC support
//       }
//   };
//
// ============================================================================

// Forward declaration for type-safe callbacks
class Tracer;

// ============================================================================
// Tracer - Abstract visitor interface for GC graph traversal
// ============================================================================
//
// The GC provides a Tracer implementation during collection. Objects call
// visit() to enumerate reachable pointers. The Tracer decides what to do
// (mark, copy, count, etc.) based on the GC algorithm.
//
/**
 * @brief Abstract visitor interface for GC graph traversal.
 *
 * The GC provides a Tracer implementation during collection. Objects call
 * visit() or visit_field() to enumerate reachable pointers. The Tracer decides
 * what to do (mark, copy, count, etc.) based on the GC algorithm.
 */
class Tracer {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~Tracer() = default;

    // ========================================================================
    // visit - Mark a pointer as reachable (for mark-and-sweep)
    // ========================================================================
    //
    // Called by objects during trace_references() to enumerate children.
    // The Tracer implementation decides what to do with the pointer based
    // on the GC algorithm (mark-and-sweep marks it, copying GC copies it, etc.)
    //
    // Args:
    //   ptr - Pointer to GC-managed Object (can be nullptr)
    //
    // Thread-safety: Called during single-threaded GC pause
    //
    // Note: For moving collectors, use visit_slot() instead, which can update
    //       the pointer location after copying.
    //
    /**
     * @brief Visit a raw pointer for marking/copying.
     * @param ptr Pointer to GC-managed Object (can be nullptr).
     */
    virtual void visit(void* ptr) = 0;

    // ========================================================================
    // visit_slot - Visit a pointer slot (for moving/copying GC)
    // ========================================================================
    //
    // Called by objects during trace_references() to enumerate children.
    // Unlike visit(void*), this receives a POINTER TO THE POINTER, allowing
    // moving collectors to update the source location after copying.
    //
    // Args:
    //   ptr_location - Address of the pointer slot (e.g., &obj->child_)
    //
    // Default implementation: Reads pointer value and calls visit() for
    // backward compatibility with mark-and-sweep collectors.
    //
    // Moving collectors: Override this to copy Object and update *ptr_location
    //
    // Example (moving collector):
    //   void visit_slot(void** ptr_location) override {
    //       gc_object** slot = reinterpret_cast<gc_object**>(ptr_location);
    //       gc_object* obj = *slot;
    //       if (obj && needs_copy(obj)) {
    //           *slot = copy_object(obj);  // Update source pointer!
    //       }
    //   }
    //
    /**
     * @brief Visit a pointer slot (for moving/copying GC).
     * @param ptr_location Address of the pointer slot (e.g., &obj->child_).
     */
    virtual void visit_slot(void** ptr_location) {
        // Default: Forward to visit() for backward compatibility
        if (ptr_location && *ptr_location) {
            visit(*ptr_location);
        }
    }

    // ========================================================================
    // visit_field<T> - Type-safe field visitor (for moving GC)
    // ========================================================================
    //
    // Template helper for visiting Object fields with type safety.
    // Automatically takes address of field and calls visit_slot().
    //
    // Usage:
    //   void trace_references(Tracer& tracer) override {
    //       tracer.visit_field(car_);   // Instead of tracer.visit(car_)
    //       tracer.visit_field(cdr_);
    //   }
    //
    // Benefits:
    // - Works with both mark-sweep (via visit_slot default impl)
    // - Works with moving GC (via visit_slot override)
    // - Type-safe (preserves T* type)
    // - Handles nullptr automatically
    //
    /**
     * @brief Type-safe field visitor (supports moving GC).
     * @tparam T GC-managed type.
     * @param field Reference to pointer field.
     */
    template<typename T>
    void visit_field(T*& field) {
        visit_slot(reinterpret_cast<void**>(&field));
    }

    // ========================================================================
    // visit<T> - Type-safe convenience wrapper (for mark-and-sweep)
    // ========================================================================
    //
    // Template wrapper that preserves type information and handles nullptr.
    // Most code should use this instead of raw visit(void*).
    //
    // Note: For moving collectors, use visit_field<T>() instead.
    //
    /**
     * @brief Type-safe pointer visitor (mark-and-sweep convenience).
     * @tparam T GC-managed type.
     * @param obj Pointer to visit.
     */
    template<typename T>
    void visit(T* obj) {
        if (obj) {
            visit(static_cast<void*>(obj));
        }
    }
};
