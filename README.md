# zero_alloc_lob
 
A **zero-allocation, cache-friendly Limit Order Book (LOB)** in C++17, built step by step from a textbook `std::map` baseline to a design with **no heap allocation on the hot path** and a **median add/cancel latency of ~60–70 ns**.

Each optimisation stage is kept in its own folder, together with its benchmark results, so you can see what every change bought (and, in one case, what it cost).

## Presentation
For detailed journey and explanation of building this order book check presentation folder.

## Repository layout
 
```
zero_alloc_lob/
|-- 1_sstl_base/                                  # Stage 1: std::map baseline + results
|-- 2_pool_allocator_added_map_removed/           # Stage 2: pool allocator, flat limits, hierarchical bitmask
|-- 3_replaced_unordered_map_with_flat_hash_map/  # Stage 3: absl::flat_hash_map
|-- 4_FINAL/                                      # Stage 4: flattened bitmask (final version)
|-- Presentation/                                 # slides walking through the whole process
|
|-- pool_allocator.hpp                # free-list pool allocator
|-- heirarchical_bit_mask.hpp         # pointer-linked hierarchical bitmask
|-- heriarchical_bit_mask_flat.hpp    # flattened (single-array) hierarchical bitmask
|-- heirarchichal_bit_mask_iter.hpp   # bitmask variant with iteration support
|-- LOB_pre_alloc.hpp                 # pre-allocated LOB
|-- lob_pre_alloc_inter.hpp           # pre-allocated LOB (bitmask-iterator variant)
|-- Limit_order_book_naive.cpp        # naive baseline LOB
|-- LOB_selph_tree_dynamic.cpp        # tree-based, dynamically allocated LOB
|-- benchmark_bitset.cpp              # bitmask microbenchmark (flat vs hierarchical)
|-- main.cpp                          # workload driver / benchmark harness
|-- test.cpp                          # correctness tests
|-- plot_bitset.ipynb                 # plots for the bitmask benchmark
|-- visualize.ipynb                   # latency percentile / histogram plots
```
 
---
 
## Build and run
 
**Requirements**
 
- A C++17 compiler (GCC 9+ or Clang 10+)
- [Abseil](https://abseil.io/) (for `absl::flat_hash_map`, Stage 3 onward)
- Python 3 with `pandas`, `matplotlib`, `jupyter` (only for the plotting notebooks)
**Build** (adjust paths to your Abseil install):
 
```bash
# Stage 4 (final)
g++ -std=c++17 -O3 -march=native main.cpp -o lob \
    $(pkg-config --cflags --libs absl_hash absl_raw_hash_set)
 
# Stage 1 baseline (no Abseil needed)
g++ -std=c++17 -O3 -march=native Limit_order_book_naive.cpp -o lob_naive
```
 
**Run**
 
```bash
./lob
```
 
<!-- TODO: add the exact command line, input arguments, and the output files produced (e.g. percentile.txt) -->
 
**Plot results**
 
Open `visualize.ipynb` to reproduce the percentile bar chart, the latency-over-time trace, and the linear / log histograms. Open `plot_bitset.ipynb` for the bitmask comparison.
 
---
 
## Benchmark methodology

- **Processor** i5 12450H
- **Workload:** about 1 million add and cancel operations per run.
- **Metrics:** P50, P75, P90, P95, P99, P99.9 and P99.99 per operation type.
- **Plots per stage:** percentile bar chart, rolling-mean latency over operation sequence, and latency histograms (linear and log-Y, clipped at the 99.99th percentile).
- **Bitmask microbenchmark:** `benchmark_bitset.cpp` reports activate / deactivate / find-leftmost percentiles in **CPU cycles** for flat vs hierarchical.

  ## Known limitation
 
- **Not yet isolated.** Numbers were measured without CPU isolation, core pinning, or frequency locking, so tails (P99.9+) include OS noise. Isolated benchmarking is the first item on the roadmap.
