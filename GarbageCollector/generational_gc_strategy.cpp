#include "pch.h"
#include "generational_gc_strategy.h"
#include "gc_box.h"
#include "gc_interface.h"
#include "mark_sweep_strategy.h"  // For MarkingTracer
#include <iostream>
#include <cassert>
#include <cstring>   // std::memcpy (libstdc++ does not provide it transitively)

// ============================================================================
// YoungGenerationAllocator Implementation
// ============================================================================

YoungGenerationAllocator::YoungGenerationAllocator(size_t size_bytes, uint8_t promotion_threshold)
    : m_from_space(nullptr)
    , m_to_space(nullptr)
    , m_next(nullptr)
    , m_end(nullptr)
    , m_space_size(size_bytes)
    , m_promotion_threshold(promotion_threshold) {

    // Allocate two semi-spaces
    m_from_space = static_cast<char*>(std::malloc(m_space_size));
    m_to_space = static_cast<char*>(std::malloc(m_space_size));

    if (!m_from_space || !m_to_space) {
        throw std::bad_alloc();
    }

    // Initialize bump pointer
    m_next = m_from_space;
    m_end = m_from_space + m_space_size;
}

YoungGenerationAllocator::~YoungGenerationAllocator() {
    std::free(m_from_space);
    std::free(m_to_space);
}

void* YoungGenerationAllocator::allocate(size_t size, size_t alignment) {
    // Align next pointer
    size_t offset = reinterpret_cast<uintptr_t>(m_next) % alignment;
    if (offset != 0) {
        m_next += (alignment - offset);
    }

    // Check if we have space
    if (m_next + size > m_end) {
        return nullptr;  // Nursery full, triggers minor GC
    }

    // Bump pointer allocation (fast path!)
    void* result = m_next;
    m_next += size;

    return result;
}

bool YoungGenerationAllocator::contains(void* ptr) const {
    char* p = static_cast<char*>(ptr);

    // Check if in from-space
    if (p >= m_from_space && p < m_from_space + m_space_size) {
        return true;
    }

    // Check if in to-space
    if (p >= m_to_space && p < m_to_space + m_space_size) {
        return true;
    }

    return false;
}

uint8_t YoungGenerationAllocator::get_age(gc_object* obj) const {
    // Linear search in age map (small during minor GC)
    for (const auto& entry : m_age_map) {
        if (entry.first == obj) {
            return entry.second;
        }
    }
    return 0;  // Newly allocated Object
}

void YoungGenerationAllocator::set_age(gc_object* obj, uint8_t age) {
    // Update existing entry or add new one
    for (auto& entry : m_age_map) {
        if (entry.first == obj) {
            entry.second = age;
            return;
        }
    }
    m_age_map.emplace_back(obj, age);
}

void YoungGenerationAllocator::reset() {
    m_next = m_from_space;
    m_age_map.clear();
}

// ============================================================================
// GenerationalGCStrategy Implementation
// ============================================================================

GenerationalGCStrategy::GenerationalGCStrategy(size_t young_size, IAllocator* old_allocator)
    : m_young_gen(young_size, 3)  // Default: promote after 3 survivals
    , m_old_allocator(old_allocator ? old_allocator : new MallocAllocator())
    , m_owns_old_allocator(old_allocator == nullptr) {
}

GenerationalGCStrategy::~GenerationalGCStrategy() {
    // Cleanup old generation objects
    for (gc_object* obj : m_old_gen_objects) {
        gc_box::deallocate(*m_old_allocator, obj);
    }

    if (m_owns_old_allocator) {
        delete m_old_allocator;
    }
}

void GenerationalGCStrategy::on_allocate(gc_object* obj) {
    // Try allocate in young generation first
    // Note: This is called AFTER Object is constructed, so we just track it

    if (m_young_gen.contains(obj)) {
        // Object was allocated in young gen
        m_stats.young_objects++;
    } else {
        // Object was allocated in old gen (large Object or after minor GC)
        m_old_gen_objects.push_back(obj);
        m_stats.old_objects++;
    }
}

void GenerationalGCStrategy::collect() {
    if (m_major_gc_requested) {
        major_collect();
        m_major_gc_requested = false;
    } else {
        minor_collect();
    }
}

// WARNING: Value-based root registration (unsafe for moving GC!)
// Stores the pointer VALUE in roots_ vector, not its LOCATION.
// After minor_collect(), this vector entry is updated (line 216),
// but the caller's local variable still points to the OLD address!
// Prefer register_persistent_root() which stores the pointer LOCATION.
void GenerationalGCStrategy::add_root(gc_object* root) {
    // FATAL ERROR: add_root() is fundamentally incompatible with moving GC
    // This is a hard block - no escape hatch, no backward compatibility.
    std::cerr << "\n*** FATAL ERROR ***\n";
    std::cerr << "add_root() called on GenerationalGCStrategy (moving collector)!\n";
    std::cerr << "This API is value-based and WILL cause stale pointer bugs.\n";
    std::cerr << "\nThe Object will move, but your pointer won't update!\n";
    std::cerr << "\nFix: Use register_persistent_root(&ptr) or gc_root<T> instead.\n";
    std::cerr << "See gc_interface.h for migration guide.\n\n";
    std::abort();  // Unconditionally terminate (debug AND release)
}

void GenerationalGCStrategy::remove_root(gc_object* root) {
    // FATAL ERROR: remove_root() is fundamentally incompatible with moving GC
    std::cerr << "\n*** FATAL ERROR ***\n";
    std::cerr << "remove_root() called on GenerationalGCStrategy (moving collector)!\n";
    std::cerr << "This API is value-based and won't find moved objects.\n";
    std::cerr << "\nFix: Use unregister_persistent_root(&ptr) or gc_root<T> instead.\n";
    std::cerr << "See gc_interface.h for migration guide.\n\n";
    std::abort();  // Unconditionally terminate (debug AND release)
}

void GenerationalGCStrategy::mark_object(gc_object* obj) {
    // Used during major GC for mark-sweep of old generation
    if (obj && !obj->is_marked()) {
        obj->set_marked(true);
    }
}

size_t GenerationalGCStrategy::object_count() const {
    return m_stats.young_objects + m_stats.old_objects;
}

size_t GenerationalGCStrategy::bytes_allocated() const {
    // Young gen bytes
    size_t young_bytes = m_young_gen.bytes_allocated();

    // Old gen bytes (approximate - could track precisely if needed)
    size_t old_bytes = m_old_gen_objects.size() * 128;  // Rough estimate

    return young_bytes + old_bytes;
}

GCStats GenerationalGCStrategy::get_stats() const {
    GCStats stats;
    stats.collections_run = m_stats.minor_collections + m_stats.major_collections;
    stats.objects_collected = 0;  // TODO: Track this
    stats.bytes_reclaimed = 0;     // TODO: Track this
    return stats;
}

void GenerationalGCStrategy::register_persistent_root(gc_object** root_ptr) {
    m_persistent_roots.push_back(root_ptr);
}

void GenerationalGCStrategy::unregister_persistent_root(gc_object** root_ptr) {
    auto it = std::find(m_persistent_roots.begin(), m_persistent_roots.end(), root_ptr);
    if (it != m_persistent_roots.end()) {
        m_persistent_roots.erase(it);
    }
}

// ============================================================================
// Minor GC (Young Generation Copy Collection)
// ============================================================================

void GenerationalGCStrategy::minor_collect() {
    m_stats.minor_collections++;

    // Cheney's copying collector algorithm:
    // Uses two pointers (scan, free) in to-space to implement BFS traversal
    // without recursion or explicit queue

    char* to_space = m_young_gen.m_to_space;
    char* scan = to_space;      // Next Object to scan
    char* free = to_space;      // Next free location for copy

    // ========================================================================
    // Phase 1: Copy roots from young generation to to-space
    // ========================================================================

    // Scan regular roots
    // WARNING: This updates the VECTOR entry, but NOT the caller's local variable!
    // If code did gc->add_root(my_obj), then my_obj still points to from-space after this loop.
    // Only m_persistent_roots (below) can update caller's variables via double-indirection.
    for (gc_object*& root : m_roots) {  // NOTE: Reference so we can update the vector!
        if (m_young_gen.contains(root)) {
            gc_object* copied = copy_object_cheney(root, free);
            root = copied;  // Update root to point to new location (in vector only!)
        }
    }

    // Scan persistent roots
    for (gc_object** root_ptr : m_persistent_roots) {
        gc_object* root = *root_ptr;
        if (root && m_young_gen.contains(root)) {
            gc_object* copied = copy_object_cheney(root, free);
            *root_ptr = copied;  // Update persistent root
        }
    }

    // Scan remembered set (old gen objects pointing to young gen)
    for (gc_object* old_obj : m_remembered_set) {
        // Scan this old Object's children and forward young gen pointers
        scan_and_forward_children(old_obj, free);
    }

    // ========================================================================
    // Phase 2: Cheney scan (iterative BFS)
    // ========================================================================
    // Process objects in to-space from scan→free
    // Each Object's children are copied, advancing free pointer
    // Continue until scan catches up to free (all objects processed)

    while (scan < free) {
        // scan points to gc_header, need to get Object pointer
        gc_header* header = reinterpret_cast<gc_header*>(scan);

        // Calculate Object pointer from header
        // Layout: [gc_header] [padding bytes] [Object]
        //         ^header                      ^Object
        //         <--sizeof(gc_header)--> <--padding-->
        gc_object* obj = reinterpret_cast<gc_object*>(scan + sizeof(gc_header) + header->padding);

        // Scan children and copy/forward them
        scan_and_forward_children(obj, free);

        // Advance scan to the next block, applying the same max-align rounding
        // that copy_object_cheney used when advancing the free pointer, so scan
        // and free stay in lockstep across the inter-block alignment padding.
        size_t obj_size = header->size;
        size_t total_block_size = sizeof(gc_header) + header->padding + obj_size;
        constexpr size_t kBlockAlign = alignof(std::max_align_t);
        scan += (total_block_size + kBlockAlign - 1) & ~(kBlockAlign - 1);
    }

    // ========================================================================
    // Phase 3: Update young generation state
    // ========================================================================

    // Update next pointer for future allocations
    m_young_gen.m_next = free;

    // Swap from/to spaces (to-space becomes new from-space)
    std::swap(m_young_gen.m_from_space, m_young_gen.m_to_space);
    m_young_gen.m_end = m_young_gen.m_from_space + m_young_gen.m_space_size;

    // Rebuild age map: Remove stale entries from old from-space
    // Only keep entries for objects in the new from-space (after swap)
    rebuild_age_map();

    // Update statistics
    m_stats.young_objects = (free - m_young_gen.m_from_space) / 128;  // Rough estimate

    // Rebuild remembered set: Keep only old→young pointers
    // Remove entries where young Object was promoted to old generation
    rebuild_remembered_set();
}

/**
 * @brief Rebuild age map after space swap
 *
 * After space swap, the age_map_ contains stale entries for objects
 * that were in the old from-space (now to-space). We need to remove
 * these entries to avoid using wrong ages if new objects are allocated
 * at the same addresses.
 *
 * Only keep entries for objects in the new from-space (which contains
 * live objects after copy and swap).
 */
void GenerationalGCStrategy::rebuild_age_map() {
    std::vector<std::pair<gc_object*, uint8_t>> new_age_map;

    for (const auto& entry : m_young_gen.m_age_map) {
        gc_object* obj = entry.first;
        // Keep entry if Object is in the current from-space
        if (m_young_gen.contains(obj)) {
            char* obj_ptr = reinterpret_cast<char*>(obj);
            // Check if in from-space specifically (not to-space)
            if (obj_ptr >= m_young_gen.m_from_space &&
                obj_ptr < m_young_gen.m_from_space + m_young_gen.m_space_size) {
                new_age_map.push_back(entry);
            }
        }
    }

    m_young_gen.m_age_map = std::move(new_age_map);
}

/**
 * @brief Rebuild remembered set after minor GC
 *
 * After minor GC, some young objects are promoted to old generation.
 * We need to remove old gen objects from remembered_set_ if they
 * no longer point to young gen (i.e., their children were promoted).
 *
 * Algorithm:
 * 1. Scan each old gen Object in remembered_set_
 * 2. Check if it still points to any young gen objects
 * 3. Keep it if yes, remove if no
 */
void GenerationalGCStrategy::rebuild_remembered_set() {
    // Filter remembered set to keep only objects with old→young pointers
    std::vector<gc_object*> new_remembered_set;

    for (gc_object* old_obj : m_remembered_set) {
        // Check if this old Object still has any young children
        bool has_young_child = false;

        class YoungChildDetector : public Tracer {
        public:
            YoungChildDetector(GenerationalGCStrategy* strategy)
                : m_strategy(strategy), m_found_young(false) {}

            void visit(void* ptr) override {
                if (!ptr) return;
                gc_object* obj = static_cast<gc_object*>(ptr);
                if (m_strategy->is_in_young_generation(obj)) {
                    m_found_young = true;
                }
            }

            bool found_young() const { return m_found_young; }

        private:
            GenerationalGCStrategy* m_strategy;
            bool m_found_young;
        };

        YoungChildDetector detector(this);
        old_obj->trace_references(detector);

        if (detector.found_young()) {
            new_remembered_set.push_back(old_obj);
        }
    }

    m_remembered_set = std::move(new_remembered_set);
}

// ============================================================================
// Major GC (Full Heap Mark-Sweep)
// ============================================================================

void GenerationalGCStrategy::major_collect() {
    m_stats.major_collections++;

    // Step 1: Collect young generation first (promotes survivors to old gen)
    minor_collect();

    // Step 2: Mark phase - trace from all roots through old generation
    // Clear marks on all old gen objects
    for (gc_object* obj : m_old_gen_objects) {
        obj->set_marked(false);
    }

    // Use iterative marking with worklist to avoid stack overflow
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Add persistent roots to worklist (slot-based, safe for moving GC)
    for (gc_object** root_ptr : m_persistent_roots) {
        if (*root_ptr && !(*root_ptr)->is_marked()) {
            tracer.visit(*root_ptr);
        }
    }

    // Add value-based roots to worklist
    // NOTE: These should be empty for GenerationalGCStrategy (add_root aborts)
    for (gc_object* root : m_roots) {
        if (root && !root->is_marked()) {
            tracer.visit(root);
        }
    }

    // Process worklist iteratively
    while (!worklist.empty()) {
        gc_object* obj = worklist.back();
        worklist.pop_back();

        if (!obj->is_marked()) {
            obj->set_marked(true);
            obj->trace_references(tracer);  // Add children to worklist
        }
    }

    // Step 3: Sweep phase - reclaim unmarked old gen objects
    auto it = m_old_gen_objects.begin();
    while (it != m_old_gen_objects.end()) {
        gc_object* obj = *it;
        if (!obj->is_marked()) {
            // Object is unreachable - reclaim it
            gc_box::deallocate(*m_old_allocator, obj);
            it = m_old_gen_objects.erase(it);
        } else {
            ++it;
        }
    }

    m_stats.old_objects = m_old_gen_objects.size();

    // Step 4: Rebuild remembered set (track old->young references)
    rebuild_remembered_set();
}

// ============================================================================
// Helper Methods - Cheney Copy Collection
// ============================================================================

/**
 * @brief Copy Object from from-space to to-space using Cheney's algorithm
 * @param obj Object to copy (in from-space)
 * @param free Current free pointer in to-space (will be advanced)
 * @return Pointer to copied Object in to-space
 *
 * Cheney's forwarding pointer trick:
 * 1. If Object already copied, return forwarding address
 * 2. Otherwise, copy Object to to-space
 * 3. Store forwarding address in original Object
 * 4. Return new address
 */
gc_object* GenerationalGCStrategy::copy_object_cheney(gc_object* obj, char*& free) {
    if (!obj || !m_young_gen.contains(obj)) {
        return obj;  // Not in young gen, don't copy
    }

    // Check if already copied (forwarding pointer set)
    // IMPORTANT: Only check forwarding pointer if Object is in FROM-space
    // Objects in TO-space have their first word as valid data, not forwarding pointer
    char* from_space = m_young_gen.m_from_space;
    char* to_space = m_young_gen.m_to_space;

    bool is_in_from_space = (reinterpret_cast<char*>(obj) >= from_space &&
                              reinterpret_cast<char*>(obj) < from_space + m_young_gen.m_space_size);

    // Get header to check/set forwarding pointer
    gc_header* header = gc_box::get_header(obj);

    if (is_in_from_space) {
        // Object is in from-space - check if it has forwarding pointer in header
        if (header->forwarding_addr != nullptr) {
            // Already copied/promoted, return forwarding address
            return reinterpret_cast<gc_object*>(header->forwarding_addr);
        }
    } else {
        // Object is in to-space - it's already been copied this GC, return as-is
        // This happens when scanning children of objects we already copied
        return obj;
    }

    // Copy Object to to-space

    // Calculate offset from header to Object (must preserve gc_box layout)
    size_t offset_from_header = reinterpret_cast<char*>(obj) - reinterpret_cast<char*>(header);

    // Total size includes header + padding + offset field + Object
    size_t total_size = offset_from_header + header->size;

    // Copy entire gc_box block (header + padding + offset field + Object)
    char* obj_start = reinterpret_cast<char*>(header);
    std::memcpy(free, obj_start, total_size);

    // Get pointer to new Object (use same offset as original)
    gc_object* new_obj = reinterpret_cast<gc_object*>(free + offset_from_header);

    // Get the copied header and clear its forwarding pointer
    gc_header* new_header = gc_box::get_header(new_obj);
    new_header->forwarding_addr = nullptr;  // Copied Object is live, not forwarded

    // Store forwarding pointer in original Object's header
    header->forwarding_addr = new_obj;

    // Increment age
    uint8_t age = m_young_gen.get_age(obj);
    uint8_t new_age = age + 1;

    // Check if should promote (age >= threshold)
    if (new_age >= m_young_gen.m_promotion_threshold) {
        // Promote to old generation instead of keeping in young
        gc_object* promoted = promote_object(new_obj);

        // Store promoted address as forwarding pointer in header
        header->forwarding_addr = promoted;

        // Don't advance free pointer since we're not keeping it in young gen
        return promoted;
    }

    // Not promoted - set new age and keep in young gen
    m_young_gen.set_age(new_obj, new_age);

    // Advance the free pointer, rounding up to the max-align granule so the
    // NEXT block's gc_header (and the object inside it) start on an aligned
    // address. Without this, free drifts to an arbitrary alignment and the next
    // reinterpret_cast<gc_header*>(free) / object access is misaligned (UB).
    // The Cheney scan loop advances by the identical rounding to stay in sync.
    // (Objects over-aligned beyond max_align_t are not supported by the moving
    // young generation; none exist in the Scheme runtime.)
    constexpr size_t kBlockAlign = alignof(std::max_align_t);
    free += (total_size + kBlockAlign - 1) & ~(kBlockAlign - 1);

    return new_obj;
}

/**
 * @brief Scan Object's children and forward pointers to copied objects
 * @param obj Object to scan
 * @param free Current free pointer (passed to copy_object_cheney)
 *
 * Uses a custom tracer that forwards each pointer encountered
 */
void GenerationalGCStrategy::scan_and_forward_children(gc_object* obj, char*& free) {
    // Create forwarding tracer
    class ForwardingTracer : public Tracer {
    public:
        ForwardingTracer(GenerationalGCStrategy* strategy, char*& free_ref)
            : m_strategy(strategy), m_free(free_ref) {}

        // Override visit_slot for moving GC (receives pointer LOCATION)
        void visit_slot(void** ptr_location) override {
            if (!ptr_location) return;

            gc_object** obj_ptr = reinterpret_cast<gc_object**>(ptr_location);
            gc_object* child = *obj_ptr;  // Read pointer from slot

            if (child && m_strategy->m_young_gen.contains(child)) {
                // Copy child to to-space and update source pointer
                *obj_ptr = m_strategy->copy_object_cheney(child, m_free);
            }
        }

        // Override visit for backward compatibility (receives pointer VALUE)
        void visit(void* ptr) override {
            // Note: This cannot update the source pointer since we only have the value
            // Objects should use visit_field() or visit_slot() for moving GC
            // This is only called by legacy code using visit() instead of visit_slot()
            if (!ptr) return;

            gc_object* obj = static_cast<gc_object*>(ptr);
            if (obj && m_strategy->m_young_gen.contains(obj)) {
                // Copy Object but cannot update source - this is a limitation
                // of the visit(void*) API. Use visit_slot(void**) instead.
                m_strategy->copy_object_cheney(obj, m_free);
            }
        }

    private:
        GenerationalGCStrategy* m_strategy;
        char*& m_free;
    };

    ForwardingTracer tracer(this, free);
    obj->trace_references(tracer);
}

gc_object* GenerationalGCStrategy::copy_object(gc_object* obj) {
    // Legacy method - not used in Cheney algorithm
    return obj;
}

/**
 * @brief Promote Object from young to old generation
 * @param obj Object in young generation to promote
 * @return Pointer to Object in old generation
 *
 * Promotion occurs when Object has survived N collections.
 * Object is copied from young gen to old gen (allocated via old allocator).
 */
gc_object* GenerationalGCStrategy::promote_object(gc_object* obj) {
    if (!obj || !m_young_gen.contains(obj)) {
        return obj;  // Not in young gen
    }

    // Allocate in old generation
    gc_header* old_header = gc_box::get_header(obj);

    // Calculate offset from header to Object (preserve layout during copy)
    size_t offset_from_header = reinterpret_cast<char*>(obj) - reinterpret_cast<char*>(old_header);

    // Total size includes header + padding + offset field + Object
    size_t total_size = offset_from_header + old_header->size;

    // Allocate memory in old gen
    void* new_memory = m_old_allocator->allocate(total_size, old_header->alignment);
    if (!new_memory) {
        // Out of memory in old gen - keep in young gen for now
        return obj;
    }

    // Copy header + Object to old gen
    std::memcpy(new_memory, old_header, total_size);

    // Get pointer to promoted Object (use same offset as in young gen)
    gc_object* promoted = reinterpret_cast<gc_object*>(
        static_cast<char*>(new_memory) + offset_from_header
    );

    // Add to old generation tracking
    m_old_gen_objects.push_back(promoted);
    m_stats.objects_promoted++;
    m_stats.bytes_promoted += old_header->size;
    m_stats.old_objects++;
    m_stats.young_objects--;

    return promoted;
}

void GenerationalGCStrategy::scan_and_forward(gc_object* obj) {
    // Legacy method - replaced by scan_and_forward_children
}

void GenerationalGCStrategy::update_pointer(gc_object** ptr_location) {
    // Legacy method - not used in Cheney algorithm
}

// ============================================================================
// Write Barrier Implementation
// ============================================================================

/**
 * @brief Check if Object is in old generation
 * @param obj Object to check
 * @return true if Object is tracked in old_gen_objects_
 *
 * Note: Linear search is acceptable here since:
 * - Only called during write barrier (infrequent)
 * - Old generation is typically small during active allocation phase
 * - Could optimize with hash set if needed
 */
bool GenerationalGCStrategy::is_in_old_generation(gc_object* obj) const {
    if (!obj) {
        return false;
    }

    // Check if Object is in old generation tracking
    for (gc_object* old_obj : m_old_gen_objects) {
        if (old_obj == obj) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Record write barrier for cross-generation pointer
 * @param obj Object being modified (old generation)
 * @param value Value being written (potentially young generation)
 *
 * Algorithm:
 * 1. Check if obj is in old generation
 * 2. Check if value is in young generation
 * 3. If both true, add obj to remembered set (if not already there)
 *
 * Why we need this:
 * - Minor GC only scans young generation + roots + remembered set
 * - If old gen Object points to young gen Object, we need to know about it
 * - Otherwise, minor GC might collect a live young Object!
 *
 * Example:
 *   old_object->child = young_object;  // Cross-generation pointer!
 *   gc.strategy().record_write_barrier(old_object, young_object);
 *
 * Without this barrier, young_object might be collected during next minor GC
 * even though it's reachable from old_object.
 */
void GenerationalGCStrategy::record_write_barrier(gc_object* obj, gc_object* value) {
    if (!obj || !value) {
        return;  // Null pointer, nothing to track
    }

    // Only care if obj is in old gen and value is in young gen
    if (!is_in_old_generation(obj)) {
        return;  // Not an old gen Object, no barrier needed
    }

    if (!is_in_young_generation(value)) {
        return;  // Not pointing to young gen, no barrier needed
    }

    // Cross-generation pointer detected!
    // Add obj to remembered set if not already there

    for (gc_object* remembered : m_remembered_set) {
        if (remembered == obj) {
            return;  // Already in remembered set
        }
    }

    // Add to remembered set
    m_remembered_set.push_back(obj);
}
