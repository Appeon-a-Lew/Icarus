

#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <random>
#include <cassert>

#include <atomic>
#include <cwchar>
#include <iostream>
#include <iterator>
#include <utility>
#include <array>
#include <iostream>
#include <iostream>
#include <deque>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <random>
#include <chrono>
// Custom work-stealing deque (from your provided code)
template <typename Job>
struct Deque {
    using qidx = unsigned int;
    using tag_t = unsigned int;

    struct alignas(int64_t) age_t {
        tag_t tag;
        qidx top;
    };

    struct alignas(64) padded_job {
        std::atomic<Job*> job;
    };

    static constexpr int q_size = 20000;
    std::atomic<qidx> bot;
    std::atomic<age_t> age;
    std::array<padded_job, q_size> deq;

    Deque() : bot(0), age(age_t{0, 0}) {}

    int size() const {
        auto local_bot = bot.load(std::memory_order_acquire);
        auto local_top = age.load(std::memory_order_acquire).top;
        return local_bot - local_top;
    }

    bool push_bottom(Job* job) {
        auto local_bot = bot.load(std::memory_order_acquire);
        deq[local_bot].job.store(job, std::memory_order_release);
        local_bot += 1;
        if (local_bot == q_size) {
            std::cerr << "internal error: scheduler queue overflow\n";
            std::abort();
        }
        bot.store(local_bot, std::memory_order_seq_cst);
        return (local_bot == 1);
    }

    Job* pop_bottom() {
        Job* result = nullptr;
        auto local_bot = bot.load(std::memory_order_acquire);
        if (local_bot != 0) {
            local_bot--;
            bot.store(local_bot, std::memory_order_release);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            auto job = deq[local_bot].job.load(std::memory_order_acquire);
            auto old_age = age.load(std::memory_order_acquire);
            if (local_bot > old_age.top)
                result = job;
            else {
                bot.store(0, std::memory_order_release);
                auto new_age = age_t{old_age.tag + 1, 0};
                if ((local_bot == old_age.top) &&
                    age.compare_exchange_strong(old_age, new_age))
                    result = job;
                else {
                    age.store(new_age, std::memory_order_seq_cst);
                    result = nullptr;
                }
            }
        }
        return result;
    }

    std::pair<Job*, bool> pop_top() {
        auto old_age = age.load(std::memory_order_acquire);
        auto local_bot = bot.load(std::memory_order_acquire);
        if (local_bot > old_age.top) {
            auto job = deq[old_age.top].job.load(std::memory_order_acquire);
            auto new_age = old_age;
            new_age.top = new_age.top + 1;
            if (age.compare_exchange_strong(old_age, new_age))
                return {job, (local_bot == old_age.top + 1)};
            else
                return {nullptr, (local_bot == old_age.top + 1)};
        }
        return {nullptr, true};
    }
};

// Global variable for tasks
std::atomic<int> global_counter(0);

// Producer function
void producer(Deque<int>& deque, int execution_time) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start_time).count() < execution_time) {
        int rand_num = dis(gen);
        if (rand_num <= 98) {
            // 98% of the time, push a task
            deque.push_bottom(new int(1));  // Push a new task that increments the global counter
        } else {
            // 2% of the time, pop a task from the bottom
            int* task = deque.pop_bottom();
            if (task) {
                global_counter.fetch_add(*task, std::memory_order_relaxed);
                delete task;
            }
        }
    }
}

// Stealer function
void stealer(Deque<int>& deque, std::atomic<bool>& stop_flag) {
    while (!stop_flag.load()) {
        auto [task, is_empty] = deque.pop_top();
        if (task) {
            global_counter.fetch_add(*task, std::memory_order_relaxed);
            delete task;
        }
    }
}
struct Node {
    int depth;
    int max_depth;
};

// Global parameters
int B = 50;  // Maximum branch factor
int D = 10; // Maximum depth of DAG
std::atomic<bool> stop_flag(false);  // Stop flag for threads

// Global atomic counters to count processed nodes
std::atomic<int> custom_nodes_processed(0);
std::atomic<int> stddeque_nodes_processed(0);

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> dis(0.0, 1.0);

// Thread function: pops nodes and generates children using custom deque
void worker_custom(Deque<Node>& deque, std::vector<Deque<Node>>& deques, int thread_id) {
    while (!stop_flag.load()) {
        Node* node = deque.pop_bottom();
        if (!node) {
            // Try stealing from other deques
            for (int i = 0; i < deques.size(); ++i) {
                if (i != thread_id) {
                    auto [stolen_node, empty] = deques[i].pop_top();
                    if (stolen_node) {
                        node = stolen_node;
                        break;
                    }
                }
            }
        }
        
        if (node) {
            // Increment the custom deque processed nodes counter
            custom_nodes_processed.fetch_add(1, std::memory_order_relaxed);

            // Process the node: Generate child nodes
            int depth = node->depth;
            int max_depth = node->max_depth;
            if (depth < max_depth) {
                int num_children = B * (1.0 - (double)depth / max_depth);
                for (int i = 0; i < num_children; ++i) {
                    Node* child = new Node{depth + 1, max_depth};
                    deque.push_bottom(child);
                }
            }
            delete node;
        }
    }
}

// Thread function: pops nodes and generates children using std::deque
void worker_stddeque(std::deque<Node*>& deque, std::vector<std::deque<Node*>>& deques, int thread_id, std::mutex& deque_mutex) {
    while (!stop_flag.load()) {
        Node* node = nullptr;
        {
            std::lock_guard<std::mutex> lock(deque_mutex); // Protect deque access
            if (!deque.empty()) {
                node = deque.back();
                deque.pop_back();
            }
        }

        if (!node) {
            // Try stealing from other deques
            for (int i = 0; i < deques.size(); ++i) {
                if (i != thread_id) {
                    std::lock_guard<std::mutex> lock(deque_mutex); // Protect deque access
                    if (!deques[i].empty()) {
                        node = deques[i].front();
                        deques[i].pop_front();
                        break;
                    }
                }
            }
        }

        if (node) {
            // Increment the std::deque processed nodes counter
            stddeque_nodes_processed.fetch_add(1, std::memory_order_relaxed);

            // Process the node: Generate child nodes
            int depth = node->depth;
            int max_depth = node->max_depth;
            if (depth < max_depth) {
                int num_children = B * (1.0 - (double)depth / max_depth);
                for (int i = 0; i < num_children; ++i) {
                    Node* child = new Node{depth + 1, max_depth};
                    {
                        std::lock_guard<std::mutex> lock(deque_mutex); // Protect deque access
                        deque.push_back(child);
                    }
                }
            }
            delete node;
        }
    }
}

int main() {
    int num_threads = 1;  // Number of worker threads
    int execution_time = 1;  // Time in seconds for which the benchmark runs

    // ****************** Test 1: Custom Deque ******************
    // Create deques for each thread (custom deque)
    std::vector<Deque<Node>> custom_deques(num_threads);

    // Create worker threads for custom deque
    std::vector<std::thread> custom_threads;
    stop_flag.store(false);  // Reset stop flag
    for (int i = 0; i < num_threads; ++i) {
        custom_threads.emplace_back(worker_custom, std::ref(custom_deques[i]), std::ref(custom_deques), i);
    }

    // Seed the initial node in one of the custom deques
    custom_deques[0].push_bottom(new Node{0, D});

    // Let the simulation run for the specified execution time
    std::this_thread::sleep_for(std::chrono::seconds(execution_time));

    // Signal workers to stop for custom deque
    stop_flag.store(true);

    // Join worker threads for custom deque
    for (auto& thread : custom_threads) {
        thread.join();
    }

    // Output the result for custom deque
    std::cout << "Total nodes processed with custom deque: " << custom_nodes_processed.load() << std::endl;

    // ****************** Test 2: std::deque ******************
    // Reset the stop flag and nodes processed counter
    stop_flag.store(false);
    stddeque_nodes_processed.store(0);

    // Create deques for each thread (std::deque)
    std::vector<std::deque<Node*>> stddeques(num_threads);
    std::mutex deque_mutex;  // Mutex to protect deque access in std::deque

    // Create worker threads for std::deque
    std::vector<std::thread> stddeque_threads;
    for (int i = 0; i < num_threads; ++i) {
        stddeque_threads.emplace_back(worker_stddeque, std::ref(stddeques[i]), std::ref(stddeques), i, std::ref(deque_mutex));
    }

    // Seed the initial node in one of the std::deques
    stddeques[0].push_back(new Node{0, D});

    // Let the simulation run for the specified execution time
    std::this_thread::sleep_for(std::chrono::seconds(execution_time));

    // Signal workers to stop for std::deque
    stop_flag.store(true);

    // Join worker threads for std::deque
    for (auto& thread : stddeque_threads) {
        thread.join();
    }

    // Output the result for std::deque
    std::cout << "Total nodes processed with std::deque:   " << stddeque_nodes_processed.load() << std::endl;

    return 0;
}
