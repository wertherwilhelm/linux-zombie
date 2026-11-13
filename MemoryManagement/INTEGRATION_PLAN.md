# Integration Plan: MemoryManagement ↔ GarbageCollector

## Overview

This document outlines how to refactor `GarbageCollector/` to use the new `MemoryManagement/` interfaces while maintaining backward compatibility.

## Current State

**GarbageCollector** currently defines its own interfaces:
- `IGarbageCollector` - Main GC interface
- `gc_object` - Base class for GC-managed objects
- `Tracer` - GC tracing helper
- `IAllocator` - Low-level allocator

These interfaces are **tightly coupled** to GC implementation.

## Target State

**GarbageCollector** implements `memory::` interfaces:
- `memory::IMemoryManager` - Allocate/deallocate
- `memory::ITracer` - GC tracing
- `memory::ITraceable` - Object tracing
- `memory::IWriteBarrier` - Generational barriers
- `memory::IRootManager` - Root management

Libraries depend on `memory::` (not GarbageCollector).

## Migration Strategy

### Phase 1: Parallel Interfaces (Backward Compatible) ✅

**Goal**: Both interfaces coexist, old code keeps working

```cpp
// GarbageCollector/gc_interface.h
#include <MemoryManagement/IMemoryManager.h>
#include <MemoryManagement/ITracer.h>
#include <MemoryManagement/IWriteBarrier.h>
#include <MemoryManagement/IRootManager.h>

class IGarbageCollector
    : public memory::IMemoryManager    // NEW
    , public memory::ITracer           // NEW
    , public memory::IWriteBarrier     // NEW
    , public memory::IRootManager      // NEW
{
public:
    // OLD interface (kept for compatibility)
    virtual void mark_object(gc_object* obj) = 0;
    virtual void collect() = 0;
    // ...

    // NEW interface (implements memory::)
    void* allocate(size_t size, size_t alignment) override {
        return allocate_impl(size, alignment);  // Delegate to old impl
    }

    void visit(void* ptr) override {
        mark_object(static_cast<gc_object*>(ptr));  // Delegate to old impl
    }

    void visit_field(void** slot) override {
        if (slot && *slot) {
            mark_object(static_cast<gc_object*>(*slot));
        }
    }

    // ... other delegations ...
};
```

**Changes**:
- ✅ Add `#include <MemoryManagement/*.h>`
- ✅ Inherit from memory:: interfaces
- ✅ Implement pure virtual methods (delegate to existing impl)
- ✅ Keep old methods (backward compatible)

**Result**: Old code works, new code can use memory:: interfaces

### Phase 2: Update gc_object → memory::ITraceable

**Goal**: GC objects implement standard tracing interface

```cpp
// GarbageCollector/gc_object.h
#include <MemoryManagement/ITraceable.h>

// OLD (before migration)
class gc_object {
public:
    virtual void trace_references(gc_manager* gc) = 0;  // OLD signature
};

// NEW (after migration)
class gc_object : public memory::ITraceable {
public:
    // NEW signature (implements memory::ITraceable)
    void trace_references(memory::ITracer& tracer) override = 0;

    // OLD signature (deprecated, delegates to new)
    [[deprecated("Use trace_references(ITracer&)")]]
    void trace_references(gc_manager* gc) {
        trace_references(static_cast<memory::ITracer&>(*gc));
    }
};
```

**Changes**:
- ✅ Inherit from `memory::ITraceable`
- ✅ Update signature to use `memory::ITracer&`
- ✅ Deprecate old signature (keep for compatibility)
- ✅ Update all derived classes to use new signature

**Migration Path**:
1. Update base class (add both signatures)
2. Update derived classes one by one
3. Compile with deprecation warnings
4. Fix warnings
5. Remove deprecated method

### Phase 3: Refactor Internal Implementation

**Goal**: Clean up internal implementation to use memory:: types

**Files to Update**:
1. `gc_manager.h/cpp` - Use memory::IMemoryManager internally
2. `gc_tracer.h/cpp` - Implement memory::ITracer
3. `gc_field.h` - Use memory::IWriteBarrier
4. `gc_local.h` - Use memory::IRootManager

**Example** (`gc_tracer.h`):
```cpp
// OLD
class Tracer {
public:
    void visit(gc_object* obj);
    void visit_field(gc_object** slot);
};

// NEW
class Tracer : public memory::ITracer {
public:
    void visit(void* ptr) override;
    void visit_field(void** slot) override;
};
```

### Phase 4: Remove Deprecated APIs

**Goal**: Clean break, full migration complete

- Remove old method signatures
- Remove compatibility shims
- Update all call sites
- Update documentation

## Mapping: Old → New

| Old Interface | New Interface | Notes |
|--------------|---------------|-------|
| `IGarbageCollector::allocate_impl()` | `memory::IMemoryManager::allocate()` | Signature matches |
| `IGarbageCollector::mark_object()` | `memory::ITracer::visit()` | Rename |
| `gc_object::trace_references(gc_manager*)` | `memory::ITraceable::trace_references(ITracer&)` | Signature change |
| `IGarbageCollector::add_root(gc_object*)` | `memory::IRootManager::add_root(void**)` | Slot-based! |
| `IGarbageCollector::write_barrier()` | `memory::IWriteBarrier::write_barrier()` | Already matches ✓ |

## Critical Change: Root Management

**OLD** (value-based):
```cpp
Node* node = gc->allocate<Node>();
gc->add_root(node);  // Stores pointer VALUE
```

**NEW** (slot-based):
```cpp
Node* node = gc->allocate<Node>();
gc->add_root(reinterpret_cast<void**>(&node));  // Stores pointer SLOT
```

**Migration**:
1. Keep old `add_root(gc_object*)` for compatibility
2. Implement as wrapper around new `add_root(void**)`
3. Update call sites to use new signature
4. Deprecate old signature
5. Remove deprecated method

## Backward Compatibility Checklist

- [ ] All existing tests pass without modification
- [ ] Deprecation warnings for old APIs
- [ ] Migration guide for users
- [ ] Adapter layer for old → new
- [ ] Performance regression tests (ensure no overhead)

## Testing Strategy

### Unit Tests
```cpp
// Test memory::IMemoryManager interface
TEST(MemoryManagerTest, Allocate) {
    gc_manager gc;
    memory::IMemoryManager& mgr = gc;
    void* ptr = mgr.allocate(100, 8);
    ASSERT_NE(ptr, nullptr);
    mgr.deallocate(ptr, 100);
}

// Test memory::ITracer interface
TEST(TracerTest, Visit) {
    gc_manager gc;
    memory::ITracer& tracer = gc;
    Node* node = gc.allocate<Node>();
    tracer.visit(node);  // Should mark as reachable
}

// Test memory::ITraceable interface
TEST(TraceableTest, TraceReferences) {
    Node* node = gc->allocate<Node>();
    memory::ITraceable& traceable = *node;
    Tracer tracer;
    traceable.trace_references(tracer);
}
```

### Integration Tests
```cpp
// Test with generic memory manager code
template<memory::MemoryManager Mgr>
void test_generic(Mgr& mgr) {
    void* ptr = mgr.allocate(100, 8);
    ASSERT_NE(ptr, nullptr);
    mgr.deallocate(ptr, 100);
}

TEST(IntegrationTest, GenericAllocator) {
    gc_manager gc;
    test_generic(gc);  // Should work via memory::IMemoryManager
}
```

### Performance Tests
```cpp
// Ensure no regression from virtual dispatch
BENCHMARK(AllocationSpeed) {
    gc_manager gc;

    // Direct call (baseline)
    {
        Timer t;
        for (int i = 0; i < 1000000; ++i) {
            void* ptr = gc.allocate_impl(100, 8);
            gc.deallocate(ptr, 100);
        }
        std::cout << "Direct: " << t.elapsed() << "ms\n";
    }

    // Via interface (should be ~same)
    {
        memory::IMemoryManager& mgr = gc;
        Timer t;
        for (int i = 0; i < 1000000; ++i) {
            void* ptr = mgr.allocate(100, 8);
            mgr.deallocate(ptr, 100);
        }
        std::cout << "Virtual: " << t.elapsed() << "ms\n";
    }
}
```

## Risk Mitigation

### Risk 1: Breaking Existing Code
**Mitigation**: Phase 1 maintains full backward compatibility

### Risk 2: Performance Regression
**Mitigation**: Virtual calls are cheap (~5 cycles), benchmark before/after

### Risk 3: ABI Incompatibility
**Mitigation**: Keep old methods as non-virtual wrappers

### Risk 4: Incomplete Migration
**Mitigation**: Deprecation warnings + migration guide

## Timeline Estimate

- **Phase 1** (Parallel Interfaces): 1-2 days
  - Add includes
  - Inherit from memory:: interfaces
  - Implement delegation methods
  - Run existing tests

- **Phase 2** (Update gc_object): 2-3 days
  - Change base class
  - Update derived classes
  - Fix compilation errors
  - Run tests

- **Phase 3** (Internal Refactoring): 3-5 days
  - Update internal implementation
  - Remove duplication
  - Optimize

- **Phase 4** (Cleanup): 1-2 days
  - Remove deprecated methods
  - Update documentation
  - Final testing

**Total**: 1-2 weeks

## Success Criteria

- [ ] All existing tests pass
- [ ] New memory:: tests pass
- [ ] No performance regression
- [ ] Documentation updated
- [ ] SceneGraph works with both old and new interfaces
- [ ] Migration guide written
- [ ] Deprecation warnings added
- [ ] Final cleanup complete

## Next Steps

**Immediate**:
1. Review this plan with stakeholders
2. Get approval for migration
3. Start Phase 1 (parallel interfaces)

**After Phase 1**:
1. Test with SceneGraph
2. Gather feedback
3. Proceed to Phase 2 if successful

---

**Status**: Plan ready, awaiting approval to start Phase 1
