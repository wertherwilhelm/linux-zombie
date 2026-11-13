# MemoryManagement - Core Abstraction Library

**Status**: ✅ Phase 1 Complete - Interfaces Defined

## Overview

`MemoryManagement/` provides the foundational abstraction for memory management strategies, decoupled from any specific implementation (GC, malloc, pools, etc.).

**Key Design Principles**:
1. **Minimal Required Interface** - Only allocate/deallocate mandatory
2. **Optional Capabilities** - GC features detected via concepts
3. **Dependency Inversion** - GC library implements these interfaces
4. **Template-Based** - Zero-cost abstraction, thread-local friendly
5. **Multithreading Ready** - Designed for per-thread allocators

## Architecture

```
MemoryManagement/          # Core abstraction (namespace: memory::)
├── IMemoryManager.h       # REQUIRED: allocate/deallocate
├── ITracer.h              # OPTIONAL: GC tracing (visit/visit_field)
├── ITraceable.h           # OPTIONAL: Object tracing support
├── IWriteBarrier.h        # OPTIONAL: Generational GC barriers
├── IRootManager.h         # OPTIONAL: GC root management
├── concepts.h             # Capability detection (C++20 concepts)
└── policies/              # Standard policy implementations (TODO)
    ├── GCPolicy.h
    ├── SharedPtrPolicy.h
    └── RawPtrPolicy.h

GarbageCollector/          # Implements memory:: interfaces
└── (refactored to use MemoryManagement - TODO)
```

## Interface Hierarchy

### Required Interface

**memory::IMemoryManager** - All allocators must implement:
```cpp
class IMemoryManager {
    virtual void* allocate(size_t size, size_t alignment) = 0;
    virtual void deallocate(void* ptr, size_t size) = 0;
};
```

### Optional Interfaces (GC-Specific)

**memory::ITracer** - For GC tracing:
```cpp
class ITracer {
    virtual void visit(void* ptr) = 0;              // Value-based (mark-sweep)
    virtual void visit_field(void** slot) = 0;      // Slot-based (moving GC)
    virtual void visit_range(void** slots, size_t count) = 0;
};
```

**memory::ITraceable** - For GC objects:
```cpp
class ITraceable {
    virtual void trace_references(ITracer& tracer) = 0;
};
```

**memory::IWriteBarrier** - For generational GC:
```cpp
class IWriteBarrier {
    virtual void write_barrier(void* owner, void* old_value, void* new_value) = 0;
};
```

**memory::IRootManager** - For GC roots:
```cpp
class IRootManager {
    virtual void add_root(void** root_slot) = 0;
    virtual void remove_root(void** root_slot) = 0;
};
```

## Capability Detection

Use C++20 concepts to detect optional capabilities:

```cpp
#include <MemoryManagement/concepts.h>

// Detect individual capabilities
template<typename T>
concept SupportsTracing = /* ... */;

template<typename T>
concept SupportsWriteBarrier = /* ... */;

template<typename T>
concept SupportsRooting = /* ... */;

// Compound capabilities
template<typename T>
concept TracingGC =
    MemoryManager<T> &&
    SupportsTracing<T> &&
    SupportsRooting<T>;

template<typename T>
concept GenerationalGC =
    TracingGC<T> &&
    SupportsWriteBarrier<T>;
```

### Conditional Compilation

```cpp
template<memory::MemoryManager Mgr>
void process(Mgr& mgr, Object* obj) {
    // Allocate (always available)
    void* ptr = mgr.allocate(100, 8);

    // Trace (only if supported)
    if constexpr (memory::SupportsTracing<Mgr>) {
        obj->trace_references(mgr);
    }

    // Write barrier (only if supported)
    if constexpr (memory::SupportsWriteBarrier<Mgr>) {
        mgr.write_barrier(obj, old_val, new_val);
    }

    // Deallocate (always available)
    mgr.deallocate(ptr, 100);
}
```

## Thread-Safety Model

The abstraction is designed for **thread-local memory managers**:

```cpp
// Each thread has its own allocator
thread_local MyAllocator allocator;

// No synchronization needed (thread-isolated)
void* ptr = allocator.allocate(100, 8);
```

**Benefits**:
- Zero synchronization overhead
- Perfect scaling to N threads
- Cache-friendly (each thread owns its memory)
- Simple implementation (no locks)

**Future**: Parallel GC via work-stealing between thread-local heaps

## How GarbageCollector Uses This

**Before** (GC defines interfaces):
```cpp
// GarbageCollector/gc_interface.h
class IGarbageCollector {
    virtual void* allocate(size, align) = 0;
    virtual void mark_object(gc_object*) = 0;
    // ...
};
```

**After** (GC implements memory:: interfaces):
```cpp
// GarbageCollector/gc_manager.h
#include <MemoryManagement/IMemoryManager.h>
#include <MemoryManagement/ITracer.h>
#include <MemoryManagement/IWriteBarrier.h>
#include <MemoryManagement/IRootManager.h>

class IGarbageCollector
    : public memory::IMemoryManager    // Required
    , public memory::ITracer           // Optional (GC)
    , public memory::IWriteBarrier     // Optional (generational)
    , public memory::IRootManager      // Optional (roots)
{
    // Implementation...
};
```

**Dependency Flow**:
```
Libraries (SceneGraph, etc.)
    ↓ depends on
MemoryManagement (interfaces)
    ↑ implemented by
GarbageCollector (concrete GC)
```

## Migration Path

### Phase 1: Extract Interfaces ✅
- [x] Create MemoryManagement/ directory
- [x] Define core interfaces (IMemoryManager, ITracer, etc.)
- [x] Define capability concepts
- [x] Document design and usage

### Phase 2: Refactor GarbageCollector (TODO)
- [ ] Update IGarbageCollector to inherit from memory:: interfaces
- [ ] Update gc_object to use memory::ITraceable
- [ ] Update Tracer to implement memory::ITracer
- [ ] Update gc_field to use memory::IWriteBarrier
- [ ] Verify backward compatibility

### Phase 3: Implement Policies (TODO)
- [ ] Create GCPolicy adapter
- [ ] Create SharedPtrPolicy adapter
- [ ] Create RawPtrPolicy adapter
- [ ] Test with SceneGraph

### Phase 4: Update Libraries (TODO)
- [ ] Update SceneGraph to use memory:: concepts
- [ ] Create reference examples
- [ ] Document best practices

## Example: Library Using Memory Abstraction

```cpp
// MyLibrary/data_structure.h
#include <MemoryManagement/concepts.h>

template<memory::MemoryManager Allocator>
class BinaryTree {
    Allocator& allocator_;
    BinaryTree* left_ = nullptr;
    BinaryTree* right_ = nullptr;

public:
    BinaryTree(Allocator& alloc) : allocator_(alloc) {}

    void insert(int value) {
        // Allocate node
        BinaryTree* node = static_cast<BinaryTree*>(
            allocator_.allocate(sizeof(BinaryTree), alignof(BinaryTree))
        );
        new (node) BinaryTree(allocator_);

        // ... insertion logic ...

        // Fire write barrier if generational GC
        if constexpr (memory::SupportsWriteBarrier<Allocator>) {
            allocator_.write_barrier(this, left_, node);
        }
        left_ = node;
    }

    // Conditional tracing
    void trace_if_gc(auto& tracer)
        requires memory::SupportsTracing<decltype(tracer)>
    {
        if (left_) tracer.visit_field(reinterpret_cast<void**>(&left_));
        if (right_) tracer.visit_field(reinterpret_cast<void**>(&right_));
    }
};

// Usage with different allocators
void example() {
    // With GC (tracing + barriers)
    GenerationalGC gc;
    BinaryTree<GenerationalGC> gc_tree(gc);

    // With malloc (no GC)
    MallocAllocator malloc_alloc;
    BinaryTree<MallocAllocator> malloc_tree(malloc_alloc);

    // Same code, different memory strategies!
}
```

## Comparison with Other Approaches

### Option A: Bake GC into Library
```cpp
class Node : public gc_object { /* ... */ };
```
❌ Can't use library without GC
❌ Forces one memory strategy
✅ Simple integration

### Option B: Template on Allocator (STL-style)
```cpp
template<typename Allocator>
class Node { /* ... */ };
```
✅ Flexible (any allocator)
❌ Verbose (every class templated)
❌ No GC-specific features (write barriers, tracing)

### Option C: Policy Pattern (Our Approach)
```cpp
template<typename Policy>
class Node { /* ... */ };
```
✅ Flexible (any memory strategy)
✅ GC-specific features via concepts
✅ Clean separation of concerns
✅ Zero-cost abstraction

## Benefits

### For Library Authors
- **Write once, work everywhere** - Same code works with GC, smart pointers, or raw pointers
- **No GC dependency** - Library can be standalone
- **Gradual adoption** - Start with raw pointers, add GC later
- **Feature detection** - Enable GC optimizations when available

### For GC Implementors
- **Clean interface** - Clear contract for GC capabilities
- **Testable** - Mock allocators for testing
- **Composable** - Mix and match capabilities
- **Extensible** - New GC strategies just implement interfaces

### For Application Developers
- **Flexibility** - Choose memory strategy per use case
- **Performance** - Template specialization for zero overhead
- **Migration** - Incrementally adopt GC
- **Debugging** - Swap allocators for leak detection

## Future Work

### Near-Term
1. Implement standard policies (GCPolicy, SharedPtrPolicy, RawPtrPolicy)
2. Refactor GarbageCollector to use memory:: interfaces
3. Update SceneGraph to demonstrate usage
4. Write integration guide

### Long-Term
1. **Parallel GC** - Work-stealing between thread-local heaps
2. **Conservative GC** - Stack scanning without precise roots
3. **Concurrent GC** - Incremental marking with write barriers
4. **NUMA Support** - Allocators aware of NUMA topology
5. **Profiling Tools** - Allocation tracking and visualization

## Design Rationale

### Why Interfaces Instead of Type Erasure?
- **Performance** - Virtual calls are cheap (~5 cycles)
- **Simplicity** - Clear inheritance hierarchy
- **Flexibility** - Easy to add new capabilities
- **Debugging** - RTTI available for introspection

### Why Concepts for Capability Detection?
- **Compile-time** - Zero runtime overhead
- **Type-safe** - Compiler errors instead of runtime failures
- **Composable** - Build compound capabilities (TracingGC, GenerationalGC)
- **Future-proof** - New capabilities added without breaking existing code

### Why Thread-Local Pattern?
- **Scalability** - Perfect parallelism (no contention)
- **Simplicity** - No locking in fast path
- **Locality** - Better cache behavior
- **Future** - Parallel GC via work-stealing

## Contributing

When adding new capabilities:
1. Define interface in `Memory*/I*.h`
2. Add concept in `concepts.h`
3. Update this README
4. Implement in concrete allocators
5. Add tests demonstrating capability

## References

- C++20 Concepts: https://en.cppreference.com/w/cpp/language/constraints
- Memory Management Patterns: Boehm GC, JVM GC, V8 GC
- Policy-Based Design: Andrei Alexandrescu's "Modern C++ Design"
- Thread-Local Storage: POSIX thread_local, C++11 thread_local

---

**Status**: Foundation complete, ready for Phase 2 (GC refactoring)
