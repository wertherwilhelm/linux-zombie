#pragma once

#include "gc_interface.h"

/**
 * @file gc_context.h
 * @brief Thread-local GC context management
 *
 * Provides access to the current thread's garbage collector without global variables.
 * This enables:
 * - Multiple GC instances per thread
 * - Temporary GC swapping (via gc_scope)
 * - Header files that don't depend on global gc
 * - Testing with isolated GC instances
 *
 * Usage:
 *   // Get current GC
 *   IGarbageCollector* gc = gc_context::current();
 *   auto obj = gc->allocate<ConsCell>(car, cdr);
 *
 *   // Temporarily use different GC
 *   gc_scope swap(isolated_gc);
 *   auto obj2 = gc_context::current()->allocate<atom<long>>(42);
 */

/**
 * @brief Thread-local garbage collector context
 *
 * Manages the current GC instance for each thread. Uses thread_local storage
 * to avoid global state and enable per-thread GC selection.
 *
 * Design Philosophy:
 * - Thread-local, not global (supports multi-threading)
 * - Interface-based (works with any IGarbageCollector implementation)
 * - Swap-friendly (gc_scope RAII for temporary changes)
 * - No header pollution (no extern declarations in headers)
 */
class gc_context {
public:
    /**
     * @brief Get the current thread's GC instance
     * @return Pointer to current GC, or nullptr if not initialized
     *
     * This is the primary accessor. Use instead of the global gc pointer.
     *
     * Example:
     *   IGarbageCollector* gc = gc_context::current();
     *   if (gc) {
     *       auto obj = gc->allocate<ConsCell>(car, cdr);
     *   }
     */
    static IGarbageCollector* current() {
        return m_current_gc;
    }

    /**
     * @brief Set the current thread's GC instance
     * @param gc New GC instance to use (can be nullptr)
     *
     * Warning: This changes the current GC for the entire thread.
     * Prefer using gc_scope for temporary changes.
     *
     * Example:
     *   gc_context::set_current(my_gc);  // Now current for this thread
     */
    static void set_current(IGarbageCollector* gc) {
        m_current_gc = gc;
    }

private:
    // Thread-local storage for current GC instance
    // Each thread has its own independent GC context
    static thread_local IGarbageCollector* m_current_gc;
};

/**
 * @brief RAII helper for temporarily swapping the current GC
 *
 * Automatically restores the previous GC when scope exits.
 * This is the preferred way to temporarily use a different GC.
 *
 * Use Cases:
 * - Isolated evaluation (sandboxing)
 * - Per-request GC in server applications
 * - Testing with mock GC
 * - Temporary GC swapping for specific operations
 *
 * Example:
 *   IGarbageCollector* main_gc = gc_context::current();
 *   IGarbageCollector* isolated = new gc_manager();
 *
 *   {
 *       gc_scope swap(isolated);
 *       // Now gc_context::current() == isolated
 *       auto obj = gc_context::current()->allocate<atom<long>>(42);
 *       // obj allocated in isolated GC
 *   } // Automatically restores main_gc
 *
 *   // Now gc_context::current() == main_gc again
 */
class gc_scope {
public:
    /**
     * @brief Swap to a new GC for this scope
     * @param new_gc The GC to use (can be nullptr)
     */
    explicit gc_scope(IGarbageCollector* new_gc)
        : m_previous_gc(gc_context::current()) {
        gc_context::set_current(new_gc);
    }

    /**
     * @brief Restore previous GC on scope exit
     */
    ~gc_scope() {
        gc_context::set_current(m_previous_gc);
    }

    // Non-copyable (RAII guard)
    gc_scope(const gc_scope&) = delete;
    gc_scope& operator=(const gc_scope&) = delete;

    // Non-movable (could invalidate stack unwinding)
    gc_scope(gc_scope&&) = delete;
    gc_scope& operator=(gc_scope&&) = delete;

private:
    IGarbageCollector* m_previous_gc;
};

/**
 * @brief Get the current GC (convenience function)
 *
 * Shorthand for gc_context::current(). Use in code that frequently
 * accesses the GC to reduce verbosity.
 *
 * Example:
 *   auto obj = get_current_gc()->allocate<ConsCell>(car, cdr);
 */
inline IGarbageCollector* get_current_gc() {
    return gc_context::current();
}
