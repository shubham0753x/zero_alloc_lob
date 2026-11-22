#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <x86intrin.h>
#include "pool_allocator.hpp"
#include "absl/container/flat_hash_map.h"
#include "heriarchical_bit_mask_flat.hpp"

// Placeholder for your Order class if needed by pool
class Order { int data[8]; }; 

double get_nanoseconds_per_cycle() {
    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t start_cycles = __rdtsc();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
    uint64_t end_cycles = __rdtsc();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto ns_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    return static_cast<double>(ns_elapsed) / (end_cycles - start_cycles);
}

int main() {
    double ns_per_cycle = get_nanoseconds_per_cycle();
    int iterations = 1000000;
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<> idx_dist(0, 10000); // 10,000 price levels
    std::uniform_int_distribution<> id_dist(1, 5000000);

    // Data structures to test
    PoolAllocator<Order> pool(iterations + 10);
    absl::flat_hash_map<int, Order*> map;
    map.reserve(iterations); // Prevent resizing from skewing data
    for(int i = 0;i<iterations;i++){
            map[i] = nullptr;
        }
        map.clear();
    Flat_heirarchical_bit_mask mask(10000);
    
    std::ofstream out("latencies_functions.csv");
    out << "Function,Latency_ns\n";

    // 1. Benchmark Memory Pool Allocation
    std::vector<Order*> allocated;
    allocated.reserve(iterations);
    for (int i = 0; i < iterations; ++i) {
        uint64_t start = __rdtsc();
        Order* o = pool.allocate();
        uint64_t end = __rdtsc();
        allocated.push_back(o);
        out << "Pool_Allocate," << (end - start) * ns_per_cycle << "\n";
    }

    // 2. Benchmark Hash Map Insert (Cache Miss Simulator)
    for (int i = 0; i < iterations; ++i) {
        int order_id = id_dist(gen);
        Order* ptr = allocated[i];
        
        uint64_t start = __rdtsc();
        map[order_id] = ptr;
        uint64_t end = __rdtsc();
        out << "HashMap_Insert," << (end - start) * ns_per_cycle << "\n";
    }

    // 3. Benchmark Bitmask Bottom-Up (Activate)
    for (int i = 0; i < iterations; ++i) {
        int idx = idx_dist(gen);
        
        uint64_t start = __rdtsc();
        mask.activate(idx);
        uint64_t end = __rdtsc();
        out << "Bitmask_Activate," << (end - start) * ns_per_cycle << "\n";
    }

    // 4. Benchmark Bitmask Top-Down (Read Best Price)
    for (int i = 0; i < iterations; ++i) {
        uint64_t start = __rdtsc();
        volatile int best = mask.get_most_right_active(); // volatile prevents compiler optimizing loop away
        uint64_t end = __rdtsc();
        out << "Bitmask_Read," << (end - start) * ns_per_cycle << "\n";
    }

    out.close();
    std::cout << "Function-wise benchmarking complete. Check latencies_functions.csv\n";
    return 0;
}