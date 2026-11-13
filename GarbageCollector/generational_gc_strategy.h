#pragma once

#include "gc_strategy.h"
#include "gc_object.h"
#include "gc_allocator.h"
#include "gc_interface.h"
#include "gc_box.h"
#include "gc_traits.h"
#include <vector>
#include <cstdint>
#include <cstddef>

// Forward declarations
class IGarbageCollector;
class YoungGenerationAllocator;
class GenerationalGCStrategy;

/**
 * @file generational_gc_strategy.h
 * @brief Generational garbage collection strategy with young/old generations
 *
 * Architecture:
 * - Young generation (nursery): Fast bump-pointer allocation, copy collection
 * - Old generation: Mark-sweep collection (reuse existing strategy)
 * - Write barrier: Track old→young pointers in remembered set
 * - Promotion: Objects surviving N minor GCs move to old generation
 *
 * Performance characteristics:
 * - Minor GC (young only): O(live young objects) - typically 1-5ms
 * - Major GC (full heap): O(all live objects) - rare, run during loading
 *
 * Expected improvement for LOD scene graphs:
 * - 100K objects: 1.68ms → 0.5ms (3.4x faster)
 * - 1M objects: 81ms → 3ms (27x faster!)
 */

/**
 * @brief Young generation allocator using semi-space copying collection
 *
 * Uses Cheney's algorithm:
 * 1. Allocate in from-space using bump pointer (fast!)
 * 2. On collection, copy live objects to to-space
 * 3. Swap from/to spaces
 * 4. Objects surviving N collections are promoted to old generation
 *
 * Memory layout:
 *   [from-space] [to-space]
 *   ^next        ^(empty until collection)
 *
 * After collection:
 *   [to-space (now from)] [from-space (now to)]
 *   ^next (after copied)  ^(empty)
 */
class YoungGenerationAllocator {
public:
    /**
     * @brief Create young generation with specified size
     * @param size_bytes Size of each semi-space (total memory = 2x this)
     * @param promotion_threshold Survivals needed to promote to old gen
     */
    YoungGenerationAllocator(size_t size_bytes = 10 * 1024 * 1024,  // 10MB default
                              uint8_t promotion_threshold = 3);

    ~YoungGenerationAllocator();

    /**
     * @brief Allocate Object in young generation
     * @param size Object size in bytes
     * @param alignment Object alignment requirement
     * @return Pointer to allocated memory, or nullptr if nursery full
     *
     * Uses fast bump-pointer allocation:
     *   if (next + size <= end) {
     *       void* obj = next;
     *       next += size;
     *       return obj;
     *   }
     *   return nullptr;  // Triggers minor GC
     */
    void* allocate(size_t size, size_t alignment);

    /**
     * @brief Check if Object is in young generation
     * @param ptr Object pointer
     * @return true if in from-space or to-space
     */
    bool contains(void* ptr) const;

    /**
     * @brief Get age of Object (number of collections survived)
     * @param obj Object pointer (must be in young generation)
     * @return Age count (0 = just allocated, 1 = survived 1 GC, etc.)
     */
    uint8_t get_age(gc_object* obj) const;

    /**
     * @brief Set age of Object
     * @param obj Object pointer
     * @param age New age value
     */
    void set_age(gc_object* obj, uint8_t age);

    /**
     * @brief Check if Object is ready for promotion
     * @param obj Object pointer
     * @return true if age >= promotion_threshold
     */
    bool should_promote(gc_object* obj) const {
        return get_age(obj) >= m_promotion_threshold;
    }

    /**
     * @brief Get number of bytes allocated in young generation
     */
    size_t bytes_allocated() const {
        return m_next - m_from_space;
    }

    /**
     * @brief Get nursery capacity
     */
    size_t capacity() const {
        return m_space_size;
    }

    /**
     * @brief Reset allocator to empty state (for testing)
     */
    void reset();

    // Internal structure for copying collection (public for strategy access)
    struct ForwardingInfo {
        void* new_address;  // Where Object was copied to
        bool forwarded;     // Has this Object been copied?
    };

private:
    // Semi-space pointers
    char* m_from_space;      // Current allocation space
    char* m_to_space;        // Copy target during collection
    char* m_next;            // Bump pointer (next allocation address)
    char* m_end;             // End of from-space

    size_t m_space_size;     // Size of each semi-space
    uint8_t m_promotion_threshold;  // Survivals needed to promote

    // Age tracking (maps Object pointer → age)
    // Note: This is reset on each collection (ages are preserved during copy)
    std::vector<std::pair<gc_object*, uint8_t>> m_age_map;

    friend class GenerationalGCStrategy;
};

/**
 * @brief Generational GC strategy combining young and old generations
 *
 * Collection types:
 * - Minor GC: Collect young generation only (fast, frequent)
 * - Major GC: Collect entire heap (slow, rare)
 *
 * Allocation flow:
 * 1. Try allocate in young generation (bump pointer, O(1))
 * 2. If young gen full → minor GC
 * 3. If minor GC doesn't free enough → try old generation
 * 4. If old gen full → major GC
 * 5. If major GC doesn't free enough → allocation failure
 */
class GenerationalGCStrategy : public GCStrategyBase {
public:
    /**
     * @brief Create generational GC with specified generation sizes
     * @param young_size Young generation size (each semi-space)
     * @param old_allocator Allocator for old generation (default: malloc)
     */
    explicit GenerationalGCStrategy(
        size_t young_size = 10 * 1024 * 1024,  // 10MB young gen
        IAllocator* old_allocator = nullptr);   // nullptr = use default MallocAllocator

    ~GenerationalGCStrategy();

    // ========================================================================
    // GCStrategy Interface Implementation
    // ========================================================================

    void on_allocate(gc_object* obj);
    void collect();  // Performs minor GC by default
    void add_root(gc_object* root);
    void remove_root(gc_object* root);
    void mark_object(gc_object* obj);
    size_t object_count() const;
    size_t bytes_allocated() const;
    GCStats get_stats() const;

    void register_persistent_root(gc_object** root_ptr);
    void unregister_persistent_root(gc_object** root_ptr);

    void set_gc_manager(IGarbageCollector* gc) {
        m_gc_manager = gc;
    }

    // ========================================================================
    // Generational-Specific Operations
    // ========================================================================

    /**
     * @brief Perform minor GC (young generation only)
     *
     * Algorithm:
     * 1. Scan roots and remembered set
     * 2. Copy live young objects to to-space
     * 3. Promote objects that survived N collections
     * 4. Update remembered set
     * 5. Swap from/to spaces
     */
    void minor_collect();

    /**
     * @brief Perform major GC (entire heap)
     *
     * Algorithm:
     * 1. Collect young generation (copy to old gen)
     * 2. Mark-sweep old generation
     * 3. Rebuild remembered set
     */
    void major_collect();

    /**
     * @brief Force next collection to be major GC
     */
    void request_major_gc() {
        m_major_gc_requested = true;
    }

    /**
     * @brief Get young generation allocator (for testing)
     */
    YoungGenerationAllocator& young_generation() {
        return m_young_gen;
    }

    /**
     * @brief Get number of minor GCs performed
     */
    size_t minor_gc_count() const {
        return m_stats.minor_collections;
    }

    /**
     * @brief Get number of major GCs performed
     */
    size_t major_gc_count() const {
        return m_stats.major_collections;
    }

    // ========================================================================
    // Write Barrier (Cross-Generation Pointer Tracking)
    // ========================================================================

    /**
     * @brief Record write barrier for cross-generation pointer
     * @param obj Object being modified (must be in old generation)
     * @param value Value being written (if in young gen, adds obj to remembered set)
     *
     * Usage:
     *   old_object->field = young_object;
     *   gc.strategy().record_write_barrier(old_object, young_object);
     *
     * This is critical for correctness: Without write barrier, minor GC might
     * miss young objects only reachable from old generation.
     */
    void record_write_barrier(gc_object* obj, gc_object* value);

    /**
     * @brief Check if Object is in young generation
     * @param obj Object to check
     * @return true if in young generation (from-space or to-space)
     */
    bool is_in_young_generation(gc_object* obj) const {
        return m_young_gen.contains(obj);
    }

    /**
     * @brief Check if Object is in old generation
     * @param obj Object to check
     * @return true if in old generation tracking
     */
    bool is_in_old_generation(gc_object* obj) const;

    // ========================================================================
    // Direct Young Generation Allocation (for testing/integration)
    // ========================================================================

    /**
     * @brief Allocate Object directly in young generation
     * @tparam T Type of Object to allocate
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return Pointer to newly allocated Object in young generation
     *
     * This method bypasses the standard gc_manager allocation path and
     * allocates directly in the young generation. Used for:
     * - Testing generational GC behavior
     * - Manual allocation control
     * - Integration with custom allocation strategies
     *
     * If young generation is full, triggers minor GC and retries.
     *
     * Usage:
     *   auto* obj = gc.strategy().allocate_young<MyObject>(args...);
     */
    template<typename T, typename... Args>
    T* allocate_young(Args&&... args) {
        // Use gc_box memory layout: [gc_header] [padding] [size_t offset] [Object]
        // This ensures compatibility with gc_box::get_header()

        constexpr size_t header_size = sizeof(gc_header);
        constexpr size_t obj_alignment = alignof(T);
        constexpr size_t obj_size = sizeof(T);
        constexpr size_t offset_size = sizeof(size_t);

        // Calculate aligned positions
        size_t pos_after_header = header_size;
        size_t offset_pos = (pos_after_header + alignof(size_t) - 1) & ~(alignof(size_t) - 1);
        size_t pos_after_offset = offset_pos + offset_size;
        size_t obj_pos = (pos_after_offset + obj_alignment - 1) & ~(obj_alignment - 1);
        size_t total_size = obj_pos + obj_size;

        constexpr size_t max_align = (alignof(gc_header) > obj_alignment)
            ? alignof(gc_header) : obj_alignment;

        // Try allocate in young generation
        void* memory = m_young_gen.allocate(total_size, max_align);

        if (!memory) {
            // Young gen full - trigger minor GC
            minor_collect();

            // Retry allocation
            memory = m_young_gen.allocate(total_size, max_align);

            if (!memory) {
                // Still can't allocate - fall back to old gen
                return allocate_old<T>(std::forward<Args>(args)...);
            }
        }

        // Initialize header (same as gc_box::allocate)
        gc_header* header = static_cast<gc_header*>(memory);
        new (header) gc_header();
        header->tracer = get_trace_fn<T>();
        header->destructor = gc_box::template get_destruct_fn<T>();
        header->size = obj_size;
        header->alignment = obj_alignment;
        header->padding = obj_pos - header_size;
        header->marked = false;

        // Store offset immediately before Object (for gc_box::get_header)
        char* Base = reinterpret_cast<char*>(memory);
        size_t* offset_ptr = reinterpret_cast<size_t*>(Base + obj_pos - offset_size);
        *offset_ptr = obj_pos;

        // Get aligned Object position
        T* obj = reinterpret_cast<T*>(Base + obj_pos);

        // Construct Object
        try {
            new (obj) T(std::forward<Args>(args)...);
        } catch (...) {
            // Constructor threw - memory stays in young gen, will be collected
            throw;
        }

        // Register with GC
        on_allocate(static_cast<gc_object*>(obj));

        return obj;
    }

    /**
     * @brief Allocate Object directly in old generation
     * @tparam T Type of Object to allocate
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return Pointer to newly allocated Object in old generation
     */
    template<typename T, typename... Args>
    T* allocate_old(Args&&... args) {
        // Use gc_box::allocate with old generation allocator
        T* obj = gc_box::allocate<T>(*m_old_allocator, std::forward<Args>(args)...);

        // Add to old generation tracking
        m_old_gen_objects.push_back(static_cast<gc_object*>(obj));
        m_stats.old_objects++;

        return obj;
    }

private:
    // ========================================================================
    // Generation Management
    // ========================================================================

    YoungGenerationAllocator m_young_gen;  // Young generation (nursery)

    // Old generation objects (mark-sweep)
    std::vector<gc_object*> m_old_gen_objects;

    // Allocator for old generation
    IAllocator* m_old_allocator;
    bool m_owns_old_allocator;  // Do we need to delete it?

    // ========================================================================
    // Root Management
    // ========================================================================

    std::vector<gc_object*> m_roots;
    std::vector<gc_object**> m_persistent_roots;

    // ========================================================================
    // Write Barrier Support (Remembered Set)
    // ========================================================================

    /**
     * @brief Remembered set: old generation objects pointing to young gen
     *
     * When old gen Object is modified to point to young gen Object:
     *   m_remembered_set.insert(old_gen_obj);
     *
     * During minor GC, scan remembered set as additional roots.
     */
    std::vector<gc_object*> m_remembered_set;

    /**
     * @brief Objects promoted to old gen during the current minor collection.
     *
     * Promoted objects leave the young to-space, so the Cheney scan loop never
     * visits them — yet their children may still live in young from-space and
     * must be copied/forwarded before the space swap discards it. minor_collect()
     * drains this list (scanning each promoted object's children) as part of the
     * collection fixpoint, then clears it. Any promoted object still pointing to a
     * surviving young object is an old→young edge and is added to the remembered set.
     */
    std::vector<gc_object*> m_promoted_this_cycle;

    // ========================================================================
    // Statistics
    // ========================================================================

    struct GenerationalStats {
        size_t minor_collections = 0;
        size_t major_collections = 0;
        size_t objects_promoted = 0;
        size_t bytes_promoted = 0;
        size_t young_objects = 0;
        size_t old_objects = 0;
    };

    GenerationalStats m_stats;
    bool m_major_gc_requested = false;

    // Back-reference to GC manager for reclaim/allocate operations
    IGarbageCollector* m_gc_manager = nullptr;

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * @brief Copy Object using Cheney's algorithm (with forwarding pointers)
     * @param obj Object to copy (in from-space)
     * @param free Current free pointer in to-space (will be advanced)
     * @return Pointer to copied Object in to-space
     */
    gc_object* copy_object_cheney(gc_object* obj, char*& free);

    /**
     * @brief Scan Object's children and forward pointers to copied objects
     * @param obj Object to scan
     * @param free Current free pointer (passed to copy_object_cheney)
     */
    void scan_and_forward_children(gc_object* obj, char*& free);

    /**
     * @brief Copy live Object from young to to-space (or promote to old)
     * @param obj Object to copy
     * @return New location of Object
     */
    gc_object* copy_object(gc_object* obj);

    /**
     * @brief Promote Object from young to old generation
     * @param obj Object to promote
     * @return Object in old generation
     */
    gc_object* promote_object(gc_object* obj);

    /**
     * @brief Scan Object and copy/forward all children (legacy)
     * @param obj Object to scan
     */
    void scan_and_forward(gc_object* obj);

    /**
     * @brief Update pointer to forwarded Object (legacy)
     * @param ptr_location Pointer to the pointer to update
     */
    void update_pointer(gc_object** ptr_location);

    /**
     * @brief Rebuild remembered set after minor GC
     *
     * Scans all objects in remembered_set_ and removes entries
     * that no longer point to young generation (i.e., their
     * children were all promoted to old generation).
     */
    void rebuild_remembered_set();

    /**
     * @brief Rebuild age map after space swap
     *
     * Removes stale entries for objects in the old from-space
     * (now to-space). Only keeps entries for objects in the new
     * from-space (live objects after copy and swap).
     */
    void rebuild_age_map();
};

/**
 * @brief Helper method to get young generation allocator (for integration)
 * @param strategy Pointer to GenerationalGCStrategy
 * @return Pointer to YoungGenerationAllocator
 */
inline YoungGenerationAllocator* get_young_gen_allocator(GenerationalGCStrategy* strategy);

/**
 * @brief Helper method to get old generation allocator (for integration)
 * @param strategy Pointer to GenerationalGCStrategy
 * @return Pointer to IAllocator for old generation
 */
inline IAllocator* get_old_gen_allocator(GenerationalGCStrategy* strategy);
