#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <climits>
#include <thread>
#include <iomanip>
#include <windows.h>


#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

// Toggle implementation based on compiler flag
#ifdef USE_INTER_ALLOC
    #include "LOB_pre_alloc_inter.hpp"
    const char* OUT_FILE_NAME = "pool_allocator_lob_latencies.csv";
#else
    #include "LOB_pre_alloc.hpp"
    const char* OUT_FILE_NAME = "lob_latencies.csv";
#endif

enum class OpType { ADD, CANCEL }; 

struct LOBEvent {
    OpType type;
    int order_id;
    int price;
    bool is_buy;
    int quantity;
};

struct PerfRecord {
    OpType type;
    uint64_t cycles;
    size_t map_size;
    size_t map_capacity;
    float load_factor;
};

// Calibrate the TSC to find nanoseconds per cycle
double calibrate_tsc() {
    std::cout << "Calibrating CPU TSC frequency..." << std::endl;
    unsigned int aux;
    
    auto chrono_start = std::chrono::high_resolution_clock::now();
    uint64_t tsc_start = __rdtsc();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    uint64_t tsc_end = __rdtscp(&aux);
    auto chrono_end = std::chrono::high_resolution_clock::now();
    
    auto ns_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(chrono_end - chrono_start).count();
    uint64_t cycles_elapsed = tsc_end - tsc_start;
    
    double ns_per_cycle = static_cast<double>(ns_elapsed) / cycles_elapsed;
    std::cout << "Calibration done: " << ns_per_cycle << " ns/cycle\n" << std::endl;
    
    return ns_per_cycle;
}

void run_benchmark() {
    SetThreadAffinityMask(GetCurrentThread(), 1 << 2);
    double ns_per_cycle = calibrate_tsc();

    int prev_close = 100000;      
    int max_change = 20;         
    int tick_size = 1;           
    
    // ARCHITECTURE FIX: Decouple Pool Capacity from Benchmark Iterations
    int pool_capacity = 100000;       // Max resting orders (fits comfortably in L3 cache)
    int target_resting = 75000;       // Realistic active book depth
    
    int warmup_volume = 50000;        // Operations to warm up instruction cache
    int benchmark_volume = 1000000;   // 1M benchmark operations
    int total_events = warmup_volume + benchmark_volume;

    // The Book now only allocates memory for 100k orders, NOT 1+ million.
    Book book(prev_close, max_change, tick_size, pool_capacity);

    std::mt19937 gen(42); 
    std::uniform_int_distribution<> price_dist(prev_close * 0.99, prev_close * 1.01);
    std::uniform_int_distribution<> qty_dist(1, 500);
    std::uniform_int_distribution<> side_dist(0, 1);
    std::uniform_int_distribution<> op_dist(1, 100);

    std::vector<int> active_orders; 
    active_orders.reserve(pool_capacity);
    int next_order_id = 1;

    // ==========================================
    // PHASE 1: PRE-FILL THE BOOK (Un-timed)
    // ==========================================
    std::cout << "Pre-filling book with " << target_resting << " resting orders..." << std::endl;
    for (int i = 0; i < target_resting; ++i) {
        book.add(next_order_id, price_dist(gen), (bool)side_dist(gen), qty_dist(gen));
        active_orders.push_back(next_order_id);
        next_order_id++;
    }

    // ==========================================
    // PHASE 2: GENERATE EVENTS
    // ==========================================
    std::cout << "Generating " << total_events << " benchmark events..." << std::endl;
    std::vector<LOBEvent> events;
    events.reserve(total_events);
    
    for (int i = 0; i < total_events; ++i) {
        int op_roll = op_dist(gen);
        
        // BOUNDED RANDOM WALK: Keep the active order count hovering around 75,000.
        // Prevent it from hitting the 100k pool limit or draining to 0.
        bool force_add = active_orders.empty() || active_orders.size() < (target_resting - 10000);
        bool force_cancel = active_orders.size() >= (pool_capacity - 100);

        if (force_add || (!force_cancel && op_roll <= 50)) {
            // ADD EVENT
            events.push_back({OpType::ADD, next_order_id, price_dist(gen), (bool)side_dist(gen), qty_dist(gen)});
            active_orders.push_back(next_order_id);
            next_order_id++;
        } else {
            // CANCEL EVENT
            std::uniform_int_distribution<> active_dist(0, active_orders.size() - 1);
            int idx = active_dist(gen);
            int target_id = active_orders[idx];

            // Fast erase from vector
            active_orders[idx] = active_orders.back();
            active_orders.pop_back();

            events.push_back({OpType::CANCEL, target_id, 0, false, INT_MAX});
        }
    }

    // ==========================================
    // PHASE 3: WARM-UP & OS PAGING
    // ==========================================
    std::cout << "Executing " << warmup_volume << " warm-up operations (un-timed)..." << std::endl;
    for (int i = 0; i < warmup_volume; ++i) {
        const auto& ev = events[i];
        if (ev.type == OpType::ADD) {
            book.add(ev.order_id, ev.price, ev.is_buy, ev.quantity);
        } else {
            book.cancel(ev.order_id, ev.quantity);
        }
    }

    std::cout << "Forcing OS memory paging for all price levels..." << std::endl;
    for(int p = prev_close * 0.80; p <= prev_close * 1.20; p += tick_size) {
        book.add(9999999, p, true, 1);
        book.cancel(9999999, 1);
        book.add(9999999, p, false, 1);
        book.cancel(9999999, 1);
    }
    
    // ==========================================
    // PHASE 4: BENCHMARK
    // ==========================================
    std::vector<PerfRecord> results;
    results.reserve(benchmark_volume);

    std::cout << "Executing " << benchmark_volume << " benchmark operations (recording RDTSC)..." << std::endl;
    unsigned int aux;
    uint64_t start_cycles, end_cycles;

    for (int i = warmup_volume; i < total_events; ++i) {
        const auto& ev = events[i];
        
        switch (ev.type) {
            case OpType::ADD:
                _mm_lfence(); // Serialize CPU instruction pipeline
                start_cycles = __rdtsc();
                _mm_lfence(); // Ensure timer read finishes before logic executes
                
                book.add(ev.order_id, ev.price, ev.is_buy, ev.quantity);
                
                end_cycles = __rdtscp(&aux);
                break;
            case OpType::CANCEL:
                _mm_lfence(); 
                start_cycles = __rdtsc();
                _mm_lfence(); 
                
                book.cancel(ev.order_id, ev.quantity);
                
                end_cycles = __rdtscp(&aux);
                break;
        }
        
        // Record hash map metrics AFTER the timer stops to avoid polluting the benchmark
        results.push_back({
            ev.type, 
            end_cycles - start_cycles,
            book.get_map_size(),
            book.get_map_capacity(),
            book.get_map_load_factor()
        });
    }
    
    // ==========================================
    // PHASE 5: EXPORT RESULTS
    // ==========================================
    std::cout << "Writing results to " << OUT_FILE_NAME << "..." << std::endl;
    std::ofstream out_file(OUT_FILE_NAME);
    out_file << "operation,cycles,nanoseconds,map_size,map_capacity,load_factor\n";
    out_file << std::fixed << std::setprecision(2);
    
    for (const auto& res : results) {
        const char* op_str = (res.type == OpType::ADD) ? "ADD" : "CANCEL";
        double ns = res.cycles * ns_per_cycle;
        
        out_file << op_str << "," 
                 << res.cycles << "," 
                 << ns << ","
                 << res.map_size << ","
                 << res.map_capacity << ","
                 << res.load_factor << "\n";
    }
    
    out_file.close();
    std::cout << "Done. Run the python script to plot the clean data." << std::endl;
}

int main() {
    run_benchmark();
    return 0;
}