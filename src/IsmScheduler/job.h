#pragma once
#include <cassert>
#include <atomic>
#include <functional>
#include <thread>
#include <type_traits>    // IWYU pragma: keep
#include <chrono>
#include <iostream>

struct WorkStealingJob {
  WorkStealingJob() : done{false} { }
  virtual ~WorkStealingJob() = default;
  
  void operator()() {
    assert(done.load(std::memory_order_relaxed) == false);
    //auto executionTime = std::chrono::high_resolution_clock::now();
    execute();
    //auto end = std::chrono::high_resolution_clock::now();
    //auto duration = end - creationTime;
    
    //std::cout << "Job executed in " << duration.count() << " \n";
    
    done.store(true, std::memory_order_release);
  }
  
  [[nodiscard]] bool finished() const noexcept {
    return done.load(std::memory_order_acquire);
  }
  
  void wait() const noexcept {
    while (!finished())
      std::this_thread::yield();
  }
  
 protected:
  virtual void execute() = 0;
  std::atomic<bool> done;
  //std::chrono::time_point<std::chrono::high_resolution_clock> creationTime;
};

template<typename F>
struct JobImpl : WorkStealingJob {
  static_assert(std::is_invocable_v<F&>);
  
  template<typename Func>
  explicit JobImpl(Func&& f) : WorkStealingJob(), f(std::forward<Func>(f)) { }
  
  void execute() override {
    f();
  }
 private:
  F f;
};

template<typename F>
auto make_job(F&& f) { 
  return JobImpl<std::remove_reference_t<F>>(std::forward<F>(f)); 
}
