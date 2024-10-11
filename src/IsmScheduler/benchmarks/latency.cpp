#include <cstddef>
#include <iostream>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/partitioner.h>
#include <vector>
#include <chrono>
#include <algorithm>
#include <thread>
#include <tbb/parallel_for.h>
#include <tbb/task_group.h>
#include "../parallel_for.h"
#include "oneapi/tbb/blocked_range.h"
// Function to get current time in nanoseconds
uint64_t get_time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

int main() {
    // Get the number of cores
    int num_cores =    4 *  std::thread::hardware_concurrency();
    //std::cout << "Number of cores detected: " << num_cores << std::endl;

    // Create a vector to store the start times
    std::vector<uint64_t> start_times(num_cores, 0);

    // Get the start time of the entire operation
    uint64_t global_start = get_time_ns();

    // Create a task arena with the number of cores

    // Execute the parallel_for in the arena
    //tbb:: parallel_for(tbb::blocked_range<size_t>(0, num_cores), [&](const tbb::blocked_range<size_t> range) {
    // Record the start time for this task
    //  for( auto i = range.begin(); i != range.end(); i++){
    //    start_times[i] = get_time_ns() - global_start;

      //} 
    //});
    

    //parallel_for(0,num_cores,[&](int i) {
            // Record the start time for this task
    //        start_times[i] = get_time_ns() - global_start;
    //    },1,0);
    // Sort the start times
    //std::sort(start_times.begin(), start_times.end());

    // Print the results
    //std::cout << "Scheduling latencies (ns):" << std::endl;
    //std::cout << "The global_start:" << global_start << std::endl;
    //for (int i = 0; i < num_cores; ++i) {
    //   std::cout << "Task " << i << ": " << start_times[i] << " ns" << std::endl;
    //}

    // Calculate and print some statistics
    uint64_t total_latency = start_times.back() - start_times.front();
    double avg_latency = static_cast<double>(total_latency) / (num_cores - 1);
    //std::cout << start_times[0] << std::endl;
    //std::cout << "\nTotal scheduling latency: " << total_latency << " ns" << std::endl;
    //std::cout << "Average latency between task starts: " << avg_latency << " ns\n\n" << std::endl;
    static tbb::detail::d1::simple_partitioner simple;
    static tbb::detail::d1::affinity_partitioner apart;
    global_start = get_time_ns();
    

    tbb:: parallel_for(0, num_cores, [&](int& i){
    // Record the start time for this task
       start_times[i] = get_time_ns() - global_start;

    });
    std::sort(start_times.begin(), start_times.end());

    // Print the results
    //std::cout << "Scheduling latencies (ns):" << std::endl;
    for (int i = 0; i < num_cores; ++i) {
      //std::cout << "Task " << i << ": " << start_times[i] << " ns" << std::endl;
    }

    // Calculate and print some statistics
   total_latency = start_times.back() - start_times.front();
   avg_latency = static_cast<double>(total_latency) / (num_cores - 1);
    //std::cout << start_times[0] << std::endl;

    //std::cout << "\nTotal scheduling latency: " << total_latency << " ns" << std::endl;
    //std::cout << "Average latency between task starts: " << avg_latency << " ns\n\n" << std::endl;

    return 0;
}
