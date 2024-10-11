
#include <cstddef>
#include <tbb/tbb.h>
#include <iostream>
#include <vector>
#include <algorithm> // for std::swap and std::sort
#include <chrono> 
#include "../parallel_for.h"
// Partition function used in quicksort
static int cnt = 0;
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
void ism_quicksort(std::vector<int>& arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);

        // Recursively sort elements before and after partition
        if (high - low > 10000) {  // Threshold for parallel execution
            parallel_do(
                [&]() { ism_quicksort(arr, low, pi - 1); }, 
                [&]() { ism_quicksort(arr, pi + 1, high); }
            );
        } else {
            // If the array is small, use serial quicksort to avoid overhead
            ism_quicksort(arr, low, pi - 1);
            ism_quicksort(arr, pi + 1, high);
        }    }
}
// Parallel quicksort function using TBB
void tbb_quicksort(std::vector<int>& arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);

        // Recursively sort elements before and after partition
        if (high - low > 10000) {  // Threshold for parallel execution
            tbb::parallel_invoke(
                [&]() { tbb_quicksort(arr, low, pi - 1); }, 
                [&]() { tbb_quicksort(arr, pi + 1, high); }
            );
        } else {
            // If the array is small, use serial quicksort to avoid overhead
            tbb_quicksort(arr, low, pi - 1);
            tbb_quicksort(arr, pi + 1, high);
        }
    }
}

// Function to verify if the array is correctly sorted
bool verify_sorted(const std::vector<int>& arr1, const std::vector<int>& arr2) {
    return arr1 == arr2; // Returns true if both arrays are identical
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
    tbb_quicksort(arr, 0, n - 1);

    // Stop measuring time
    auto end = std::chrono::high_resolution_clock::now();
    // Calculate the elapsed time in milliseconds
    std::chrono::duration<double> diff = end - start;


    //std::cout << "Elapsed time: " << elapsed.count() << " s" << std::endl;
    auto time_tbb = diff.count();
    start = std::chrono::high_resolution_clock::now();

    // Perform parallel quicksort
    //ism_quicksort(arr_copy, 0, n - 1);

    // Stop measuring time
    end = std::chrono::high_resolution_clock::now();
    diff  = end - start;
    auto time_ism = diff.count();

    //std::cout <<time_ism << std::endl;

    
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
