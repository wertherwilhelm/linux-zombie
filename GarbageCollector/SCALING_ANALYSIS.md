# Scaling Analysis: Earth-to-Centimeter LIDAR Scene Graph

**Date**: December 31, 2025
**Application**: Hierarchical scene graph for planetary-scale LIDAR visualization
**Resolution**: Earth surface → centimeter-level detail
**GC Strategy**: Evaluating current mark-sweep vs. generational GC

---

## Problem Scale

### Theoretical Maximum (Full Earth at cm Resolution)

**Earth surface area**: ~510,000,000 km² = 5.1 × 10¹⁴ m²

**At 1cm² resolution**:
- Points needed: 5.1 × 10¹⁸ points (5.1 quintillion!)
- **Clearly impossible to load all at once**

### Practical Reality: Hierarchical LOD

You'll use a **quadtree/octree hierarchy** to stream data on-demand:

```
Level 0 (Planet):        100s of nodes      (continental-scale tiles)
Level 1 (Continental):   1,000s of nodes    (country-scale)
Level 2 (Regional):      10,000s of nodes   (city-scale)
Level 3 (Local):         100,000s of nodes  (block-scale)
Level 4 (Street):        1,000,000s nodes   (building-scale)
Level 5 (Detail):        10,000,000s nodes  (meter-scale LIDAR)
Level 6 (High Detail):   100,000,000s nodes (centimeter-scale LIDAR)
```

**Key insight**: You'll only keep **visible + nearby** nodes in memory, not the entire hierarchy.

---

## Scene Size Scenarios

### Scenario 1: Overview (Zoomed Out to Continent)

**View**: Entire continent visible (e.g., North America)
**LOD Levels Active**: 0-3
**Objects in Memory**:
- Coarse tiles: ~1,000 nodes
- Medium tiles: ~10,000 nodes
- Fine tiles (near view): ~50,000 nodes
- **Total**: ~61,000 objects

**GC Behavior**:
- Mark-sweep pause: ~10-15ms (acceptable for 60 FPS)
- Minor GC pause: ~2-3ms (much better!)

---

### Scenario 2: City View (Zoomed to Street Level)

**View**: City block with buildings
**LOD Levels Active**: 2-5
**Objects in Memory**:
- Coarse tiles (background): ~10,000 nodes
- Medium tiles: ~100,000 nodes
- Fine tiles (foreground): ~500,000 nodes
- **Total**: ~610,000 objects

**GC Behavior**:
- Mark-sweep pause: ~80-120ms ❌ **FRAME BUDGET BLOWN** (60 FPS = 16ms)
- Minor GC pause: ~3-5ms ✅ **Acceptable**
- Major GC pause: ~80-120ms (run during loading/transition)

**Verdict**: Generational GC **essential** at this scale!

---

### Scenario 3: Maximum Detail (cm-level LIDAR Close-Up)

**View**: Centimeter-level scan of building facade
**LOD Levels Active**: 4-6
**Objects in Memory**:
- Background tiles: ~100,000 nodes
- Mid-detail tiles: ~1,000,000 nodes
- High-detail LIDAR: ~5,000,000 nodes
- **Total**: ~6,100,000 objects

**GC Behavior**:
- Mark-sweep pause: ~800-1200ms ❌ **CATASTROPHIC** (1+ second pause!)
- Minor GC pause: ~5-10ms ⚠️ **Marginal** (still fits in budget)
- Major GC pause: ~800-1200ms (run during loading/transition)

**Verdict**: Generational GC **absolutely required**!

---

## Memory Footprint Estimates

### Per-Object Memory

```cpp
struct LODNode : gc_object {
    gc_header header;       // 48 bytes (gc_box overhead)
    LODNode* children[4];   // 32 bytes (quadtree)
    BoundingBox bbox;       // 24 bytes (spatial extents)
    VertexBuffer* mesh;     // 8 bytes (GPU data pointer)
    uint32_t lod_level;     // 4 bytes
    uint32_t flags;         // 4 bytes
    // Total: ~120 bytes per node (rounded up for alignment)
};
```

**Memory per scenario**:
- Scenario 1 (61K objects): ~7.3 MB
- Scenario 2 (610K objects): ~73 MB
- Scenario 3 (6.1M objects): ~732 MB

**Generational GC tuning**:
- Young generation: 10-20 MB (enough for ~100K temp LOD nodes)
- Old generation: 100-800 MB (persistent scene structure)

---

## Performance Testing Strategy

### Phase 1: Baseline (Current Mark-Sweep GC)

**Test**: Measure pause times at different scales

**Benchmark 1**: Small scene (10K objects)
```
Setup: Create 10,000 LOD nodes in quadtree
Action: Trigger GC 100 times
Measure: Min/max/avg pause time
Expected: ~2-5ms avg
```

**Benchmark 2**: Medium scene (100K objects)
```
Setup: Create 100,000 LOD nodes
Action: Trigger GC 100 times
Measure: Min/max/avg pause time
Expected: ~20-40ms avg ❌ (exceeds frame budget)
```

**Benchmark 3**: Large scene (1M objects)
```
Setup: Create 1,000,000 LOD nodes
Action: Trigger GC 100 times
Measure: Min/max/avg pause time
Expected: ~200-400ms avg ❌ (catastrophic)
```

**Benchmark 4**: LOD churn (allocation/deallocation)
```
Setup: 100K persistent nodes
Action: Allocate 10K temp nodes, navigate (triggers GC), repeat 100x
Measure: Pause time distribution, allocation rate
Expected: High churn → frequent GCs → bad for mark-sweep
```

---

### Phase 2: Generational GC Implementation

**Milestone 1**: Young generation allocator (week 1)
- Bump-pointer allocation in nursery
- Copy-collection for minor GC
- Promotion to old generation

**Milestone 2**: Write barrier (week 2)
- Cross-generation pointer tracking
- `gc_ref<T>` smart pointer for automatic barriers
- Remembered set for old→young pointers

**Milestone 3**: Tuning and benchmarks (week 3)
- Re-run all benchmarks with generational GC
- Tune generation sizes for different scenarios
- Measure minor/major GC pause distribution

**Expected results**:
- Minor GC: 2-5ms (90-95% of collections)
- Major GC: 80-400ms (5-10% of collections, during transitions)

---

## Recommended GC Configuration

### For Overview Scenario (< 100K objects)
**Strategy**: Current mark-sweep GC is **sufficient**!
**Reason**: 10-15ms pauses fit in frame budget (or barely exceed it)
**Optimization**: Use thread-local GC per view (already implemented)

---

### For City View Scenario (100K-1M objects)
**Strategy**: **Generational GC required**
**Configuration**:
- Young generation: 10 MB (~100K objects)
- Old generation: 100 MB (~1M objects)
- Minor GC trigger: Every 5 MB allocated
- Promotion threshold: 3 minor GC survivals

**Expected behavior**:
- 90% of GCs are minor (2-5ms)
- 10% of GCs are major (80-120ms, during navigation)

---

### For Maximum Detail Scenario (1M-10M objects)
**Strategy**: **Generational GC + Incremental Major GC**
**Configuration**:
- Young generation: 20 MB (~200K objects)
- Old generation: 1 GB (~10M objects)
- Minor GC trigger: Every 10 MB allocated
- Major GC: Incremental, spread over 100 frames

**Expected behavior**:
- Minor GC: 5-10ms (every 10-20 frames)
- Major GC increment: 1-2ms per frame (amortized over 100 frames)
- Total pause: Never exceeds 12ms ✅

---

## Allocation Patterns in LOD Scene Graph

### Pattern 1: Camera Navigation (High Churn)

**Scenario**: User pans camera across city
```
Frame 1:  Allocate 5K new LOD nodes (entering frustum)
          Deallocate 5K old nodes (leaving frustum)
Frame 2:  Allocate 5K new nodes, deallocate 5K old
...
Frame N:  Continuous churn
```

**GC Impact**:
- Mark-sweep: Must scan entire 610K node graph every GC ❌
- Generational: Only scan 5K young nodes per minor GC ✅

**Speedup**: ~100x faster minor GCs!

---

### Pattern 2: Zoom In (Refining Detail)

**Scenario**: User zooms from continent → city → building
```
Phase 1: Allocate coarse tiles (persistent, move to old gen)
Phase 2: Allocate medium tiles (persistent, move to old gen)
Phase 3: Allocate fine tiles (high churn, stay in young gen)
```

**GC Impact**:
- Old generation grows slowly (coarse tiles are reused)
- Young generation churns rapidly (fine tiles created/destroyed)
- Major GCs are rare (only when old gen grows significantly)

**Benefit**: Perfect fit for generational GC!

---

### Pattern 3: Multi-View Rendering (Thread-Local GCs)

**Scenario**: 4 simultaneous views (main + 3 thumbnail cameras)
```
Thread 1 (Main view):      610K objects, minor GC every 20 frames
Thread 2 (Thumbnail 1):    50K objects, minor GC every 50 frames
Thread 3 (Thumbnail 2):    50K objects, minor GC every 50 frames
Thread 4 (Thumbnail 3):    50K objects, minor GC every 50 frames
```

**GC Impact**:
- Each thread has independent GC (already implemented!)
- Minor GCs are staggered (don't all happen in same frame)
- Total pause budget shared across threads

**Benefit**: Thread-local GCs + generational = perfect combo!

---

## Performance Testing Harness

I'll create a benchmark suite that simulates these scenarios. It will:

1. **Generate realistic LOD hierarchies** (quadtree with bounding boxes)
2. **Simulate camera navigation** (allocate/deallocate patterns)
3. **Measure GC pause times** at different scales
4. **Compare mark-sweep vs generational** (after implementation)
5. **Output CSV data** for graphing in Excel/Python

Would you like me to implement this benchmark suite now?

---

## Immediate Next Steps

### Option A: Benchmark Current GC (1-2 days)
1. Create LOD node benchmark classes
2. Implement realistic allocation patterns
3. Measure pause times at 10K, 100K, 1M objects
4. Generate performance report

**Benefit**: Know exactly when generational GC becomes necessary

---

### Option B: Implement Generational GC (2-3 weeks)
1. Week 1: Young generation allocator + copy collector
2. Week 2: Write barrier + promotion logic
3. Week 3: Benchmark and tune

**Benefit**: Ready for production at any scale

---

### Option C: Do Both (Recommended!)
1. **This week**: Benchmark current GC (establishes baseline)
2. **Next 3 weeks**: Implement generational GC
3. **Week 4**: Re-benchmark, compare results, tune

**Benefit**: Data-driven development with clear before/after metrics

---

## Expected Benchmark Results

### Current Mark-Sweep GC

| Scene Size | Objects | Pause Time | Verdict |
|------------|---------|------------|---------|
| Small | 10K | 2-5ms | ✅ Acceptable |
| Medium | 100K | 20-40ms | ❌ Too slow |
| Large | 1M | 200-400ms | ❌ Catastrophic |

### After Generational GC

| Scene Size | Objects | Minor GC | Major GC | Verdict |
|------------|---------|----------|----------|---------|
| Small | 10K | 1-2ms | 5-10ms | ✅ Excellent |
| Medium | 100K | 2-5ms | 80-120ms | ✅ Good |
| Large | 1M | 5-10ms | 200-400ms | ✅ Acceptable* |

*Major GCs run during loading/navigation (user doesn't notice)

---

## Questions to Guide Implementation

### 1. What's your target frame rate?
- 60 FPS (16.67ms budget)?
- 30 FPS (33.33ms budget)?
- VR 90 FPS (11.11ms budget)? ⚠️

### 2. How often do you navigate?
- Continuous panning (high LOD churn)?
- Occasional jumps (low churn)?
- Will affect GC frequency

### 3. How many simultaneous views?
- Single view (1 GC instance)?
- Multi-view (4+ GC instances)?
- Will affect thread-local GC benefit

### 4. When can you tolerate pauses?
- During loading screens (unlimited time)?
- During camera transitions (50-100ms OK)?
- Never (must use incremental GC)?

---

## Recommendation

Given your requirements:
- **Earth-to-cm scale**: You'll hit 1M+ objects at max detail
- **Design phase**: Need data to make informed decisions
- **Generational GC desired**: Perfect fit for LOD pattern

**I recommend**:

1. **This session**: Implement performance benchmark suite (2-3 hours)
2. **Run benchmarks**: Establish baseline for current mark-sweep GC
3. **Analyze results**: See exactly where it breaks down
4. **Next session**: Start generational GC implementation with data-driven tuning

Shall I create the benchmark suite now?

---

*End of Document*
