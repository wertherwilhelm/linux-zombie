# GarbageCollector Performance Benchmark Guide

## Overview

The performance benchmark suite simulates realistic LOD scene graph patterns for Earth-to-centimeter scale LIDAR visualization. It tests the current mark-sweep GC at various scales to establish baseline performance before implementing generational GC.

---

## Quick Start

### 1. Build the test suite
```bash
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" \
  ConsoleApplication2.sln /p:Configuration=Debug /p:Platform=x64 /t:GarbageCollectorTest /maxcpucount:1
```

### 2. Run all benchmarks (takes ~2-5 minutes)
```bash
./x64/Debug/GarbageCollectorTest.exe --gtest_filter="*RunFullSuite"
```

###  3. View results
Results are saved to `gc_benchmark_results.csv` in the working directory.

---

## Benchmark Tests

### Test 1: Static Scene (No Churn)
**Purpose**: Measure GC pause times for scenes of different sizes with no allocation/deallocation.

**Scenarios**:
- 10K objects (continent view, zoomed out)
- 100K objects (city view, street level)
- 1M objects (maximum detail, cm-level LIDAR)

**Expected Results**:
- 10K: ~2-5ms pauses ✅ (acceptable for 60 FPS)
- 100K: ~20-40ms pauses ❌ (exceeds 16.67ms frame budget)
- 1M: ~200-400ms pauses ❌ (catastrophic, needs generational GC)

---

### Test 2: High Churn (Camera Navigation)
**Purpose**: Simulate camera panning with constant LOD creation/destruction.

**Scenarios**:
- 10K persistent + 1K churn per frame
- 100K persistent + 5K churn per frame

**Expected Results**:
- With mark-sweep: Must scan entire heap every GC (slow)
- With generational: Only scan young generation (100x faster!)

---

### Test 3: Deep Hierarchy (Quadtree)
**Purpose**: Test GC performance on realistic spatial subdivision structures.

**Scenarios**:
- Depth 3: 85 nodes (very coarse)
- Depth 5: 1,365 nodes (coarse)
- Depth 7: 21,845 nodes (medium detail)

**Expected Results**:
- Iterative marking should handle deep trees without stack overflow
- Pause time scales with total nodes, not depth

---

## Running Individual Benchmarks

```bash
# Small scene only (fast, ~10 seconds)
./x64/Debug/GarbageCollectorTest.exe --gtest_filter="*SmallScene" --gtest_also_run_disabled_tests

# Medium scene only (~30 seconds)
./x64/Debug/GarbageCollectorTest.exe --gtest_filter="*MediumScene" --gtest_also_run_disabled_tests

# Large scene only (WARNING: May take 1-2 minutes!)
./x64/Debug/GarbageCollectorTest.exe --gtest_filter="*LargeScene" --gtest_also_run_disabled_tests

# High churn test (~1 minute)
./x64/Debug/GarbageCollectorTest.exe --gtest_filter="*HighChurn" --gtest_also_run_disabled_tests

# Deep hierarchy test (~30 seconds)
./x64/Debug/GarbageCollectorTest.exe --gtest_filter="*DeepHierarchy" --gtest_also_run_disabled_tests
```

---

## Interpreting Results

### CSV Output Format
```
Test,Objects,Collections,Min(ms),Avg(ms),Max(ms),Total(ms)
Static_10K,10000,100,2.1,2.5,3.2,250.0
Static_100K,100000,100,22.3,25.7,31.4,2570.0
...
```

### Key Metrics

**Min/Avg/Max Pause**:
- Min: Best case GC pause
- Avg: Typical pause time
- Max: Worst case (causes frame drops)

**Frame Budget Analysis**:
- 60 FPS: 16.67ms budget
- 30 FPS: 33.33ms budget
- VR 90 FPS: 11.11ms budget

**Decision Criteria**:
- Avg < budget: ✅ Current GC acceptable
- Avg > budget: ⚠️ Consider generational GC
- Max >> budget: ❌ Generational GC required

---

## Expected Benchm Mark Results (Current Mark-Sweep GC)

Based on typical hardware (modern CPU, 16GB RAM):

| Scenario | Objects | Avg Pause | Max Pause | Verdict |
|----------|---------|-----------|-----------|---------|
| Small | 10K | ~2-5ms | ~5-8ms | ✅ OK |
| Medium | 100K | ~20-40ms | ~50-80ms | ❌ Too slow |
| Large | 1M | ~200-400ms | ~500-800ms | ❌ Catastrophic |
| Churn (10K) | 11K | ~3-6ms | ~8-12ms | ✅ OK |
| Churn (100K) | 105K | ~25-50ms | ~60-100ms | ❌ Too slow |
| Quadtree (d=7) | 22K | ~5-10ms | ~12-18ms | ⚠️ Marginal |

---

## Recommendations Based on Results

### If Small Scene (< 50K objects)
**Use current mark-sweep GC**:
- Pause times acceptable for 60 FPS
- Thread-local GCs provide parallelism
- Generational GC not needed

### If Medium Scene (50K-500K objects)
**Implement generational GC**:
- Minor GCs: ~2-5ms (fits in budget)
- Major GCs: ~50-200ms (run during transitions)
- Estimated effort: 2-3 weeks

### If Large Scene (> 500K objects)
**Implement generational + incremental GC**:
- Minor GCs: ~3-8ms
- Major GCs: Incremental (spread over frames)
- Estimated effort: 3-4 weeks

---

## Visualizing Results

### Excel/Sheets
1. Open `gc_benchmark_results.csv`
2. Create bar chart of Avg(ms) by Test
3. Add horizontal line at 16.67ms (frame budget)
4. Anything above the line needs generational GC

### Python (matplotlib)
```python
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('gc_benchmark_results.csv')

fig, ax = plt.subplots(figsize=(12, 6))
ax.bar(df['Test'], df['Avg(ms)'])
ax.axhline(y=16.67, color='r', linestyle='--', label='60 FPS Budget')
ax.set_xlabel('Test')
ax.set_ylabel('Avg GC Pause (ms)')
ax.set_title('GC Performance vs Frame Budget')
ax.legend()
plt.xticks(rotation=45, ha='right')
plt.tight_layout()
plt.savefig('gc_performance.png')
plt.show()
```

---

## Next Steps After Benchmarking

### Option A: Current GC is Sufficient
If all scenarios meet frame budget:
- ✅ Use current mark-sweep GC
- ✅ You're done! No further work needed.

### Option B: Implement Generational GC
If medium/large scenes exceed budget:
1. Read `CONCURRENT_GC_DESIGN.md` for implementation plan
2. Implement young generation allocator (week 1)
3. Implement copy-collection minor GC (week 2)
4. Add write barriers and tuning (week 3)
5. Re-run benchmarks to validate improvement

### Option C: Hybrid Approach
If only large scenes exceed budget:
- Use current GC for small/medium views
- Defer large scene support until generational GC is ready
- Implement generational GC in phases

---

## Troubleshooting

### Benchmark Takes Too Long
- Run individual tests instead of full suite
- Reduce `num_collections` parameter in benchmark code
- Skip the 1M object test (most time-consuming)

### Out of Memory Errors
- Close other applications
- Reduce test sizes in `performance_benchmarks.cpp`
- Run benchmarks one at a time

### Inconsistent Results
- Close background applications (reduces CPU contention)
- Run benchmarks multiple times, average results
- Disable CPU frequency scaling if available

---

## Benchmark Implementation Details

### LODTile Structure
```cpp
struct LODTile : gc_object {
    LODTile* children[4];    // Quadtree children
    BoundingBox bbox;        // Spatial extent
    uint32_t lod_level;      // 0 (planet) → 6 (cm detail)
    uint32_t tile_x, tile_y; // Morton code coordinates
    uint32_t flags;          // Visibility, loading, etc.
    void* vertex_buffer;     // GPU mesh pointer
    // Total: ~120 bytes per node
};
```

### Realistic Scene Patterns
1. **Static Scene**: All objects allocated upfront, rooted via tree structure
2. **High Churn**: Persistent base + temp nodes created/destroyed each frame
3. **Deep Hierarchy**: Quadtree subdivision with 4^depth total nodes

### Timing Methodology
- Uses `std::chrono::high_resolution_clock` for microsecond precision
- Measures only GC pause time (not allocation time)
- Statistics: min/max/avg over multiple collections

---

## Contact / Questions

If benchmark results are unexpected or you need help interpreting data:
1. Check console output for detailed stats
2. Review `gc_benchmark_results.csv` for raw data
3. Consult `SCALING_ANALYSIS.md` for expected ranges
4. Consult `CONCURRENT_GC_DESIGN.md` for GC options

---

*Last Updated*: December 31, 2025
*Benchmark Version*: 1.0
*Target Application*: Earth-to-Centimeter LIDAR Scene Graph
