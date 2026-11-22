#include <iostream>
#include <vector>
#include <fstream>
#include <random>
#include <x86intrin.h>
#include "heirarchichal_bit_mask_iter.hpp"
#include "heriarchical_bit_mask_flat.hpp"
#include <windows.h>



inline uint64_t get_tsc_start() {
    _mm_lfence();
    return __rdtsc();
}
inline uint64_t get_tsc_end() {
    unsigned int aux;
    uint64_t tsc = __rdtscp(&aux);
    _mm_lfence();
    return tsc;
}
inline void do_not_optimize(int v) { asm volatile("" : : "g"(v) : "memory"); }

int main() {
    SetThreadAffinityMask(GetCurrentThread(), 1 << 2);

    const int NUM_ELEMENTS = 40000;
    const int NUM_OPS = 1000000;
    
    Flat_heirarchical_bit_mask flat_mask(NUM_ELEMENTS);
    HierarchicalMask hier_mask(NUM_ELEMENTS);

    // Pre-allocate results vectors
    std::vector<uint64_t> f_act(NUM_OPS, 0), f_deact(NUM_OPS, 0), f_left(NUM_OPS, 0), f_right(NUM_OPS, 0);
    std::vector<uint64_t> h_act(NUM_OPS, 0), h_deact(NUM_OPS, 0), h_left(NUM_OPS, 0), h_right(NUM_OPS, 0);
    std::vector<uint64_t> baseline(NUM_OPS, 0);

    // Pre-generate random indices
    std::vector<int> target_indices(NUM_OPS);
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dist(0, NUM_ELEMENTS - 1);
    for (int i = 0; i < NUM_OPS; ++i) target_indices[i] = dist(gen);

    std::cout << "Warming up caches..." << std::endl;
    for (int i = 0; i < 10000; ++i) {
        flat_mask.activate(target_indices[i]);
        hier_mask.activate(target_indices[i]);
        do_not_optimize(flat_mask.get_most_left_active());
        do_not_optimize(hier_mask.get_left_most_active());
        flat_mask.deactivate(target_indices[i]);
        hier_mask.deactivate(target_indices[i]);
    }

    uint64_t start, end;
    int dummy;

    std::cout << "Benchmarking Flat Mask..." << std::endl;
    for (int i = 0; i < NUM_OPS; ++i) {
        int idx = target_indices[i];
        
        start = get_tsc_start(); do_not_optimize(idx); end = get_tsc_end();
        baseline[i] = end - start;

        start = get_tsc_start(); flat_mask.activate(idx); end = get_tsc_end();
        f_act[i] = end - start;

        start = get_tsc_start(); dummy = flat_mask.get_most_left_active(); end = get_tsc_end();
        do_not_optimize(dummy); f_left[i] = end - start;

        start = get_tsc_start(); dummy = flat_mask.get_most_right_active(); end = get_tsc_end();
        do_not_optimize(dummy); f_right[i] = end - start;

        start = get_tsc_start(); flat_mask.deactivate(idx); end = get_tsc_end();
        f_deact[i] = end - start;
    }

    std::cout << "Benchmarking Hierarchical Mask..." << std::endl;
    for (int i = 0; i < NUM_OPS; ++i) {
        int idx = target_indices[i];
        
        start = get_tsc_start(); hier_mask.activate(idx); end = get_tsc_end();
        h_act[i] = end - start;

        start = get_tsc_start(); dummy = hier_mask.get_left_most_active(); end = get_tsc_end();
        do_not_optimize(dummy); h_left[i] = end - start;

        start = get_tsc_start(); dummy = hier_mask.get_right_most_active(); end = get_tsc_end();
        do_not_optimize(dummy); h_right[i] = end - start;

        start = get_tsc_start(); hier_mask.deactivate(idx); end = get_tsc_end();
        h_deact[i] = end - start;
    }

    std::cout << "Writing to comparison_latencies.csv..." << std::endl;
    std::ofstream out("comparison_latencies.csv");
    out << "baseline,f_act,f_left,f_right,f_deact,h_act,h_left,h_right,h_deact\n";
    for (int i = 0; i < NUM_OPS; ++i) {
        out << baseline[i] << "," << f_act[i] << "," << f_left[i] << "," << f_right[i] << "," << f_deact[i] << ","
            << h_act[i] << "," << h_left[i] << "," << h_right[i] << "," << h_deact[i] << "\n";
    }
    out.close();
    std::cout << "Done." << std::endl;
    return 0;
}