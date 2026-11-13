#pragma once

#include "gc_traits.h"
#include <cstddef>
#include <vector>

/**
 * @brief Requirements for GC strategy implementations.
 *
 * Strategies implement specific garbage collection algorithms. The gc_manager
 * template delegates to the strategy for collection logic.
 *
 * Required interface:
 * - void on_allocate(gc_object* obj): Called when an Object is allocated.
 * - void collect(): Run garbage collection.
 * - void add_root(gc_object* obj): Register a GC root.
 * - void remove_root(gc_object* obj): Unregister a GC root.
 * - void shutdown(): Clean up strategy resources.
 * - size_t object_count() const: Return number of live objects.
 * - GCStats get_stats() const: Return collection statistics.
 */

/// @brief Statistics about garbage collection performance.
struct GCStats {
    size_t collections_run = 0;
    size_t objects_allocated = 0;
    size_t objects_collected = 0;
    size_t bytes_allocated = 0;
    size_t bytes_reclaimed = 0;
};

/**
 * @brief Optional Base class for strategies.
 *
 * Provides common infrastructure like statistics tracking. Strategies can
 * inherit from this or implement the concept directly.
 */
class GCStrategyBase {
public:
    virtual ~GCStrategyBase() = default;

    // Statistics
    GCStats get_stats() const { return m_stats; }

protected:
    GCStats m_stats;
};
