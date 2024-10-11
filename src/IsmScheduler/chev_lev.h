
#pragma once
#include <atomic>
#include <optional>
#include <vector>
#include "job.h" // Include the WorkStealingJob definition
#define profiling_stats 1
template <typename T>
class WorkStealingQueue {
  static_assert(std::is_pointer_v<T>, "T must be a pointer type");

  struct Array {
    int64_t C;
    int64_t M;
    std::atomic<T>* S;

    explicit Array(int64_t c) : 
      C {c},
      M {c-1},
      S {new std::atomic<T>[static_cast<size_t>(C)]} {
    }

    ~Array() {
      delete [] S;
    }

    int64_t capacity() const noexcept {
      return C;
    }
    
    void push(int64_t i, T o) noexcept {
      S[i & M].store(o, std::memory_order_relaxed);
    }

    T pop(int64_t i) noexcept {
      return S[i & M].load(std::memory_order_relaxed);
    }

    Array* resize(int64_t b, int64_t t) {
      Array* ptr = new Array {2*C};
      for(int64_t i=t; i!=b; ++i) {
        ptr->push(i, pop(i));
      }
      return ptr;
    }
  };

  std::atomic<int64_t> _top;
  std::atomic<int64_t> _bottom;
  std::atomic<Array*> _array;
  std::vector<Array*> _garbage;

public:
#ifdef profiling_stats
  int pushBottom{0}, popBottom{0}, popTop{0}, resize{0}, success{0};
  long long cas{0}, fence{0};

#endif //profiling_stats

  explicit WorkStealingQueue(int64_t capacity = 1024) {
    _top.store(0, std::memory_order_relaxed);
    _bottom.store(0, std::memory_order_relaxed);
    _array.store(new Array{capacity}, std::memory_order_relaxed);
    _garbage.reserve(32);

  }

  ~WorkStealingQueue() {
    for(auto a : _garbage) {
      //delete a;
    }
    delete _array.load();
  }

  void push(T o) {
    int64_t b = _bottom.load(std::memory_order_relaxed);
    int64_t t = _top.load(std::memory_order_acquire);
    Array* a = _array.load(std::memory_order_relaxed);

    if(a->capacity() - 1 < (b - t)) {
      Array* tmp = a->resize(b, t);
      _garbage.push_back(a);
      std::swap(a, tmp);
      _array.store(a, std::memory_order_relaxed);
    }

    a->push(b, o);
    std::atomic_thread_fence(std::memory_order_release);
    _bottom.store(b + 1, std::memory_order_relaxed);
  #ifdef profiling_stats
    //fence++;
    pushBottom++;
  #endif // a
  }

  std::optional<T> pop() {
    int64_t b = _bottom.load(std::memory_order_relaxed) - 1;
    Array* a = _array.load(std::memory_order_relaxed);
    _bottom.store(b, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    int64_t t = _top.load(std::memory_order_relaxed);

    std::optional<T> item;

    if(t <= b) {
      item = a->pop(b);
      if(t == b) {
        if(!_top.compare_exchange_strong(t, t+1, 
                                         std::memory_order_seq_cst, 
                                         std::memory_order_relaxed)) {
          item = std::nullopt;
        }
        _bottom.store(b + 1, std::memory_order_relaxed);
        #ifdef profiling_stats
        cas++;
        
        #endif // DEBUG
      }
    }
    else {
      _bottom.store(b + 1, std::memory_order_relaxed);
    }
#ifdef profiling_stats
    fence++;
    popBottom++;
#endif
    return item;
  }

  std::optional<T> steal() {
    int64_t t = _top.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    int64_t b = _bottom.load(std::memory_order_acquire);

    std::optional<T> item;

    if(t < b) {
      Array* a = _array.load(std::memory_order_consume);
      item = a->pop(t);
      if(!_top.compare_exchange_strong(t, t+1,
                                       std::memory_order_seq_cst,
                                       std::memory_order_relaxed)) {

        item =  std::nullopt;
#ifdef profiling_stats
        cas++;
#endif // DEBUG
      }
#ifdef profiling_stats
      else success++;
#endif
    }
#ifdef  profiling_stats
    fence++;
    popTop++;
#endif // DEBUG
    return item;
  }

  // Function: capacity
  int64_t capacity() const noexcept {
    return _array.load(std::memory_order_relaxed)->capacity();
  }
};
