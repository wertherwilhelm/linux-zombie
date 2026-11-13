# Concurrent GC Design for Scene Graph Application

**Date**: December 31, 2025
**Status**: Design Document
**Target**: Non-blocking GC for real-time rendering ("render never pauses")

## Problem Statement

Your scene graph application has the following requirements:
- **Multi-threaded rendering** with shared scene graph
- **On-demand LOD node creation** during traversal (reentrant allocation during GC)
- **R-tree spatial indexing backend** feeding objects on-the-fly
- **Multiple concurrent traversals** (culling, rendering, physics, etc.)
- **Critical constraint**: "Desire is to have render never pause"

The current mark-and-sweep GC is **stop-the-world**, which blocks all mutator threads during collection. This violates the "render never pauses" requirement.

---

## Current Architecture Strengths

✅ **Already implemented**:
1. Thread-local GC instances (each thread has its own GC)
2. Reentrant allocation (LOD nodes can be created during GC)
3. Per-GC persistent roots (no cross-contamination)
4. Iterative marking (avoids stack overflow on deep hierarchies)

✅ **What works well**:
- Thread-local GCs mean collections are already **per-thread**, not global
- Multiple traversal threads can run independently
- Each thread's GC pauses only that thread (not all threads)

---

## GC Pause Analysis

### Current Stop-the-World Per-Thread GC

**Scenario**: Render thread with thread-local GC containing 100,000 scene nodes

**Collection phases**:
1. **Mark phase**: Iteratively trace from roots (~10-50ms for 100K objects)
2. **Sweep phase**: Delete unmarked objects (~5-20ms)
3. **Total pause**: ~15-70ms per collection

**Impact on rendering**:
- Target: 60 FPS → 16.67ms frame budget
- **Problem**: Single GC pause (15-70ms) blows the frame budget
- Result: Frame drops, stuttering, visible pauses

### Key Insight: Thread-Local GC is Already "Concurrent"

Since each thread has its own GC:
- **Render thread GC** collects only render thread's objects
- **Physics thread GC** collects only physics thread's objects
- **Background loader GC** collects only loader thread's objects

**They don't interfere with each other!**

However, **within a single thread**, the GC still stops that thread completely.

---

## Design Options

### Option 1: Incremental GC (Recommended for Scene Graphs)

**Concept**: Split mark-and-sweep into small increments, interleaved with rendering.

**Architecture**:
```
Frame 1: Render (14ms) + GC mark 10% (1ms) = 15ms
Frame 2: Render (14ms) + GC mark 10% (1ms) = 15ms
...
Frame 10: Render (14ms) + GC mark 10% (1ms) = 15ms
Frame 11: Render (14ms) + GC sweep (1ms) = 15ms
```

**Implementation**:
- Add `IncrementalMarkSweepStrategy` alongside `MarkSweepStrategy`
- Track marking progress (e.g., worklist + "objects scanned so far")
- Each frame, mark N objects (tunable based on frame budget)
- When marking completes, sweep incrementally

**Pros**:
- ✅ Render thread never pauses longer than ~1-2ms
- ✅ No read barriers (doesn't slow down field access)
- ✅ Works with existing architecture (just a new strategy)
- ✅ Tunable per frame budget

**Cons**:
- ⚠️ Requires write barrier (when mutator modifies pointers during marking)
- ⚠️ Slightly higher total GC time (due to barriers)
- ⚠️ More complex implementation

**Best for**:
- Real-time rendering with strict frame budgets
- Applications where frame drops are unacceptable

---

### Option 2: Concurrent GC (Advanced)

**Concept**: GC runs in a separate thread, mutator threads continue running.

**Architecture**:
```
Thread 1 (Render):  [Frame] [Frame] [Frame] [Frame] ...
Thread 2 (GC):      [ Mark ] [ Sweep ] [ Idle ] [ Mark ] ...
```

**Implementation**:
- Spawn GC worker thread per mutator thread
- Mutator uses **read/write barriers** for safe concurrent access
- GC periodically pauses mutators briefly for "handshake" (root scanning)

**Pros**:
- ✅ Render thread rarely pauses (only for brief handshakes)
- ✅ GC overhead is on separate CPU core
- ✅ Scales well with many CPU cores

**Cons**:
- ❌ Complex implementation (requires barriers, synchronization)
- ❌ Read/write barriers slow down all field access (~5-10% overhead)
- ❌ Requires atomic operations for tri-color marking
- ❌ Harder to debug (race conditions, deadlocks)

**Best for**:
- Server applications with many CPU cores available
- Applications that can tolerate slight throughput loss for lower latency

---

### Option 3: Generational GC (Excellent for Scene Graphs!)

**Concept**: Most objects die young. Focus GC effort on newly allocated objects.

**Architecture**:
```
Young Generation (nursery):
  - New LOD nodes, temporary traversal data
  - Collected frequently (~every few frames)
  - Fast to collect (small, mostly dead)

Old Generation (tenured):
  - Long-lived scene graph structure
  - Collected rarely (~every few seconds)
  - Slower to collect (large, mostly alive)
```

**Implementation**:
- Allocate new objects in "young generation" bump-pointer region
- Minor GC: Copy-collect young generation (~1-5ms)
- Objects surviving N collections → promoted to old generation
- Major GC: Mark-sweep old generation (~50-100ms, but rare)

**Pros**:
- ✅ Most collections are **minor GCs** (~1-5ms) - fits in frame budget!
- ✅ Major GCs are rare (tunable, e.g., every 100 frames)
- ✅ Excellent for LOD pattern (temp nodes die quickly)
- ✅ Lower pause times than simple mark-sweep

**Cons**:
- ⚠️ Write barrier for cross-generation pointers
- ⚠️ More complex memory layout
- ⚠️ Major GCs still pause (but rare enough to run during level transitions)

**Best for**:
- Real-time graphics with LOD hierarchies
- Scenarios with high allocation rate of short-lived objects
- **YOUR USE CASE!**

---

### Option 4: Region-Based GC (Future Exploration)

**Concept**: Divide heap into regions, collect one region at a time.

**Architecture**:
```
Region 1: [Terrain LOD nodes]  → Collect
Region 2: [Character models]   → Skip
Region 3: [UI elements]        → Skip
```

**Implementation**:
- Allocate related objects in the same region
- Collect regions independently
- Use "remembered sets" to track cross-region pointers

**Pros**:
- ✅ Pause time proportional to region size (not total heap)
- ✅ Can prioritize regions (e.g., collect temporary LOD regions first)
- ✅ Scales to large heaps

**Cons**:
- ❌ Very complex implementation
- ❌ Requires application hints about object grouping
- ❌ Write barriers for remembered sets

**Best for**:
- Very large heaps (> 1GB)
- Applications with clear object lifetime patterns
- When incremental/generational GC isn't enough

---

## Recommendation for Your Scene Graph

**Phase 1: Generational GC** (Highest ROI)

Why generational GC is perfect for your use case:
1. **LOD nodes are ephemeral** - Created during traversal, die quickly
2. **Scene structure is persistent** - Root nodes, skeleton live for minutes/hours
3. **Minor GCs fit in frame budget** - 1-5ms pause every ~10 frames is acceptable
4. **Major GCs during transitions** - Run full collection during level loads

**Example tuning**:
- Young generation: 4MB (enough for ~50,000 temp LOD nodes)
- Minor GC threshold: Every 2MB allocated (~every 5-10 frames)
- Promotion threshold: Objects surviving 3 minor GCs
- Major GC threshold: Old generation exceeds 100MB or manual trigger

**Expected pause times**:
- Minor GC: **1-5ms** (fits in 16ms frame budget!)
- Major GC: **50-100ms** (run during loading screens)

---

**Phase 2: Incremental Marking** (If generational isn't enough)

If major GCs are still too long:
1. Use generational GC for young generation (fast minor collections)
2. Use incremental marking for old generation (major collections spread over frames)

**Combined approach**:
```
Frames 1-10:  Minor GC (~2ms)
Frames 11-20: Minor GC (~2ms) + Incremental major mark 10%
Frames 21-30: Minor GC (~2ms) + Incremental major mark 10%
...
Frames 91-100: Minor GC (~2ms) + Incremental major sweep
```

**Pause times**:
- Most frames: **2-3ms** (minor GC only)
- Frames with major increment: **3-5ms** (minor + major increment)
- **Never exceeds 5ms!**

---

## Implementation Roadmap

### Milestone 1: Generational GC Foundation (2-3 weeks)

**Tasks**:
1. Implement young generation allocator (bump pointer)
2. Implement copy-collection algorithm for minor GC
3. Add write barrier for cross-generation pointers
4. Implement promotion logic (age tracking)
5. Add tuning parameters (generation sizes, promotion threshold)

**Deliverables**:
- `GenerationalGCStrategy` class
- Tests demonstrating minor GC pause < 5ms
- Performance benchmarks (allocation throughput, pause time distribution)

---

### Milestone 2: Incremental Old Generation (1-2 weeks)

**Tasks**:
1. Extend generational GC with incremental marking for old generation
2. Add tri-color marking state (white/gray/black)
3. Implement work-stealing for mark worklist
4. Add frame-budget tuning (e.g., "mark 1000 objects per frame")

**Deliverables**:
- Hybrid generational + incremental GC
- Tests demonstrating all pauses < 5ms
- Documentation of tuning parameters

---

### Milestone 3: Profiling and Tuning (1 week)

**Tasks**:
1. Integrate with real scene graph application
2. Profile pause times under various workloads
3. Tune generation sizes and collection frequency
4. Add GC statistics dashboard (pause histogram, memory usage)

**Deliverables**:
- Production-ready configuration
- Performance report (before/after pause times)
- Tuning guide for different scene complexities

---

## Write Barrier Design

For both generational and incremental GC, you'll need a write barrier.

**Naive approach** (source-level):
```cpp
void set_child(LODNode* parent, LODNode* child) {
    parent->child = child;
    // Write barrier: Track cross-generation or gray→white pointers
    gc->record_pointer_update(parent, child);
}
```

**Better approach** (compiler/proxy):
```cpp
template<typename T>
class gc_ref {
public:
    gc_ref& operator=(T* ptr) {
        gc_context::current()->write_barrier(this, ptr);
        _ptr = ptr;
        return *this;
    }

    T* operator->() const { return _ptr; }

private:
    T* _ptr;
};

struct LODNode : gc_object {
    gc_ref<LODNode> child;  // Automatically applies write barrier!
};
```

**Barrier overhead**:
- Generational: ~1-2 cycles per pointer write
- Incremental: ~3-5 cycles per pointer write
- **Impact**: ~5-10% throughput loss, but eliminates frame drops

---

## Next Steps

1. **Decide on approach**: Generational GC (recommended) vs. Incremental GC
2. **Prototype young generation allocator** (bump pointer, copy collector)
3. **Benchmark pause times** on realistic scene graph data
4. **Iterate on tuning** (generation sizes, promotion threshold)

Would you like me to proceed with implementing a generational GC prototype?

---

## References

- **"The Garbage Collection Handbook"** by Jones, Hosking, Moss (2011)
  - Chapter 7: Generational GC
  - Chapter 8: Incremental GC

- **Unreal Engine's GC** (generational + incremental)
  - Young generation for temp objects
  - Incremental marking for old generation
  - Write barriers via `TObjectPtr<T>`

- **V8 JavaScript Engine** (generational + concurrent)
  - Scavenger for young generation (~2ms pauses)
  - Concurrent marker for old generation
  - Excellent model for real-time constraints

---

*End of Document*
