#pragma once

#include "gc_strategy.h"
#include "gc_interface.h"
#include "gc_tracer.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

class gc_object;

/**
 * @brief Classic mark-and-sweep garbage collection strategy.
 *
 * Implements a stop-the-world mark-and-sweep collector with iterative marking
 * (avoids stack overflow), conservative root scanning, and a three-phase sweep
 * to avoid iterator invalidation.
 *
 * Algorithm:
 * 1. Mark phase traces from roots, marking all reachable objects.
 * 2. Sweep phase deletes unmarked objects and reclaims memory.
 */
class MarkSweepStrategy : public GCStrategyBase {
public:
    MarkSweepStrategy();
    ~MarkSweepStrategy();

    /**
     * @brief Set the owning GC (for backward compatibility with trace_references).
     * @param mgr Pointer to the GC that owns this strategy.
     */
    void set_gc_manager(IGarbageCollector* mgr) { m_gc_manager = mgr; }

    // GCStrategyConcept interface

    /**
     * @brief Register a newly allocated Object.
     * @param obj Object to track.
     */
    void on_allocate(gc_object* obj);

    /**
     * @brief Run a garbage collection cycle.
     */
    void collect();

    /**
     * @brief Add a GC root (Object that should never be collected).
     * @param obj Root Object pointer.
     */
    void add_root(gc_object* obj);

    /**
     * @brief Remove a GC root.
     * @param obj Root Object pointer.
     */
    void remove_root(gc_object* obj);

    /**
     * @brief Shutdown strategy and cleanup resources.
     */
    void shutdown();

    /**
     * @brief Get number of live objects.
     * @return Count of tracked allocations.
     */
    size_t object_count() const { return m_all_objects.size(); }

    /**
     * @brief Get total bytes allocated.
     * @return Approximate bytes currently allocated.
     */
    size_t bytes_allocated() const { return m_bytes_allocated; }

    /**
     * @brief Mark an Object as reachable (called during mark phase).
     * @param obj Object to mark.
     */
    void mark_object(gc_object* obj);

private:
    /**
     * @brief Mark phase: mark all reachable objects starting from roots.
     */
    void mark();

    /**
     * @brief Sweep phase: delete all unmarked objects.
     */
    void sweep();

    /**
     * @brief Iteratively mark an Object and all objects it references.
     * @param obj Object to begin marking from.
     */
    void mark_from(gc_object* obj);

    /// All allocated objects.
    std::vector<gc_object*> m_all_objects;
    /// GC roots with refcounts (registered via add_root).
    std::unordered_map<gc_object*, size_t> m_gc_roots;

    /// Objects allocated during collection (traced immediately).
    std::vector<gc_object*> m_pending_allocations;

    /// Persistent roots registered with this GC instance.
    std::vector<gc_object**> m_persistent_roots;

    /// Current bytes allocated (approximate).
    size_t m_bytes_allocated;
    /// Flag to prevent recursive collection.
    bool m_collecting;

    /// Back-reference to owning GC (for trace_references compatibility).
    IGarbageCollector* m_gc_manager = nullptr;

    /// Iterative marking support: worklist for mark phase.
    std::vector<gc_object*>* m_marking_worklist = nullptr;

public:
    // IGarbageCollector persistent root interface
    void register_persistent_root(gc_object** root_ptr);
    void unregister_persistent_root(gc_object** root_ptr);

    // Check if collection is currently in progress
    bool is_collecting() const { return m_collecting; }
};

/**
 * @brief Tracer implementation for mark-and-sweep GC.
 *
 * Implements the Tracer interface for mark-and-sweep collection. When an
 * Object calls visit(), this tracer queues the Object so the collector can
 * mark it and process its children iteratively. This keeps objects decoupled
 * from GC implementation details.
 */
class MarkingTracer : public Tracer {
public:
    /**
     * @brief Construct a marking tracer.
     * @param worklist Worklist to add unmarked objects to.
     */
    explicit MarkingTracer(std::vector<gc_object*>* worklist)
        : m_worklist(worklist) {}

    /**
     * @brief Visit a pointer and enqueue it if it is not yet marked.
     * @param ptr Candidate Object pointer (can be null).
     */
    void visit(void* ptr) override;

private:
    /// Worklist for iterative marking.
    std::vector<gc_object*>* m_worklist;
};
