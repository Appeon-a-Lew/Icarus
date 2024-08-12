#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <sycl/sycl.hpp>
#include <chrono>
#include <unistd.h>
#include <vector>
#include <random>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>
#include <tbb/global_control.h>
#include <cmath>
#include <tbb/tbb.h>
#include "schedule.hpp"  

const int MIN_ARRAY_SIZE = 7e7;
const int MAX_ARRAY_SIZE = 2e8;
const int NUM_RUNS = 5;

void heavy_computation(int& x) {
  double tmp = 0;
  for (size_t i = 0; i < x ;i++) {

      tmp += sin(x) * cos(x) + sqrt(x);
  }
  x = static_cast<int>(tmp) % 1000;
}

std::vector<int> initialize_data(int size) {
    std::vector<int> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);  
    for (auto& x : data) {
        x = dis(gen);
    }
    return data;
}

double run_sequential(std::vector<int>& data) {
    auto start = std::chrono::high_resolution_clock::now();

    for (auto& x : data) {
        heavy_computation(x);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    return diff.count();
}

double run_sycl(std::vector<int>& data) {
    sycl::queue queue{sycl::default_selector_v};
    sycl::range<1> range(data.size());

    sycl::buffer<int, 1> data_buf(data.data(), sycl::range<1>(data.size()));

    auto start = std::chrono::high_resolution_clock::now();

    queue.submit([&](sycl::handler& cgh) {
        auto data_acc = data_buf.get_access<sycl::access::mode::read_write>(cgh);

        cgh.parallel_for<class heavy_jobs>(range, [=](sycl::id<1> idx) {
          });
    });

    queue.wait();

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = end - start;
    return diff.count();
}

double run_tbb_simple(std::vector<int>& data) {
   
    tbb::concurrent_vector<double> latencies;
  
    auto start = std::chrono::high_resolution_clock::now();
    tbb::parallel_for_each(data.begin(), data.end(),
        [&latencies,start](int& x) {
  #ifdef aaaa 
            auto tast_start = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> latency = tast_start - start;
            latencies.push_back(latency.count());
  #endif 
            heavy_computation(x);
        });

    auto end = std::chrono::high_resolution_clock::now();


    std::chrono::duration<double> diff = end - start;
  
  #ifdef aaaa
    auto min_latency = *std::min_element(latencies.begin(), latencies.end());
    auto max_latency = *std::max_element(latencies.begin(), latencies.end());
    double avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();

    std::sort(latencies.begin(), latencies.end());
    double median_latency = latencies.size() % 2 == 0
        ? (latencies[latencies.size() / 2 - 1] + latencies[latencies.size() / 2]) / 2.0
        : latencies[latencies.size() / 2];

    // Print out the results
    std::cout << "Average scheduling latency: " << avg_latency << " seconds" << std::endl;
    std::cout << "Median scheduling latency: " << median_latency << " seconds" << std::endl;
    std::cout << "Minimum scheduling latency: " << min_latency << " seconds" << std::endl;
    std::cout << "Maximum scheduling latency: " << max_latency << " seconds" << std::endl;
    std::cout << "NUMBER OF JOBS:" << latencies.size() << "\n";
  #endif 
    return diff.count();
}

static tbb::affinity_partitioner ap;

double run_tbb_blocked(std::vector<int>& data) {
    auto start = std::chrono::high_resolution_clock::now();

    tbb::parallel_for(tbb::blocked_range<size_t>(0, data.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i) {
                heavy_computation(data[i]);
            }
        });

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    return diff.count();
}

double run_maxis_scheduler(std::vector<int>& data, auto cfg) {
    auto start = std::chrono::high_resolution_clock::now();
    sched::parallel_for(sched::range(0, data.size()), [&](const sched::range& r) {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            heavy_computation(data[i]);
        }
    },cfg);
    //sched::parallel_for_each(data.begin(), data.end(), [](int& r) {
    //   heavy_computation(r);
    //}); 
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    return diff.count();
}

int main() {
    sched::INSTANCE = std::make_unique<sched::scheduler_t>();

    std::cout << "Comparing different parallel execution strategies\n";
    std::cout << "Number of runs: " << NUM_RUNS << "\n\n";

    double total_time_tbb_simple = 0;
    double total_time_tbb_blocked = 0;
    double total_time_maxis = 0;
    double total_time_sycl = 0;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(MIN_ARRAY_SIZE, MAX_ARRAY_SIZE);

    for (int i = 0; i < NUM_RUNS; ++i) {
        int array_size = size_dist(gen);
        std::cout << "Run " << i + 1 << " with array size: " << array_size << "\n";
        auto data = initialize_data(array_size);
        auto _data = data;
       
        std::vector<size_t> morsel_hints(sched::thread_count());
        std::uniform_int_distribution<> hint_dist(0, array_size);
        for (unsigned j = 0; j < (sched::thread_count()); ++j) {
          
            morsel_hints[j] = hint_dist(gen);
        
            std::cout << morsel_hints[j] << ", ";
        }
        std::cout << std::endl; 
        std::sort(morsel_hints.begin(), morsel_hints.end());
        morsel_hints[0] = 0;
        morsel_hints[morsel_hints.size()-1] = array_size;
        MaxisScheduler::RuntimeConfig config;
        config.morsel_hints = libdb::RefOrInstance<std::vector<size_t>>(std::move(morsel_hints));

        
        double time_maxis = run_maxis_scheduler(_data, config);
        total_time_maxis += time_maxis;
        std::cout << "  MaxisScheduler:" << time_maxis << " seconds\n";
        _data = data;
        double time_sycl = run_sycl(_data);
        total_time_sycl += time_sycl;
        std::cout << "  SYCL:          " << time_sycl << " seconds\n";
        _data = data;
        double time_tbb_blocked = run_tbb_blocked(_data);
        total_time_tbb_blocked += time_tbb_blocked;
        std::cout << "  TBB Blocked:   " << time_tbb_blocked << " seconds\n";
 
        _data = data;
        double time_tbb_simple = run_tbb_simple(_data);
        total_time_tbb_simple += time_tbb_simple;
        std::cout << "  TBB Simple:    " << time_tbb_simple << " seconds\n";
        
  }

    std::cout << "\nAverage times:\n";
    std::cout << "  TBB Simple:    " << total_time_tbb_simple / NUM_RUNS << " seconds\n";
    std::cout << "  TBB Blocked:   " << total_time_tbb_blocked / NUM_RUNS << " seconds\n";
    std::cout << "  MaxisScheduler:" << total_time_maxis / NUM_RUNS << " seconds\n";
    std::cout << "  SYCL:          " << total_time_sycl / NUM_RUNS << " seconds\n";

    return 0;
}

