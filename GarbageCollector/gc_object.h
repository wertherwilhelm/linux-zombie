#pragma once

/**
 * @file gc_object.h
 * @brief Minimal GC Object interface - completely Scheme-independent
 *
 * This defines the minimal contract between the garbage collector and
 * managed objects. Any language (Scheme, Python, Ruby, etc.) can implement
 * this interface to participate in garbage collection.
 *
 * Design Philosophy:
 * - Minimal surface area (only what GC needs)
 * - No language-specific methods
 * - Virtual for polymorphism (small overhead, necessary for type-erasure)
 * - Forward-compatible (can add concurrent GC support later)
 */

// Forward declaration for GC tracing interface
class Tracer;

/**
 * @brief Base class for all garbage-collected objects
 *
 * This is the minimal interface required by the mark-and-sweep collector.
 * Language runtimes (Scheme, Python, etc.) derive their Object hierarchies
 * from this Base class.
 *
 * Required Implementations:
 * - trace_references(Tracer&) - Enumerate child pointers for marking
 *
 * Provided Implementations:
 * - Marked flag for mark-and-sweep algorithm
 * - Virtual destructor for proper cleanup
 *
 * Example (Scheme):
 *   class SchemeObject : public gc_object {
 *       // Scheme-specific: type system, eval, print, etc.
 *   };
 *
 * Example (Python):
 *   class PyObject : public gc_object {
 *       // Python-specific: PyTypeObject, tp_traverse, etc.
 *   };
 */
class gc_object {
public:
    gc_object() : m_marked(false) {}
    virtual ~gc_object() = default;

    // ========================================================================
    // Mark-and-Sweep Algorithm Interface
    // ========================================================================

    /**
     * @brief Set the marked flag (used during GC mark phase)
     * @param marked True if reachable, false if potentially unreachable
     *
     * Called by the GC during the mark phase. Objects are marked as
     * reachable if there's a path from a GC root. Unmarked objects
     * are collected during the sweep phase.
     */
        void set_marked(bool marked) { m_marked = marked; }

    /**
     * @brief Check if Object is marked as reachable
     * @return True if marked (reachable), false otherwise
     *
     * Called by the GC to avoid re-marking (handles cycles).
     * Also called during sweep to identify collectible objects.
     */
    bool is_marked() const { return m_marked; }

    /**
     * @brief Enumerate all child Object pointers for GC traversal
     * @param tracer The GC tracer that receives child references
     *
     * Pure virtual - must be implemented by derived classes.
     * Each implementation should call tracer.visit_field() on every gc_object*
     * field it contains. The tracer handles the actual marking logic.
     *
     * IMPORTANT: Use visit_field() instead of visit() for moving GC support.
     * visit_field() receives the field LOCATION (via reference) and can update
     * it after copying objects. visit() only receives the VALUE and cannot update.
     *
     * Example (Scheme cons cell):
     *   void trace_references(Tracer& tracer) override {
     *       tracer.visit_field(_car);  // Field location passed, can be updated
     *       tracer.visit_field(_cdr);
     *   }
     *
     * Example (Python tuple):
     *   void trace_references(Tracer& tracer) override {
     *       for (PyObject*& item : _items) {  // NOTE: reference!
     *           tracer.visit_field(item);
     *       }
     *   }
     *
     * Thread-safety: Called during single-threaded GC pause
     */
    virtual void trace_references(Tracer& tracer) = 0;

private:
    bool m_marked;  ///< Mark flag for mark-and-sweep algorithm
};
