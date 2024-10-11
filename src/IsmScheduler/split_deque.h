#pragma once
#include "mailbox.h"
#include "oneapi/tbb/detail/_task.h"
#include <cassert>

#include <atomic>
#include <cwchar>
#include <iostream>
#include <iterator>
#include <utility>
#include <array>
#include <iostream>


#define profiling_stats1 1

// Deque from Arora, Blumofe, and Plaxton (SPAA, 1998).
//
// Supports:
//
// push_bottom:   Only the owning thread may call this
// pop_bottom:    Only the owning thread may call this
// pop_top:       Non-owning threads may call this
//

template <typename Job>
struct Deque {
  using qidx = unsigned int;
  using tag_t = unsigned int;

  // use std::atomic<age_t> for atomic access.
  // Note: Explicit alignment specifier required
  // to ensure that Clang inlines atomic loads.
  struct alignas(int64_t) age_t {
    // cppcheck bug prevents it from seeing usage with braced initializer
    tag_t tag;                // cppcheck-suppress unusedStructMember
    qidx top;                 // cppcheck-suppress unusedStructMember
  };

  // align to avoid false sharing
  struct alignas(64) padded_job {
    std::atomic<Job*> job;
  };    

  static constexpr int q_size = 20000;
  std::atomic<qidx> bot;
  std::atomic<age_t> age;

#ifdef profiling_stats
  int pushBottom, popBottom, popTop, success;
  long long cas, fence;

#endif //profiling_stats
  std::array<padded_job, q_size> deq;


  Deque() : bot(0),
#ifdef profiling_stats
            pushBottom(0), popBottom(0), popTop(0), cas(0), fence(0), success(0),
#endif
 age(age_t{0, 0}) {}
  void cleanup() {
    auto size_loc = size();
    auto local_bot = bot.load(std::memory_order_acquire);
    auto old_age = age.load(std::memory_order_acquire);

    for (qidx i = old_age.top; i < local_bot; ++i) {
        Job* job = deq[i].job.load(std::memory_order_acquire);
        if(!job) break;
        task_proxy* tp = dynamic_cast<task_proxy*>(tp);
        
        if (tp && tp->is_accessed_()) {  // Assuming Job has a method `is_done()`
            
            delete tp;  // Delete the job
            deq[i].job.store(nullptr, std::memory_order_release);  // Clear the job from the deque
        }
    }

    // Update the top index if possible (e.g., if all jobs before a certain index are done)
    while (old_age.top < local_bot && !deq[old_age.top].job.load(std::memory_order_acquire)) {
        ++old_age.top;
    }

    age.store(old_age, std::memory_order_release);
    auto size_new_loc = size();
    std::cout << "\nCLEANUP\n" << "old size was: " << size_loc << " and new size: " << size_new_loc << "\n";


}


  int size() const {
    auto local_bot = bot.load(std::memory_order_acquire);  // Load the current bottom index
    auto local_top = age.load(std::memory_order_acquire).top;  // Load the current top index
    return local_bot - local_top;  // The size is the difference between bot and top
  }
  // Adds a new job to the bottom of the queue. Only the owning
  // thread can push new items. This must not be called by any
  // other thread.
  //
  // Returns true if the queue was empty before this push
  bool push_bottom(Job* job) {
    //static volatile int cnt = 0;
    //if(++cnt %128 == 0) cleanup();
    auto local_bot = bot.load(std::memory_order_acquire);      // atomic load
    deq[local_bot].job.store(job, std::memory_order_release);  // shared store
    local_bot += 1;
    if (local_bot == q_size) {
      std::cerr << "internal error: scheduler queue overflow\n";
      std::abort();
    }
    bot.store(local_bot, std::memory_order_seq_cst);  // shared store
    Job* tmp = deq[local_bot-1].job.load();



#ifdef profiling_stats
    pushBottom++;
#endif 
    return (local_bot == 1);
  }

  // Pop an item from the top of the queue, i.e., the end that is not
  // pushed onto. Threads other than the owner can use this function.
  //
  // Returns {job, empty}, where empty is true if job was the
  // only job on the queue, i.e., the queue is now empty
  std::pair<Job*, bool> pop_top() {
    auto old_age = age.load(std::memory_order_acquire);    // atomic load
    auto local_bot = bot.load(std::memory_order_acquire);  // atomic load
#ifdef profiling_stats
    popTop++;
#endif 

    if (local_bot > old_age.top) {
      auto job = deq[old_age.top].job.load(std::memory_order_acquire);  // atomic load
      auto new_age = old_age;
      new_age.top = new_age.top + 1;
#ifdef profiling_stats
    cas++;
#endif 

      if (age.compare_exchange_strong(old_age, new_age)){
#ifdef profiling_stats
        success++;
#endif // DEBUG
        return {job, (local_bot == old_age.top + 1)};
      }
      else
        return {nullptr, (local_bot == old_age.top + 1)};
    }
    return {nullptr, true};
  }

  // Pop an item from the bottom of the queue. Only the owning
  // thread can pop from this end. This must not be called by any
  // other thread.
  Job* pop_bottom() {
    Job* result = nullptr;
    auto local_bot = bot.load(std::memory_order_acquire);  // atomic load
    if (local_bot != 0) {
      local_bot--;
      bot.store(local_bot, std::memory_order_release);  // shared store
      std::atomic_thread_fence(std::memory_order_seq_cst);
      auto job =
        deq[local_bot].job.load(std::memory_order_acquire);  // atomic load
      auto old_age = age.load(std::memory_order_acquire);      // atomic load
#ifdef profiling_stats
      fence++;
#endif // DEBUG

      if (local_bot > old_age.top)
        result = job;
      else {
        bot.store(0, std::memory_order_release);  // shared store
        auto new_age = age_t{old_age.tag + 1, 0};
        if ((local_bot == old_age.top) &&
          age.compare_exchange_strong(old_age, new_age))
          result = job;
        else {
          age.store(new_age, std::memory_order_seq_cst);  // shared store
          result = nullptr;
        }
#ifdef profiling_stats
        cas++;
#endif
      }
    }
#ifdef profiling_stats
    popBottom++;
#endif 
    return result;
  }
};
