#include "pch.h"
#include "gc_manager.h"
#include "gc_context.h"
#include "mark_sweep_strategy.h"
#include "gc_allocator.h"
#include "gc_object.h"

// Global GC instance (implementation only, not exported in headers)
gc_manager* gc = nullptr;

// ============================================================================
// IGarbageCollector implementation
// ============================================================================

IGarbageCollector::IGarbageCollector(IAllocator* alloc)
    : m_allocator(alloc ? alloc : &gc_box::default_allocator) {
    // Initialize with provided allocator or default
}

// ============================================================================
// gc_manager implementation
// ============================================================================

gc_manager::gc_manager()
    : IGarbageCollector(&gc_box::default_allocator)  // Pass default allocator to Base
    , m_strategy(new MarkSweepStrategy())
    , m_gc_threshold(64 * 1024 * 1024) {  // Increased to 64MB for test stability
    // Set back-reference so strategy can call our mark_object()
    m_strategy->set_gc_manager(this);
}

void gc_manager::set_scan_stack(bool on) {
    m_strategy->set_scan_stack(on);
}

gc_manager::~gc_manager() {
    delete m_strategy;
}

// ========================================================================
// New slot-based root API (implements memory::IRootManager)
// ========================================================================

void gc_manager::add_root(void** root_slot) {
    // Delegate to strategy's persistent root API (slot-based)
    m_strategy->register_persistent_root(reinterpret_cast<gc_object**>(root_slot));
}

void gc_manager::remove_root(void** root_slot) {
    // Delegate to strategy's persistent root API (slot-based)
    m_strategy->unregister_persistent_root(reinterpret_cast<gc_object**>(root_slot));
}

void gc_manager::collect() {
    m_strategy->collect();
}

void gc_manager::mark_object(gc_object* obj) {
    m_strategy->mark_object(obj);
}

void gc_manager::reclaim(gc_object* obj) {
    gc_box::deallocate(obj);
}

size_t gc_manager::object_count() const {
    return m_strategy->object_count();
}

size_t gc_manager::bytes_allocated() const {
    return m_strategy->bytes_allocated();
}

gc_manager::gc_stats gc_manager::get_stats() const {
    GCStats strategy_stats = m_strategy->get_stats();

    // Convert strategy stats to legacy gc_stats format
    gc_stats stats;
    stats.collections_run = strategy_stats.collections_run;
    stats.objects_collected = strategy_stats.objects_collected;
    stats.bytes_freed = strategy_stats.bytes_reclaimed;  // Map bytes_reclaimed to bytes_freed
    return stats;
}

// ============================================================================
// IGarbageCollector protected interface implementation
// ============================================================================

void* gc_manager::allocate_impl(size_t size, size_t align) {
    // Delegate to the pluggable allocator
    return m_allocator->allocate(size, align);
}

void gc_manager::on_allocate(gc_object* obj) {
    m_strategy->on_allocate(obj);

    // DON'T trigger GC if we're already collecting (prevents nested collection issues)
    if (m_strategy->is_collecting()) {
        return;
    }

    // Trigger GC if we've exceeded the threshold
    if (m_strategy->bytes_allocated() > m_gc_threshold) {
        // P1: `obj` has just been allocated but is not yet reachable from any root
        // (the caller hasn't received it). Root its slot across the collection so it
        // isn't swept and returned as a dangling pointer. &obj is a stable stack slot
        // for the duration of this call. Only paid when a collection actually fires.
        add_root(reinterpret_cast<void**>(&obj));
        collect();
        remove_root(reinterpret_cast<void**>(&obj));
    }
}

void initialize_gc() {
    if (!gc) {
        gc = new gc_manager();
        // This is the process-wide runtime GC. The reader/evaluator hold live Object*
        // in unrooted C++ locals, so it must conservatively scan the machine stack during
        // mark to avoid reclaiming them mid-operation. Isolated gc_manager / gc_manager_t
        // instances (tests, embedding with explicit roots) stay PRECISE by default.
        gc->set_scan_stack(true);
        // Set as current thread-local GC
        gc_context::set_current(gc);
    }
}

void shutdown_gc() {
    if (gc) {
        // Clear thread-local context
        gc_context::set_current(nullptr);
        delete gc;
        gc = nullptr;
    }
}
