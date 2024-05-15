#include <iostream>
#include <omp.h>
#include <chrono>

int main() {
    size_t num_jobs = 1000000; 

    auto start = std::chrono::high_resolution_clock::now();
    omp_set_num_threads(16);
    int* data = new int[num_jobs];
    //#pragma omp parallel for (static)
    for (size_t i = 0; i < num_jobs;++i) {
     data[i] = i;
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;

    std::cout << "All empty jobs executed successfully." << std::endl;
    std::cout << "Execution time: " << duration.count() << " ms" << std::endl;

    return 0;
}

