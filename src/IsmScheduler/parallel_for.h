
#include <cassert>
#include <cstdlib>

#include <algorithm>
#include <functional>
#include <oneapi/tbb/blocked_range.h>
#include <string>
#include <thread>
#include <type_traits>  // IWYU pragma: keep
#include <utility>
#include <tbb/blocked_range.h>
#include <chrono>
#include <iostream>

inline size_t num_workers();

inline size_t worker_id();

template <typename F>
inline void parallel_for(size_t start, size_t end, F&& f, long granularity = 0,
                         bool conservative = false);

template <typename Lf, typename Rf>
inline void parallel_invoke(Lf&& left, Rf&& right, bool conservative = false);

template <typename Lf, typename Rf>
inline void parallel_do(Lf&& left, Rf&& right, bool conservative = false) {
  static_assert(std::is_invocable_v<Lf&&>);
  static_assert(std::is_invocable_v<Rf&&>);
  
  par_do(std::forward<Lf>(left), std::forward<Rf>(right), conservative);
  }

#include "schedule.h"

#include "job.h"

inline uint64_t get_time_parallel_for() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

inline unsigned int init_num_workers() {
    return std::thread::hardware_concurrency();
}
using scheduler_type = scheduler_ism< WorkStealingJob>;
extern inline scheduler_type& get_current_scheduler() {
  auto current_scheduler = scheduler_type::get_current_scheduler();
  if (current_scheduler == nullptr) {
    static thread_local scheduler_type local_scheduler(init_num_workers());

    //std::cout << get_time_parallel_for()<< std::endl; 
    return local_scheduler;
  }
  //std::cout << get_time_parallel_for()<< std::endl; 
  return *current_scheduler;
}


inline size_t num_workers() {
  return get_current_scheduler().num_workers();
}

inline size_t worker_id() {
  return get_current_scheduler().worker_id();
}

template <typename F>
inline void parallel_for(size_t start, size_t end, F&& f, long granularity, bool conservative) {
  static_assert(std::is_invocable_v<F&, size_t>);
  auto wrapper_lambda = [&](tbb::blocked_range<size_t> range){
    for(size_t i = range.begin(); i != range.end(); ++i){
      f(i);
    }
  };
  if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i = start; i < end; i++) f(i);
    //f(tbb::blocked_range<size_t>(start,end));
  }
  else if (end > start) {
    fork_join_scheduler::parfor(get_current_scheduler(), start, end,
    wrapper_lambda, static_cast<size_t>(granularity), conservative);
  }
}

template <typename F>
inline void parallel_for_morsel(size_t start, size_t end, F&& f, long granularity, bool conservative) {
  static_assert(std::is_invocable_v<F&, tbb::blocked_range<size_t>>);
  if ((end - start) <= static_cast<size_t>(granularity)) {
    f(tbb::blocked_range<size_t>(start,end));
  }
  else if (end > start) {
    fork_join_scheduler::parfor(get_current_scheduler(), start, end,
    std::forward<F>(f), static_cast<size_t>(granularity), conservative);
  }
}

template <typename Lf, typename Rf>
inline void par_do(Lf&& left, Rf&& right, bool conservative) {
  static_assert(std::is_invocable_v<Lf&&>);
  static_assert(std::is_invocable_v<Rf&&>);
  fork_join_scheduler::pardo(get_current_scheduler(), std::forward<Lf>(left), std::forward<Rf>(right), conservative);
  //::usleep(2);
}

template <typename F>
void execute_with_scheduler(unsigned int p, F&& f) {
  scheduler_type scheduler(p);
  std::invoke(std::forward<F>(f));
}



