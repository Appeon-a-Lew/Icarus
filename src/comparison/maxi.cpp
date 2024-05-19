#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>  
#include "schedule.hpp"

class EmptyJob {
public:
    std::chrono::high_resolution_clock::time_point scheduled_time;
    std::chrono::high_resolution_clock::time_point latency;
    

    // The constructor sets the scheduled time
    EmptyJob() : scheduled_time(std::chrono::high_resolution_clock::now()) {}

    void perform() const {
        }
};

int main() {
    size_t num_jobs = 1000000; 
    std::vector<int> data(num_jobs);
    std::fill(data.begin(), data.end(), 0);

    std::vector<EmptyJob> jobs(num_jobs);

    sched scheduler;

    auto start = std::chrono::high_resolution_clock::now();
    scheduler::parallel_for_each(jobs.begin(), jobs.end(), [](const EmptyJob& job) {
        job.perform();
    });
    // sched::parallel_for(tbb::blocked_range<size_t>(0, num_jobs), [&](const tbb::blocked_range<size_t>& r) {
    //     for (size_t i = r.begin(); i != r.end(); ++i) {
    //         data[i] = static_cast<int>(i);
    //     }
    // });

    auto end = std::chrono::high_resolution_clock::now();

    

    std::chrono::duration<double, std::milli> duration = end - start;

    std::cout << "All jobs executed successfully." << std::endl;
    std::cout << "Execution time: " << duration.count() << " ms" << std::endl;

    return 0;
}