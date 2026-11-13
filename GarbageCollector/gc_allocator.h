#pragma once

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <vector>

/**
 * IAllocator - Abstract allocator interface
 *
 * Provides virtual allocation/deallocation for pluggable memory management.
 * Implementations can use malloc, pool allocation, arena allocation, etc.
 */
struct IAllocator {
    virtual ~IAllocator() = default;

    /**
     * Allocate memory with specified size and alignment
     * @param bytes Number of bytes to allocate
     * @param align Alignment requirement (power of 2)
     * @return Pointer to allocated memory, or nullptr on failure
     */
    virtual void* allocate(size_t bytes, size_t align) = 0;

    /**
     * Deallocate memory previously allocated
     * @param p Pointer to memory to free
     * @param bytes Original allocation size (for size-tracking allocators)
     */
    virtual void deallocate(void* p, size_t bytes) = 0;
};

/**
 * MallocAllocator - Standard malloc/free based allocator
 *
 * Uses aligned_alloc or _aligned_malloc to honor alignment requirements.
 * Falls back to posix_memalign or over-allocating with manual alignment.
 */
struct MallocAllocator : IAllocator {
    void* allocate(size_t bytes, size_t align) override {
        if (align == 0) align = alignof(std::max_align_t);

#if defined(_WIN32) || defined(_WIN64)
        // Windows: use _aligned_malloc
        return _aligned_malloc(bytes, align);
#elif defined(__APPLE__) || (_POSIX_C_SOURCE >= 200112L)
        // POSIX: use posix_memalign
        void* ptr = nullptr;
        if (posix_memalign(&ptr, align, bytes) != 0) {
            return nullptr;
        }
        return ptr;
#elif __cplusplus >= 201703L
        // C++17: use aligned_alloc (requires size to be multiple of alignment)
        size_t aligned_bytes = (bytes + align - 1) & ~(align - 1);
        return std::aligned_alloc(align, aligned_bytes);
#else
        // Fallback: over-allocate and manually align
        // Allocate extra space for alignment and storing original pointer
        size_t total_size = bytes + align + sizeof(void*);
        void* raw = std::malloc(total_size);
        if (!raw) return nullptr;

        // Calculate aligned position
        char* raw_bytes = reinterpret_cast<char*>(raw);
        char* aligned_pos = raw_bytes + sizeof(void*);
        size_t space = total_size - sizeof(void*);
        void* aligned = aligned_pos;

        if (std::align(align, bytes, aligned, space)) {
            // Store original pointer before aligned memory
            void** orig_ptr = reinterpret_cast<void**>(aligned) - 1;
            *orig_ptr = raw;
            return aligned;
        }

        std::free(raw);
        return nullptr;
#endif
    }

    void deallocate(void* p, size_t bytes) override {
        (void)bytes;
        if (!p) return;

#if defined(_WIN32) || defined(_WIN64)
        _aligned_free(p);
#elif defined(__APPLE__) || (_POSIX_C_SOURCE >= 200112L) || (__cplusplus >= 201703L)
        std::free(p);
#else
        // Fallback: recover original pointer
        void** orig_ptr = reinterpret_cast<void**>(p) - 1;
        std::free(*orig_ptr);
#endif
    }
};

/**
 * PoolAllocator - Fixed-size block pool allocator
 *
 * Pre-allocates pages and carves them into fixed-size blocks.
 * Very fast allocation/deallocation via free list.
 * Single size-class version (all allocations rounded to block_size).
 */
class PoolAllocator : public IAllocator {
public:
    /**
     * Construct pool allocator
     * @param block_size Size of each block (allocations rounded up to this)
     * @param page_size Size of each page (carved into blocks)
     */
    explicit PoolAllocator(size_t block_size = 256, size_t page_size = 4096)
        : m_block_size(block_size)
        , m_page_size(page_size)
        , m_free_list(nullptr) {
        // Ensure block size is at least pointer-sized
        if (m_block_size < sizeof(FreeNode)) {
            m_block_size = sizeof(FreeNode);
        }
    }

    // Move constructor (required because vector<unique_ptr> is non-copyable)
    PoolAllocator(PoolAllocator&& other) noexcept
        : m_block_size(other.m_block_size)
        , m_page_size(other.m_page_size)
        , m_free_list(other.m_free_list)
        , m_pages(std::move(other.m_pages)) {
        other.m_free_list = nullptr;
    }

    // Move assignment
    PoolAllocator& operator=(PoolAllocator&& other) noexcept {
        if (this != &other) {
            m_block_size = other.m_block_size;
            m_page_size = other.m_page_size;
            m_free_list = other.m_free_list;
            m_pages = std::move(other.m_pages);
            other.m_free_list = nullptr;
        }
        return *this;
    }

    // Delete copy operations (pool allocator should not be copied)
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    ~PoolAllocator() override {
        // Pages automatically freed via unique_ptr
    }

    void* allocate(size_t bytes, size_t align) override {
        (void)align;  // Single size-class ignores alignment

        // All allocations use m_block_size
        if (bytes > m_block_size) {
            // For now, fall back to malloc for oversized allocations
            // TODO: Support multiple size buckets
            return std::malloc(bytes);
        }

        // If free list empty, carve a new page
        if (!m_free_list) {
            allocate_page();
        }

        // Pop from free list
        void* result = m_free_list;
        m_free_list = m_free_list->next;
        return result;
    }

    void deallocate(void* p, size_t bytes) override {
        if (!p) return;

        // If oversized (malloc'd), free directly
        if (bytes > m_block_size) {
            std::free(p);
            return;
        }

        // Push back onto free list
        FreeNode* node = static_cast<FreeNode*>(p);
        node->next = m_free_list;
        m_free_list = node;
    }

private:
    struct FreeNode {
        FreeNode* next;
    };

    void allocate_page() {
        // Allocate new page
        auto page = std::make_unique<std::byte[]>(m_page_size);
        std::byte* Base = page.get();

        // Carve page into blocks and link into free list
        size_t num_blocks = m_page_size / m_block_size;
        for (size_t i = 0; i < num_blocks; ++i) {
            FreeNode* node = reinterpret_cast<FreeNode*>(Base + i * m_block_size);
            node->next = m_free_list;
            m_free_list = node;
        }

        // Store page to keep it alive
        m_pages.push_back(std::move(page));
    }

    size_t m_block_size;
    size_t m_page_size;
    FreeNode* m_free_list;
    std::vector<std::unique_ptr<std::byte[]>> m_pages;
};

/**
 * StdAllocAdapter - Adapter to use IAllocator with std containers
 *
 * Allows std::vector, std::map, etc. to use GC allocator.
 *
 * Usage:
 *   std::vector<int, StdAllocAdapter<int>> vec{StdAllocAdapter<int>{&gc.allocator()}};
 */
template<class T>
struct StdAllocAdapter {
    using ValueType = T;

    IAllocator* alloc;

    StdAllocAdapter(IAllocator* a = nullptr) : alloc(a) {}

    template<class U>
    StdAllocAdapter(const StdAllocAdapter<U>& other) : alloc(other.alloc) {}

    T* allocate(std::size_t n) {
        if (!alloc) return static_cast<T*>(std::malloc(n * sizeof(T)));
        return static_cast<T*>(alloc->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t n) {
        if (!alloc) {
            std::free(p);
            return;
        }
        alloc->deallocate(p, n * sizeof(T));
    }

    bool operator==(const StdAllocAdapter& rhs) const { return alloc == rhs.alloc; }
    bool operator!=(const StdAllocAdapter& rhs) const { return !(*this == rhs); }
};

#ifdef __cpp_lib_memory_resource
#include <memory_resource>

/**
 * GcMemoryResource - PMR wrapper for IAllocator (C++17+)
 *
 * Adapts IAllocator to std::pmr::memory_resource interface.
 * Enables use with std::pmr containers (pmr::vector, pmr::map, etc.)
 *
 * Usage:
 *   GcMemoryResource gc_mr(&gc.allocator());
 *   std::pmr::vector<int> vec(&gc_mr);
 */
class GcMemoryResource : public std::pmr::memory_resource {
public:
    explicit GcMemoryResource(IAllocator* alloc = nullptr)
        : m_alloc(alloc) {}

    void set_allocator(IAllocator* alloc) {
        m_alloc = alloc;
    }

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (!m_alloc) {
            return std::malloc(bytes);
        }
        return m_alloc->allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        (void)alignment;
        if (!m_alloc) {
            std::free(p);
            return;
        }
        m_alloc->deallocate(p, bytes);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        const GcMemoryResource* other_gc = dynamic_cast<const GcMemoryResource*>(&other);
        return other_gc && other_gc->m_alloc == m_alloc;
    }

private:
    IAllocator* m_alloc;
};
#endif // __cpp_lib_memory_resource
