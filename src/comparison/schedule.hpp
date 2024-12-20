// -----------------------------------------------------------------------------
// 2023 Maximilian Kuschewski
// -----------------------------------------------------------------------------
#pragma once
#include <oneapi/tbb/cache_aligned_allocator.h>
#include <sys/resource.h>
#include <unistd.h>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>
#include <optional>
#include <cstring>
#include <functional>
#include <iostream>

#include <tbb/blocked_range.h>

#include "debug.hpp"
#include "template.hpp"
#include "ref_or_instance.hpp"
#include "cacheline.hpp"
#include "barrier.hpp"
#include "../IsmScheduler/split_deque.h"
struct ThreadLocalProvider {
    // expected to be set on thread initialization
    inline static thread_local int THREAD_LOCAL_ID = -1;
};

template<typename T, unsigned Align = libdb::NO_FALSE_SHARING_BYTES>
struct ThreadLocalVec {
    enum State : uint8_t { UNINITIALIZED = 0, INITIALIZED = 1 };
    struct StatePair {
        T data;
        State state;
        StatePair() = delete;
        StatePair(const StatePair&) = delete;
        StatePair(StatePair&&) = delete;
    };
    struct LocalEntry {
        libdb::type_aligned_memory<T> data;
        State state;

        LocalEntry() { set_uninitialized(); }
        ~LocalEntry() {
            if (initialized()) {
                data.data()->~T();
                set_uninitialized();
            }
        }

        [[nodiscard]] bool initialized() const {
            return state == INITIALIZED;
        }
        void set_initialized() { state = INITIALIZED; }
        void set_uninitialized() { state = UNINITIALIZED; }

        T& value() { return *data.data(); }
        const T& value() const { return *data.data(); }

        // NOLINTBEGIN(google-explicit-constructor)
        operator const StatePair&() const { return *std::bit_cast<const StatePair*>(this); ; }
        operator StatePair&() { return *std::bit_cast<StatePair*>(this); }
        // NOLINTEND(google-explicit-constructor)
    };
    static_assert(sizeof(StatePair) == sizeof(LocalEntry));
    static_assert(sizeof(libdb::type_aligned_memory<T>) == sizeof(T));

    using DataVec = std::vector<libdb::pad_to<LocalEntry, Align>>; // tbb::cache_aligned_allocator<PaddedEntry>

    template<typename BaseIt>
    struct _iterator {
        using base = BaseIt;
        BaseIt _it;
        _iterator(BaseIt it) : _it(it) {}
        _iterator& operator++() {++_it; return *this;}
        _iterator& operator++(int) {auto ret = *this; ++(*this); return ret; }
        bool operator==(_iterator other) { return _it == other._it; }
        bool operator!=(_iterator other) { return _it != other._it; }

        std::ptrdiff_t operator-(const _iterator& other) const {
            return _it - other._it;
        }
    };
    struct iterator : _iterator<typename DataVec::iterator> {
        using _iterator<typename DataVec::iterator>::_iterator;
        StatePair& operator*() {
            return *this->_it;
        }
    };
    struct const_iterator : _iterator<typename DataVec::const_iterator> {
        using _iterator<typename DataVec::const_iterator>::_iterator;
        const StatePair& operator*() const {
            return *this->_it;
        }
    };

    using Constructor = std::function<void(T*)>;
    static inline Constructor default_constructor{ [](T* ptr) { new (ptr) T(); } };

    DataVec data;
    Constructor constructor;

    explicit ThreadLocalVec(std::function<void(T*)> cons = default_constructor);
    explicit ThreadLocalVec(int threads, std::function<void(T*)> cons = default_constructor)
       : data(threads)
       , constructor(cons) {}

    ThreadLocalVec(const ThreadLocalVec&) = delete;
    ThreadLocalVec(ThreadLocalVec&& o) noexcept
        : data(std::move(o.data))
        , constructor(std::move(o.constructor)) {}

    ~ThreadLocalVec() = default;

    void clear() {
        // TODO make more efficient / multithread?
        auto tcnt = data.size();
        data.clear();
        data.resize(tcnt);
    }

    T& operator[](unsigned idx) {
        assert(data[idx].initialized());
        return data[idx].value();
    }

    bool destroy_entry(unsigned idx) {
        if (data[idx].initialized()) {
            data[idx].value().~T();
            data[idx].set_uninitialized();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool initialized(unsigned idx) const {
        return data[idx].initialized();
    }

    iterator begin() { return data.begin(); }
    iterator end() { return data.end(); }

    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }

    [[nodiscard]] size_t size() const { return data.size(); }

    T& local(bool& existed) {
        auto id = ThreadLocalProvider::THREAD_LOCAL_ID;
        existed = data[id].initialized();
        if (!existed) {
            constructor(&(static_cast<T&>(data[id].value())));
            data[id].set_initialized();
        }
        return data[id].value();
    }

    T& remote(unsigned id) {
        if (!data[id].initialized()) {
            constructor(&(static_cast<T&>(data[id].value())));
            data[id].set_initialized();
        }
        return data[id].value();
    }

    T& local() {
        auto id = ThreadLocalProvider::THREAD_LOCAL_ID;
        if (!data[id].initialized()) {
            constructor(&(static_cast<T&>(data[id].value())));
            data[id].set_initialized();
        }
        return data[id].value();
    }
};


#include <atomic>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <optional>
#include <thread>
#include <cassert>
#include <unistd.h>

struct task_proxy {
    intptr_t task_and_tag;
    task_proxy *next_in_mailbox;
    unsigned outbox_id;

    task_proxy() : task_and_tag(0), next_in_mailbox(nullptr), outbox_id(0) {}

    static const intptr_t pool_bit = 1 << 0;
    static const intptr_t mailbox_bit = 1 << 1;
    static const intptr_t location_mask = pool_bit | mailbox_bit;

    static bool is_shared(intptr_t tat) {
        return (tat & location_mask) == location_mask;
    }

    static void* task_ptr(intptr_t tat) {
        return reinterpret_cast<void*>(tat & ~location_mask);
    }
};

class mail_outbox {
    using proxy_ptr = task_proxy*;
    proxy_ptr my_first;
    std::atomic<proxy_ptr*> my_last;
    std::atomic<int> my_task_count;
    bool my_is_idle;

public:
    mail_outbox() : my_first(nullptr), my_last(&my_first), my_task_count(0), my_is_idle(false) {}

    bool push(task_proxy* t) {
        if (my_task_count.load(std::memory_order_relaxed) > 32) {
            return false;
        }
        my_task_count.fetch_add(1, std::memory_order_relaxed);
        t->next_in_mailbox = nullptr;
        proxy_ptr* link = my_last.exchange(&t->next_in_mailbox);
        *link = t;
        return true;
    }

    task_proxy* pop() {
        task_proxy* curr = my_first;
        if (!curr) return nullptr;
        task_proxy** prev_ptr = &my_first;
        if (task_proxy* second = curr->next_in_mailbox) {
            *prev_ptr = second;
        } else {
            *prev_ptr = nullptr;
            if (my_last.compare_exchange_strong(prev_ptr, &curr->next_in_mailbox)) {
                // Successfully transitioned mailbox from having one item to having none.
            } else {
                while (!(second = curr->next_in_mailbox)) std::this_thread::yield();
                *prev_ptr = second;
            }
        }
        my_task_count.fetch_sub(1, std::memory_order_relaxed);
        return curr;
    }

    bool empty() const {
        return my_first == nullptr;
    }

    bool recipient_is_idle() const {
        return my_is_idle;
    }

    void set_is_idle(bool value) {
        my_is_idle = value;
    }
};

class mail_inbox {
    mail_outbox* my_putter;

public:
    mail_inbox() : my_putter(nullptr) {}

    void attach(mail_outbox& putter) {
        my_putter = &putter;
    }

    task_proxy* pop() {
        return my_putter->pop();
    }

    bool empty() const {
        return my_putter->empty();
    }

    void set_is_idle(bool value) {
        if (my_putter) {
            my_putter->set_is_idle(value);
        }
    }
};


struct EMPTY_TYPE {};
struct MaxisScheduler : ThreadLocalProvider {
    using self_t = MaxisScheduler;
    using item_t = uint64_t;
    using morsel_t = tbb::blocked_range<item_t>;
    static constexpr unsigned DEFAULT_MORSEL_SIZE = 1;

    struct Execution;
    using worker_t = std::function<void(unsigned,Execution&)>;

    template<typename T, unsigned Align = 64>
    using tls = ThreadLocalVec<T, Align>;

    struct RuntimeConfig {
        unsigned morsel_size{DEFAULT_MORSEL_SIZE};
        double initial_morsel_multiplier{4};
        libdb::RefOrInstance<std::vector<size_t>> morsel_hints;

        template<typename F>
        RuntimeConfig with(F fn) const {
           RuntimeConfig copy = *this;
           fn(copy);
           return copy;
        }
    };

    struct Config : RuntimeConfig {
        unsigned threads{std::thread::hardware_concurrency()};
        //unsigned threads{1};
        int pin{1};

        template<typename F>
        Config with(F fn) const {
           Config copy = *this;
           fn(copy);
           return copy;
        }
    };

   struct Execution {
        struct alignas(16) range_t { item_t start; item_t end; };
struct ThreadState {
  Deque<morsel_t> work_deque;  // Work-stealing deque for jobs
  std::vector<std::unique_ptr<morsel_t>> morsels;  // Store owned morsel_t objects
  std::atomic<size_t> processed{0};  // Tracks the number of processed items
  bool done{false};  // Indicates if the thread has finished processing

  ThreadState() = default;
  ThreadState(const ThreadState& o) = delete;  // Disable copy constructor
  morsel_t* mark_processed_and_get() {
            auto job = work_deque.pop_bottom();
            if (job) {
                processed.fetch_add(job->size(), std::memory_order_relaxed);
            }
            return job;
        }

        void assign_morsel(item_t start, item_t end) {
            auto morsel = std::make_unique<morsel_t>(start, end);
            work_deque.push_bottom(morsel.get());
            morsels.push_back(std::move(morsel));
  }
};
      // std::atomic<uint64_t> joinable_count{0};
        std::atomic<uint64_t> done_count{0};
        std::atomic<unsigned> hints_used{0};
        const RuntimeConfig& config;
        size_t item_count;
        unsigned thread_count{0};
        const worker_t& worker;
        std::vector<ThreadState> cursors;

        DEBUGGING(std::atomic<uint64_t> worker_count{0}; std::atomic<uint64_t> processed{0};)
        Execution(const Config& global_config, const RuntimeConfig& config, const worker_t& worker, item_t items)
    : config(config)
    , item_count(items)
    , thread_count(global_config.threads)
    , worker(worker)
    , cursors(global_config.threads) {

    const auto& hints = *config.morsel_hints;

    size_t total_items = items;
    size_t morsel_size = get_morsel_size();  // Determine the appropriate morsel size
    size_t items_per_thread = total_items / thread_count;  // Initial large chunk for each thread
    for (unsigned i = 0; i < thread_count; ++i) {
        size_t start_item = i * items_per_thread;
        size_t end_item = (i + 1) == thread_count ? total_items : (i + 1) * items_per_thread;

        // Break down the large chunk into smaller morsels and assign them to the deque
        while (start_item < end_item) {
            size_t current_morsel_size = std::min(morsel_size, end_item - start_item);
            cursors[i].assign_morsel(start_item, start_item + current_morsel_size);
            start_item += current_morsel_size;
        }
    }
}
morsel_t* next(unsigned tid, std::optional<item_t> size = std::nullopt, unsigned sleep_time = 1) {
    auto& lstate = cursors[tid];
    if (done_count == thread_count) {
        return nullptr;
    }
    morsel_t* task = nullptr;
    // Try to pop from the local deque first
    //morsel_t* task = lstate.work_deque.pop_bottom();
    if ((task = lstate.mark_processed_and_get())) {

        return task;
    } else {
        lstate.done = true;  // Mark this thread as done if no more local work is available
        ++done_count;
    }

    // Attempt to steal from neighbors
    for (auto offset = 1u; offset != thread_count; ++offset) {
        auto neighbour_id = (tid + offset) % thread_count;
        auto& neighbour = cursors[neighbour_id];
        auto [stolen_task, asd] = neighbour.work_deque.pop_top();
        if (stolen_task != nullptr) {
            return stolen_task;
        }
    }

    // All tasks processed
    return nullptr;
}


    size_t get_morsel_size(std::optional<item_t> size = std::nullopt) {
    return std::max(static_cast<item_t>(1ul), size.value_or(config.morsel_size));
  }
};  // struct Execution

static constexpr uint64_t WAIT = 0x0ul;
static constexpr uint64_t QUIT = 0x1ul;
static Execution* wait_flag() { return std::bit_cast<Execution*>(WAIT); }
static Execution* quit_flag() { return std::bit_cast<Execution*>(QUIT); }

Config config;
std::function<void(int tid, Execution* exec)> worker;
std::vector<std::thread> threads;
std::vector<std::atomic<uint64_t>> execution_ptrs;

std::atomic<Execution*> current_execution;

static_assert(decltype(current_execution)::is_always_lock_free,
"Atomic execution storage is not lock-free.");

explicit MaxisScheduler(Config config)
  : config(std::move(config))
  , threads()
  , execution_ptrs(config.threads)
  , current_execution(wait_flag()) {
  init_threads();
}

explicit MaxisScheduler() : MaxisScheduler(Config()) {}

~MaxisScheduler() {
  change_execution(quit_flag());
  for (auto& t : threads) { t.join(); }
}

void execute(uint64_t items, const worker_t& worker, const std::optional<RuntimeConfig>& _cfg = std::nullopt) {
  const RuntimeConfig& cfg = _cfg.value_or(this->config);
  if(current_execution.load() != wait_flag()) {
#ifndef NDEBUG
    std::cerr << "warning: nested exec call in scheduler on thread " << THREAD_LOCAL_ID << "; executing on single thread instead." << std::endl;
#endif
    Config single_threaded_config = this->config.with([](auto& c) { c.threads = 1; });
    Execution exec(single_threaded_config, cfg, worker, items);
    worker(0, exec); // don't use THREAD_LOCAL_ID here
    return;
  }
  Execution exec(this->config, cfg, worker, items);
  change_execution(&exec);
  worker(0, exec);
  change_execution(wait_flag());
  for (auto done_count = 0u;; done_count = 0) {
    for (auto& thread_local_ptr : execution_ptrs) {
      done_count += thread_local_ptr == 0;
    }
    if (done_count == config.threads) { break; }
    _mm_pause(); // ::usleep(5);
  }
}

[[nodiscard]] bool pin_threads() const { return config.pin >= 0; }

static unsigned core_count() { return std::thread::hardware_concurrency(); }

template<typename T, unsigned Align = 64>
tls<T, Align> make_local(typename tls<T, Align>::Constructor constructor = tls<T, Align>::default_constructor) {
  assert(THREAD_LOCAL_ID == 0);
  return ThreadLocalVec<T, Align>(config.threads, constructor);
}

private:
void worker_function(int tid) {
  THREAD_LOCAL_ID = tid;
  if (pin_threads()) { this->set_affinity((config.pin + tid) % core_count()); }
  DEBUGGING(int proc{0}, fin{0});
    while (true) {
      current_execution.wait(wait_flag());
      // TODO may be able to get away without recv_barrier
      // if checking that exec has not changed here
      // and going to done_barrier in that case
      // as it must mean everyone has already finished and execution is reset to wait_flag()
      // before this thread got woken
      do {
        auto exec = current_execution.load();
        execution_ptrs[tid] = std::bit_cast<uint64_t>(exec);
        // might have changed; re-check
      } while (std::bit_cast<uint64_t>(current_execution.load()) != execution_ptrs[tid]);
      switch (execution_ptrs[tid]) {
        case WAIT:
          if constexpr (WAIT != 0) { // wait_flag == 0
            execution_ptrs[tid] = 0;
          }
          break;
        case QUIT:
          return;
        default:
          auto exec = std::bit_cast<Execution*>(execution_ptrs[tid]);
          DEBUGGING(++exec->worker_count;)
            exec->worker(tid, *exec);
          DEBUGGING(++proc);
          execution_ptrs[tid] = 0; // mark as done
          DEBUGGING(++fin; assert(proc == fin));
          // ++exec->joinable_count;
          break;
      }
    }
  }

  void init_threads() {
    threads.reserve(config.threads);
    THREAD_LOCAL_ID = 0;
    for (unsigned tid = 1; tid < config.threads; ++tid) {
      execution_ptrs[tid] = 0;
      threads.emplace_back(&self_t::worker_function, this, static_cast<int>(tid));
    }
  }

  // void wait_for_execution_change(Execution* seen) {
  //     execution_barrier.arrive_and_wait();
  // }

private:
  void change_execution(Execution* next) {
    current_execution.store(next);
    current_execution.notify_all();
  }

  static void set_niceness(int prio) {
    auto priores = setpriority(PRIO_PROCESS, 0, prio);
    if (priores < 0) {
      auto err = errno;
      throw "error while setting thread priority: " + std::string(strerror(err));
    }
  }

  static void set_affinity(int core) {
    pthread_t thread = pthread_self();
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    int res = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
      auto e = errno;
      throw std::logic_error{"error while setting thread affinity " + std::to_string(res) +
        "/" + std::string(strerror(e))};
    }
  }
};  // struct MaxisScheduler



struct TaskTiming {
  std::chrono::steady_clock::time_point available_time;
  std::chrono::steady_clock::time_point start_time;
  std::chrono::steady_clock::time_point end_time;
  unsigned thread_id;
};
struct scheduler {
  using scheduler_t = MaxisScheduler;
  using exec_t = scheduler_t::Execution;
  using morsel_t = scheduler_t::morsel_t;
  using config_t = scheduler_t::RuntimeConfig;
  using init = scheduler_t::Config;

  template<typename T, unsigned Align = 64>
  using tls = scheduler_t::tls<T, Align>;

  using range = morsel_t;

  static inline std::unique_ptr<scheduler_t> INSTANCE;

  scheduler() {
    ensure(!INSTANCE);
    INSTANCE = std::make_unique<scheduler_t>();
  }

  explicit scheduler(const scheduler_t::Config& cfg) {
    ensure(!INSTANCE);
    INSTANCE = std::make_unique<scheduler_t>(cfg);
  }

  static scheduler_t& instance() {
    return *INSTANCE.get();
  }

  static scheduler_t::Config default_config() {
    return {};
  }

  static scheduler_t::Config config() {
    if (!INSTANCE) { return {}; }
    return instance().config;
  }

  static unsigned thread_count() {
    assert(INSTANCE);
    return instance().config.threads;
  }

  template<typename F>
  static void execute(size_t count, F fn, const std::optional<config_t>& cfg = std::nullopt) {
    instance().execute(count, fn, cfg);
  }

  template<typename F>
  static void execute(size_t count, F fn, const std::function<void(config_t&)>& cfgfn) {
    auto& inst = instance();
    inst.execute(count, fn, inst.config.with(cfgfn));
  }

  template<typename F>
  static size_t get_optimal_morsel_size(size_t start, size_t end, F fn){
    size_t done = 0;
    size_t sz = 1;
    unsigned long long int ticks = 0;
    do {
        sz = std::min(sz, end - (start + done));
        auto tstart = std::chrono::steady_clock::now();
        fn(morsel_t(start + done, start + done + sz));
        auto tstop = std::chrono::steady_clock::now();
        ticks = static_cast<unsigned long long int>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(tstop - tstart).count()
        );
        done += sz;
        sz *= 2;
    } while (ticks < 10000000 && done < (end - start));
    return done;
  }



  template<typename F>
  static void parallel_for(const morsel_t& range, F fn, config_t override_cfg) {
    auto& inst = instance();
      size_t increment = 0; 
      auto cfg = override_cfg.with([&range, &inst,&increment,&fn](auto& r) {
      if (range.grainsize() != inst.config.morsel_size) {
        r.morsel_size = range.grainsize();
      } else {
        increment = get_optimal_morsel_size(range.begin(),range.end(),fn);
        r.morsel_size = std::max(1ul,increment); //std::max(1ul, range.size() / static_cast<size_t>(inst.config.threads  * 512));
      }
    });
    morsel_t adjusted_morsel = morsel_t(range.begin() + increment , range.end());
 //   std::cout<< "[DBG]: Morsel size: " << cfg.morsel_size << "\n"; 
 //   std::cout<< "[DBG]: increment size: " << increment << "\n";
    uint64_t offset = adjusted_morsel.begin();
    instance().execute(adjusted_morsel.size(), [&](int tid, exec_t& exec) {
      while(auto next = exec.next(tid)) {
        fn(morsel_t(offset + next->begin(), offset + next->end()));
      }
    }, cfg);
  }


  template<typename F>
  static void parallel_for_latency(const morsel_t& range, F fn, config_t override_cfg) {
    auto& inst = instance();
    auto cfg = override_cfg.with([&range, &inst](auto& r) {
      if (range.grainsize() != inst.config.morsel_size) {
        r.morsel_size = range.grainsize();
      } else {
        r.morsel_size = std::max(1ul, range.size() / static_cast<size_t>(inst.config.threads  * 128));
      }
    });
    uint64_t offset = range.begin();
    std::vector<std::vector<TaskTiming>> all_timings(inst.config.threads);
    std::atomic<size_t> next_task_id(0);

    // Record the time when tasks become available

    auto tasks_available_time = std::chrono::steady_clock::now();

    instance().execute(range.size(), [&](int tid, exec_t& exec) {
      std::vector<TaskTiming>& thread_timings = all_timings[tid];
      while( 1 ) {

        auto next = exec.next(tid);
        if(!next)break;
        size_t task_id = next_task_id.fetch_add(1);
        auto start = std::chrono::steady_clock::now();
        fn(morsel_t(offset + next->begin(), offset + next->end()));
        auto end = std::chrono::steady_clock::now();
        thread_timings.push_back({tasks_available_time, start, end, static_cast<unsigned>(tid)});
      }
    }, cfg);

    // Aggregate and analyze results
    std::vector<TaskTiming> all_task_timings;
    for (const auto& thread_timings : all_timings) {
      all_task_timings.insert(all_task_timings.end(), thread_timings.begin(), thread_timings.end());
    }

    // Calculate statistics for both scheduling and execution latency
    std::vector<std::chrono::nanoseconds> scheduling_latencies;
    std::vector<std::chrono::nanoseconds> execution_latencies;

    for (const auto& timing : all_task_timings) {
      scheduling_latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
        timing.start_time - timing.available_time));
      execution_latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
        timing.end_time - timing.start_time));
    }

    // Calculate and print statistics for both latencies
    auto  print_stats = [](const std::vector<std::chrono::nanoseconds>& latencies, const std::string& label) {
      // Create a non-const copy of the latencies for sorting
      std::vector<std::chrono::nanoseconds> sorted_latencies = latencies;
      std::sort(sorted_latencies.begin(), sorted_latencies.end(), [](const auto& a, const auto& b) { return a.count() < b.count(); });

      auto total = std::accumulate(sorted_latencies.begin(), sorted_latencies.end(), std::chrono::nanoseconds(0));
      auto avg = total / sorted_latencies.size();
      auto median = sorted_latencies[sorted_latencies.size() / 2];
      auto min = sorted_latencies.front();
      auto max = sorted_latencies.back();

      std::cout << label << " Latency Statistics:\n"
        << "  Average: " << avg.count() << " ns\n"
        << "  Median:  " << median.count() << " ns\n"
        << "  Min:     " << min.count() << " ns\n"
        << "  Max:     " << max.count() << " ns\n"
        << " NUMBER OF JOBS: " << sorted_latencies.size()  << "\n";
    };
    print_stats(scheduling_latencies, "Scheduling");
    print_stats(execution_latencies, "Execution");
  }




  template<typename F>
  static void parallel_for(const morsel_t& range, F fn) {
    parallel_for(range, fn, instance().config);
  }

  template<typename F>
  static void parallel_for_hinted(const morsel_t& range, const libdb::RefOrInstance<std::vector<size_t>>& hints, F fn, const std::function<void(config_t&)>& cfgfn = [](config_t& c){}) {
    auto& inst = instance();
    auto cfg = inst.config.with([&](auto& r) {
      cfgfn(r);
      r.morsel_hints = hints;
      if (range.grainsize() != inst.config.morsel_size) {
        r.morsel_size = range.grainsize();
      }
    });
    uint64_t offset = range.begin();
    instance().execute(range.size(), [&](int tid, exec_t& exec) {
      while(auto next = exec.next(tid)) {
        fn(morsel_t(offset + next->begin(), offset + next->end()));
      }
    }, cfg);
  }

  template<typename F>
  static void parallel_for(const morsel_t& range, F fn, const std::function<void(config_t&)>& cfgfn) {
    auto& inst = instance();
    auto cfg = inst.config.with([&](auto& r) {
      cfgfn(r);
      if (range.grainsize() != inst.config.morsel_size) { r.morsel_size = range.grainsize(); }
    });
    uint64_t offset = range.begin();
    instance().execute(range.size(), [&](int tid, exec_t& exec) {
      while(auto next = exec.next(tid)) {
        fn(morsel_t(offset + next->begin(), offset + next->end()));
      }
    }, cfg);
  }

  template<typename T, typename F>
  static void parallel_for_tls(tls<T>& tls, F fn, const std::function<void(config_t&)>& cfgfn = [](auto& cfg){}) {
    auto& inst = instance();
    inst.execute(tls.size(), [&](int tid, exec_t& exec) {
      while(auto next = exec.next(tid)) {
        for (auto i = next->begin(); i != next->end(); ++i) {
          if (tls.initialized(i)) {
            invoke_indexed_if_possible(fn, i, tls[i]);
          }
        }
      }
    }, inst.config.with(cfgfn));
  }

  template<typename It, typename F>
  static void parallel_for_each(It start, It end, F fn, const std::optional<config_t>& cfg = std::nullopt) {
    // only works for pointer-like iterators
    instance().execute(end - start, [&](int tid, exec_t& exec) {
      while(auto next = exec.next(tid)) {
        for (auto i = next->begin(); i != next->end(); ++i) {
          fn(start[i]);
        }
      }
    }, cfg);
  }

  template<typename It, typename F, typename CfgFn>
  static void parallel_for_each(It start, It end, F fn, CfgFn cfgfn) {
    auto& inst = instance();
    // only works for pointer-like iterators
    inst.execute(end - start, [&](int tid, exec_t& exec) {
      while(auto next = exec.next(tid)) {
        for (auto i = next->begin(); i != next->end(); ++i) {
          fn(start[i]);
        }
      }
    }, inst.config.with(cfgfn));
  }

  template<typename T, unsigned Align = 64>
  static tls<T, Align> make_local(typename tls<T, Align>::Constructor constructor = tls<T, Align>::default_constructor) {
    assert(INSTANCE);
    return INSTANCE->make_local<T, Align>(constructor);
  }
};

using sched = scheduler;

template<typename T, unsigned Align>
ThreadLocalVec<T, Align>::ThreadLocalVec(Constructor cons)
: ThreadLocalVec(sched::thread_count(), cons) {}
