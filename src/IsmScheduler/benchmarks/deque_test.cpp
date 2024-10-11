
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include "../split_deque.h"  // Assuming your Deque code is saved here

// High-resolution timer function
auto get_time() {
    return std::chrono::high_resolution_clock::now();
}

// Job structure (assuming it is similar to task_proxy in your other code)
struct Job {
    bool is_done = false;  // Simulate a job being completed
};

// Producer function for pushing jobs to the bottom of the deque
void producer(Deque<Job>& deque, int task_count, std::atomic<long long>& total_push_time) {
    for (int i = 0; i < task_count; ++i) {
        Job* job = new Job();  // Create a new job

        // Time the push_bottom operation
        auto start = get_time();
        deque.push_bottom(job);
        auto end = get_time();

        // Accumulate time taken for push_bottom
        total_push_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }
}

// Consumer function for popping jobs from the bottom of the deque
void bottom_consumer(Deque<Job>& deque, int task_count, std::atomic<long long>& total_pop_bottom_time) {
    for (int i = 0; i < task_count; ++i) {
        // Time the pop_bottom operation
        auto start = get_time();
        Job* job = deque.pop_bottom();
        auto end = get_time();

        // Accumulate time taken for pop_bottom
        total_pop_bottom_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        if (job) {
            //delete job;  // Simulate completing the job
        }
    }
}

// Consumer function for popping jobs from the top of the deque
void top_consumer(Deque<Job>& deque, int task_count, std::atomic<long long>& total_pop_top_time) {
    for (int i = 0; i < task_count; ++i) {
        // Time the pop_top operation
        auto start = get_time();
        auto [job, empty] = deque.pop_top();
        auto end = get_time();

        // Accumulate time taken for pop_top
        total_pop_top_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        if (job) {
            //delete job;  // Simulate completing the job
        }
    }
}

int main() {
    const int num_producers = 10;
    const int num_bottom_consumers = 10;
    const int num_top_consumers = 10;
    const int tasks_per_producer = 10000;
    for( size_t i = 0; i < 200; ++i){
    // Create the deque and timing accumulators
    Deque<Job> deque;
    std::atomic<long long> total_push_time{0};
    std::atomic<long long> total_pop_bottom_time{0};
    std::atomic<long long> total_pop_top_time{0};

    // Create producer and consumer threads
    std::thread producer_thread(producer, std::ref(deque), tasks_per_producer, std::ref(total_push_time));
    std::thread bottom_consumer_thread(bottom_consumer, std::ref(deque), tasks_per_producer, std::ref(total_pop_bottom_time));
    std::thread top_consumer_thread(top_consumer, std::ref(deque), tasks_per_producer, std::ref(total_pop_top_time));

    // Join threads
    producer_thread.join();
    bottom_consumer_thread.join();
    top_consumer_thread.join();

    // Calculate the average times
    double average_push_time = static_cast<double>(total_push_time) / tasks_per_producer;
    double average_pop_bottom_time = static_cast<double>(total_pop_bottom_time) / tasks_per_producer;
    double average_pop_top_time = static_cast<double>(total_pop_top_time) / tasks_per_producer;

    // Output the results
    //std::cout << "Total time taken for push_bottom operations: " << total_push_time.load() << " nanoseconds\n";
    //std::cout << "Average time per push_bottom operation: " << average_push_time << " nanoseconds\n";

    //std::cout << "Total time taken for pop_bottom operations: " << total_pop_bottom_time.load() << " nanoseconds\n";
    //std::cout << "Average time per pop_bottom operation: " << average_pop_bottom_time << " nanoseconds\n";

    //std::cout << "Total time taken for pop_top operations: " << total_pop_top_time.load() << " nanoseconds\n";
    //std::cout << "Average time per pop_top operation: " << average_pop_top_time << " nanoseconds\n";
    std::cout << " ("<< i<< ", " << average_pop_top_time<< ") ";
    }
    return 0;
}
