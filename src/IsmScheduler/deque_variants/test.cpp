
#include <iostream>
#include <vector>
#include "signalvariant.h"  // Assuming the provided scheduler code is in this header

#include <cstddef>
#include <tbb/tbb.h>
#include <iostream>
#include <vector>
#include <algorithm> // for std::swap and std::sort
#include <chrono> 
#include <iostream>
#include <vector>
#include <algorithm> // for std::swap and std::sort
#include <chrono> 
// Partition function used in quicksort
int partition(std::vector<int>& arr, int low, int high) {
    int pivot = arr[high]; // Choosing the last element as the pivot
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j] < pivot) {
            i++;
            std::swap(arr[i], arr[j]);
        }
    }
    std::swap(arr[i + 1], arr[high]);
    return i + 1;
}

// Parallel Quicksort using the work-stealing scheduler
void ism_quicksort(std::vector<int>& arr, int low, int high, parlay::fork_join_scheduler& scheduler) {
    if (low < high) {
        int pi = partition(arr, low, high);

        // Threshold for parallel execution (adjust as needed for performance)
        if (high - low > 10000) {
            // Recursively sort elements before and after partition in parallel
            scheduler.pardo(
                [&]() { ism_quicksort(arr, low, pi - 1, scheduler); },
                [&]() { ism_quicksort(arr, pi + 1, high, scheduler); }
            );
        } else {
            // Use serial quicksort for small subarrays to avoid parallel overhead
            ism_quicksort(arr, low, pi - 1, scheduler);
            ism_quicksort(arr, pi + 1, high, scheduler);
        }
    }
}


std::vector<int> generate_random_array(int n) {
    std::vector<int> arr(n);
    std::random_device rd; // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator
    std::uniform_int_distribution<> distrib(0, n*2); // Define the range

    for (int i = 0; i < n; ++i) {
        arr[i] = distrib(gen); // Generate numbers within the specified range
    }
    return arr;
}
int main() {
  std::vector<double> sizes = {1e4,1e5,1e6,1e7,1e8,1e9};

  parlay::fork_join_scheduler scheduler;  // Instantiate the scheduler
  std::cout << "Array size, TBB time, Hybrid Time\n";
  for(size_t i =0 ; i < sizes.size(); i++){
    size_t n = sizes[i];
    std::vector<int> arr = generate_random_array(n);
    //std::cout << "Generated the array\n\n";

    // Make a copy of the array for correctness check
    std::vector<int> arr_copy = arr;


    // Start measuring time
    auto start = std::chrono::high_resolution_clock::now();

    // Perform parallel quicksort
    //tbb_quicksort(arr, 0, n - 1);

    // Stop measuring time
    auto end = std::chrono::high_resolution_clock::now();
    // Calculate the elapsed time in milliseconds
    std::chrono::duration<double> diff = end - start;


    //std::cout << "Elapsed time: " << elapsed.count() << " s" << std::endl;
    auto time_tbb = diff.count();
    start = std::chrono::high_resolution_clock::now();

    // Perform parallel quicksort
    ism_quicksort(arr_copy, 0, n - 1,scheduler);

    // Stop measuring time
    end = std::chrono::high_resolution_clock::now();
    diff  = end - start;
    auto time_ism = diff.count();

    std::cout <<time_ism << std::endl;
    // Print profiling stats (CAS, fence, etc.)
    #ifdef profiling_stats
    long long total_cas = 0;
    long long total_fence = 0;
    long long total_c1 = 0;
    long long total_c2 = 0;
    long long total_c3 = 0;
    long long total_c4 = 0;
    
    for (const auto& deque : *scheduler.get_deques()) {  // Assuming you have a way to get access to the deques
        total_cas += deque.cas;
        total_fence += deque.fence;
        total_c1 += deque.c1;  // Pop bottom
        total_c2 += deque.c2;  // Steal
        total_c3 += deque.c3;  // Pop top
        total_c4 += deque.c4;  // Update bottom
    }

    std::cout << "Profiling stats:" << std::endl;
    std::cout << "CAS operations: " << total_cas << std::endl;
    std::cout << "Fence operations: " << total_fence << std::endl;
    std::cout << "Pop bottom (c1): " << total_c1 << std::endl;
    std::cout << "Steals (c2): " << total_c2 << std::endl;
    std::cout << "Pop top (c3): " << total_c3 << std::endl;
    std::cout << "Update bottom (c4): " << total_c4 << std::endl;
    #endif
    
    // Sort the copy using the standard library sort function for correctness check
    //std::sort(arr_copy.begin(), arr_copy.end());

    // Verify that the parallel quicksort result matches the standard sort
    //if (verify_sorted(arr, arr_copy)) {
    //    std::cout << "Correctness check passed!" << std::endl;
    //} else {
    //    std::cout << "Correctness check failed!" << std::endl;
    //}
  }
  return 0;
}   
