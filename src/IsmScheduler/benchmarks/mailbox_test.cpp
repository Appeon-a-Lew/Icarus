
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "../mailbox.h"  // Assuming your code is in this file

// Helper function to get current high-resolution time
auto get_time() {
    return std::chrono::high_resolution_clock::now();
}

// Producer function to push tasks into the mail_outbox
void producer(mail_outbox& outbox, int task_count) {
    for (int i = 0; i < task_count; ++i) {
        task_proxy* t = new task_proxy();  // Simulate task creation
        outbox.push(t);
    }
}

// Consumer function to pop tasks from the mail_inbox and record time taken
void consumer(mail_inbox& inbox, int task_count, std::atomic<long long>& total_pop_time) {
    for (int i = 0; i < task_count; ++i) {
        auto start = get_time();  // Start timing the pop
        task_proxy* t = inbox.pop();
        auto end = get_time();    // End timing the pop

        if (t) {
            // Record time taken for the pop operation
            total_pop_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            // Simulate task completion
            delete t;
        }
    }
}

int main() {
    const int num_producers = 4;
    const int num_consumers = 1;
    const int tasks_per_producer = 10000;  // Number of tasks each producer will generate
    std::atomic<long long> total_pop_time{0};  // To store total pop operation time

    // Create the outbox and inbox
    mail_outbox outbox;
    outbox.construct();  // Initialize the outbox
    mail_inbox inbox;
    inbox.attach(outbox);  // Attach inbox to outbox

    // Create producer and consumer threads
    std::thread producer_thread(producer, std::ref(outbox), tasks_per_producer);
    std::thread consumer_thread(consumer, std::ref(inbox), tasks_per_producer, std::ref(total_pop_time));

    // Join threads
    producer_thread.join();
    consumer_thread.join();

    // Calculate the average time per pop operation
    double average_pop_time = static_cast<double>(total_pop_time) / tasks_per_producer;

    // Output the results
    //std::cout << "Total time taken for pop operations: " << total_pop_time.load() << " nanoseconds\n";
    //std::cout << "Average time per pop operation: " << average_pop_time << " nanoseconds\n";
    std::cout << average_pop_time << ", ";
    return 0;
}

