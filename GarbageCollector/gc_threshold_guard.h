#pragma once

/**
 * @file gc_threshold_guard.h
 * @brief RAII guard to temporarily raise GC threshold during critical sections
 *
 * When performing operations that allocate heavily (evaluation, compilation),
 * frequent GC can cause performance issues or even correctness problems if
 * objects are collected during a multi-step allocation sequence.
 *
 * GcThresholdGuard temporarily raises the GC threshold to prevent collection
 * during the protected scope, then restores it automatically.
 *
 * Usage:
 *   void eval_large_expression(Object* expr, gc_manager* gc) {
 *       // Prevent GC during this critical section
 *       GcThresholdGuard guard(gc, 256 * 1024 * 1024);  // 256MB
 *
 *       // ... heavy allocation during evaluation ...
 *
 *   }  // Threshold restored automatically
 *
 * Thread Safety:
 *   Each thread should have its own gc_manager or use gc_context::current().
 *   The guard is NOT thread-safe across gc_managers.
 */

#include "gc_manager.h"
#include <cstddef>

/**
 * @brief RAII guard that temporarily raises GC threshold
 *
 * The guard ensures that:
 * 1. Threshold is raised to at least current_allocated + min_threshold_bytes
 * 2. Threshold is never lowered below the previous setting
 * 3. Previous threshold is restored on scope exit
 */
class GcThresholdGuard {
public:
    /**
     * @brief Construct guard and raise GC threshold
     *
     * @param gc Garbage collector manager (can be nullptr for no-op)
     * @param min_threshold_bytes Minimum headroom to add above current allocation
     *
     * If gc is nullptr, the guard does nothing (safe for optional GC scenarios).
     */
    explicit GcThresholdGuard(gc_manager* gc, size_t min_threshold_bytes = 256 * 1024 * 1024)
        : m_gc(gc)
        , m_previous(0)
        , m_active(false)
    {
        if (!m_gc) return;

        m_previous = m_gc->gc_threshold();
        size_t current = m_gc->bytes_allocated();

        // Calculate desired threshold:
        // - At least min_threshold_bytes above current
        // - Never lower than previous setting
        size_t desired = current + min_threshold_bytes;
        if (m_previous > desired) {
            desired = m_previous;
        }

        m_gc->set_gc_threshold(desired);
        m_active = true;
    }

    /**
     * @brief Destructor restores previous threshold
     */
    ~GcThresholdGuard() {
        if (m_active && m_gc) {
            m_gc->set_gc_threshold(m_previous);
        }
    }

    // Non-copyable, non-movable (RAII semantics)
    GcThresholdGuard(const GcThresholdGuard&) = delete;
    GcThresholdGuard& operator=(const GcThresholdGuard&) = delete;
    GcThresholdGuard(GcThresholdGuard&&) = delete;
    GcThresholdGuard& operator=(GcThresholdGuard&&) = delete;

    /**
     * @brief Check if guard is active (has a valid gc_manager)
     */
    bool is_active() const { return m_active; }

    /**
     * @brief Get the previous threshold (for debugging)
     */
    size_t previous_threshold() const { return m_previous; }

private:
    gc_manager* m_gc;
    size_t m_previous;
    bool m_active;
};

/**
 * @brief Default GC guard headroom for evaluation operations
 *
 * 256MB is a reasonable default that allows substantial allocation
 * without triggering GC, while not exhausting system memory.
 */
constexpr size_t kDefaultGcGuardBytes = 256 * 1024 * 1024;

/**
 * @brief Large GC guard headroom for heavy operations (compilation, large evals)
 *
 * 512MB for operations known to allocate heavily.
 */
constexpr size_t kLargeGcGuardBytes = 512 * 1024 * 1024;
