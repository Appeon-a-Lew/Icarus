#include <sycl/sycl.hpp>
#include <iostream>
#include <chrono>
#include <climits>



int main() {
    sycl::queue queue{sycl::default_selector_v};

    size_t num_jobs = 1000000 ;  // Number of empty jobs
    int * data = new int[num_jobs];
    sycl::range<1> range(num_jobs);
     queue.submit([&](sycl::handler& cgh){} );
    auto start = std::chrono::high_resolution_clock::now();

    queue.submit([&](sycl::handler& cgh) {
        cgh.parallel_for<class empty_jobs>(range, [=](sycl::id<1> idx) {
            data[idx] = idx;
        });
    });

    queue.wait();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;

    std::cout << "All empty jobs executed successfully." << std::endl;
    std::cout << "Execution time: " << duration.count() << " ms" << std::endl;

    return 0;
}

