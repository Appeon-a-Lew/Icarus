
#include <cstddef>
#include <iostream>
#include <random>
#include <atomic>
#include <tbb/tbb.h>
#include <chrono>
#include <vector>
#include "../parallel_for.h"
using namespace std;
double calculate_pi_ism(long long num_points) {
    // Atomic variable to store the count of points inside the circle
    atomic<long long> points_inside_circle(0);

    // Parallel loop to generate points and check if they are inside the circle
    parallel_for_morsel(0, num_points, [&](const tbb::blocked_range<size_t>& r) {
        unsigned int seed = r.begin();  // Use the beginning of the range as the seed
        std::mt19937 gen(seed);
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        long long local_count = 0;
        for (long long i = r.begin(); i != r.end(); ++i) {
            double x = dist(gen);
            double y = dist(gen);
            if (x * x + y * y <= 1.0) {
                ++local_count;
            }
        }
        atomic_fetch_add(&points_inside_circle,local_count);  // Safely add local count to the global count
    },0,0);

    // Calculate the estimated value of Pi
    return 4.0 * points_inside_circle / static_cast<double>(num_points);
}

// Function to perform Monte Carlo simulation in parallel using TBB's parallel_for
double calculate_pi(long long num_points) {
    // Atomic variable to store the count of points inside the circle
    atomic<long> points_inside_circle(0);

    // Parallel loop to generate points and check if they are inside the circle
    tbb::parallel_for(tbb::blocked_range<long long>(0, num_points), [&](const tbb::blocked_range<long long>& r) {
        unsigned int seed = r.begin();  // Use the beginning of the range as the seed
        std::mt19937 gen(seed);
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        long long local_count = 0;
        for (long long i = r.begin(); i != r.end(); ++i) {
            double x = dist(gen);
            double y = dist(gen);
            if (x * x + y * y <= 1.0) {
                ++local_count;
            }
        }
        atomic_fetch_add(&points_inside_circle,local_count);  // Safely add local count to the global count
    });

    // Calculate the estimated value of Pi
    return 4.0 * points_inside_circle / static_cast<double>(num_points);
}

int main() {
    std::vector<double> sizes = {1e5,5e5,1e6,5e6,1e7,5e7,1e8,5e8,1e9};
    for(size_t i = 0; i < sizes.size(); ++i  ){
    // Calculate Pi using Monte Carlo method in parallel
    size_t num_points = sizes[i];
    auto start = std::chrono::high_resolution_clock::now(); 
    double pi = calculate_pi(num_points);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    //cout << "Estimated value of Pi: " << pi << endl;

    //std::cout << "The parallel time: " << diff.count() << std::endl;
    auto time_tbb = diff.count();
    start = std::chrono::high_resolution_clock::now(); 
    //pi = calculate_pi_ism(num_points);
    end = std::chrono::high_resolution_clock::now();
    diff = end - start;

    auto time_ism = diff.count();

    //std::cout<< time_ism <<std::endl;
  }
    return 0;
}
