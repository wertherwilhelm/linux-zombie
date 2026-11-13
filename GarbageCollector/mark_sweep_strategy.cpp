#include "pch.h"
#include "mark_sweep_strategy.h"
#include "gc_persistent_root.h"
#include "gc_box.h"
#include "gc_object.h"
#include <iostream>
#include <chrono>

MarkSweepStrategy::MarkSweepStrategy()
    : m_bytes_allocated(0)
    , m_collecting(false) {
}

MarkSweepStrategy::~MarkSweepStrategy() {
    shutdown();
}

void MarkSweepStrategy::on_allocate(gc_object* obj) {
    if (!obj) return;

    // Track actual object size from gc_box header
    size_t obj_size = gc_box::get_size(obj);
    m_bytes_allocated += obj_size;
    m_stats.objects_allocated++;

    if (m_collecting) {
        // During collection, queue for immediate tracing
        // (Supports on-demand allocation during trace_references, e.g., LOD nodes)
        m_pending_allocations.push_back(obj);

        // Add to marking worklist if currently marking
        // This ensures the new Object is traced immediately
        if (m_marking_worklist) {
            m_marking_worklist->push_back(obj);
        }
    } else {
        m_all_objects.push_back(obj);
    }
}

void MarkSweepStrategy::add_root(gc_object* obj) {
    if (obj) {
        // Increment refcount (insert with 1 if not present, increment if present)
        auto it = m_gc_roots.find(obj);
        if (it != m_gc_roots.end()) {
            it->second++;  // Increment existing refcount
        } else {
            m_gc_roots[obj] = 1;  // First reference
        }
    }
}

void MarkSweepStrategy::remove_root(gc_object* obj) {
    if (obj) {
        // Decrement refcount, remove if reaches zero
        auto it = m_gc_roots.find(obj);
        if (it != m_gc_roots.end()) {
            it->second--;  // Decrement refcount
            if (it->second == 0) {
                m_gc_roots.erase(it);  // Remove when refcount reaches zero
            }
        }
    }
}

void MarkSweepStrategy::collect() {
    // Prevent recursive collection
    if (m_collecting) {
        return;
    }

    m_collecting = true;

    size_t objects_before = m_all_objects.size();
    size_t bytes_before = m_bytes_allocated;

    // Run mark and sweep
    mark();
    sweep();

    // Integrate objects allocated during collection
    // (These were already traced during mark phase via m_marking_worklist)
    if (!m_pending_allocations.empty()) {
        m_all_objects.insert(m_all_objects.end(),
                           m_pending_allocations.begin(),
                           m_pending_allocations.end());
        m_pending_allocations.clear();
    }

    // Update statistics
    m_stats.collections_run++;
    m_stats.objects_collected += (objects_before - m_all_objects.size());
    m_stats.bytes_reclaimed += (bytes_before - m_bytes_allocated);

#ifdef GC_DEBUG
    std::cout << "GC: Collected " << (objects_before - m_all_objects.size())
              << " objects, freed " << (bytes_before - m_bytes_allocated)
              << " bytes" << std::endl;
    std::cout << "GC: " << m_all_objects.size() << " objects remaining, "
              << m_bytes_allocated << " bytes allocated" << std::endl;
#endif

    m_collecting = false;
}

void MarkSweepStrategy::mark() {
    // Clear all marks AND build lookup set for root validation
    std::unordered_set<gc_object*> valid_objects;
    for (gc_object* obj : m_all_objects) {
        obj->set_marked(false);
        valid_objects.insert(obj);
    }

    // Mark from all stack roots (registered via add_root)
    for (const auto& pair : m_gc_roots) {
        // Validate root is a known allocated object
        if (valid_objects.find(pair.first) != valid_objects.end()) {
            mark_from(pair.first);
        }
    }

    // Mark from all persistent heap roots registered with this GC instance
    for (gc_object** root_ptr : m_persistent_roots) {
        if (!root_ptr) continue;

        gc_object* obj = *root_ptr;
        if (!obj) continue;

        // ROBUST SAFETY CHECK: Only mark if object is in our tracked allocations
        // This catches ALL forms of corruption (freed memory, stale pointers, etc.)
        if (valid_objects.find(obj) == valid_objects.end()) {
            continue;  // Skip - not a valid tracked object
        }

        mark_from(obj);
    }
}

void MarkSweepStrategy::mark_from(gc_object* obj) {
    if (!obj || obj->is_marked()) {
        return;
    }

    // ITERATIVE MARKING: Use explicit worklist to avoid C++ stack overflow
    std::vector<gc_object*> worklist;
    worklist.push_back(obj);

    auto* previous_worklist = m_marking_worklist;
    m_marking_worklist = &worklist;

    MarkingTracer tracer(&worklist);

    while (!worklist.empty()) {
        gc_object* current = worklist.back();
        worklist.pop_back();

        if (!current || current->is_marked())
            continue;

        current->set_marked(true);
        current->trace_references(tracer);
    }

    m_marking_worklist = previous_worklist;
}

void MarkSweepStrategy::mark_object(gc_object* obj) {
    // If we're in iterative marking mode, add to worklist instead of recursing
    if (m_marking_worklist) {
        if (obj && !obj->is_marked())
            m_marking_worklist->push_back(obj);
    } else {
        // Fallback: Direct marking (for backward compatibility)
        mark_from(obj);
    }
}

void MarkSweepStrategy::sweep() {
    // Three-phase sweep to avoid iterator invalidation:
    // Phase 1: Identify objects to delete (read-only pass)
    // Phase 2: Delete the objects
    // Phase 3: Build new vector without deleted objects

    // Phase 1: Collect unmarked objects and track bytes freed (accurate!)
    std::unordered_set<gc_object*> to_delete;
    size_t bytes_freed = 0;

    for (gc_object* obj : m_all_objects) {
        if (obj && !obj->is_marked()) {
            to_delete.insert(obj);
            // Track actual bytes being freed from gc_box header
            bytes_freed += gc_box::get_size(obj);
        }
    }

    // Phase 2: Reclaim the objects via manager (uses pluggable allocator)
    for (gc_object* obj : to_delete) {
        if (m_gc_manager) {
            m_gc_manager->reclaim(obj);
        }
    }

    // Phase 3: Remove deleted objects from m_all_objects
    // Using unordered_set lookup makes this O(N) instead of O(N*M)
    auto new_end = std::remove_if(m_all_objects.begin(), m_all_objects.end(),
        [&to_delete](gc_object* obj) {
            return to_delete.find(obj) != to_delete.end();
        });
    m_all_objects.erase(new_end, m_all_objects.end());

    // Update bytes allocated (accurate - subtract actual freed bytes!)
    m_bytes_allocated -= bytes_freed;
}

void MarkSweepStrategy::shutdown() {
    // Clean up all remaining objects via manager (uses pluggable allocator)
    for (gc_object* obj : m_all_objects) {
        if (m_gc_manager) {
            m_gc_manager->reclaim(obj);
        }
    }
    m_all_objects.clear();
    m_gc_roots.clear();

    // Reset accounting (all objects deallocated)
    m_bytes_allocated = 0;
}

// ============================================================================
// MarkingTracer implementation
// ============================================================================

void MarkingTracer::visit(void* ptr) {
    if (!ptr) return;

    // Cast to gc_object* (all GC-managed types must derive from gc_object)
    gc_object* obj = static_cast<gc_object*>(ptr);

    // Add to worklist if not yet marked
    // IMPORTANT: Do NOT mark here! The main loop marks objects when processing them.
    // Marking here would cause the loop to skip calling trace_references on children.
    if (!obj->is_marked()) {
        if (m_worklist) {
            m_worklist->push_back(obj);  // Enqueue for marking and tracing
        }
    }
}

// ============================================================================
// Persistent Root Management (Per-GC Instance)
// ============================================================================

void MarkSweepStrategy::register_persistent_root(gc_object** root_ptr) {
    if (root_ptr) {
        m_persistent_roots.push_back(root_ptr);
    }
}

void MarkSweepStrategy::unregister_persistent_root(gc_object** root_ptr) {
    if (root_ptr) {
        auto it = std::find(m_persistent_roots.begin(), m_persistent_roots.end(), root_ptr);
        if (it != m_persistent_roots.end()) {
            m_persistent_roots.erase(it);
        }
    }
}
