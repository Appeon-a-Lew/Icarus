

#include <cassert>
#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <chrono>         // IWYU pragma: keep
#include <iostream>
#include <memory>
#include <numeric>
#include <oneapi/tbb/blocked_range.h>
#include <thread>
#include <type_traits>    // IWYU pragma: keep
#include <unistd.h>
#include <utility>
#include <vector>
#include <barrier>
#include <tbb/blocked_range.h>
#include "oneapi/tbb/detail/_task.h"
#include "split_deque.h"         // IWYU pragma: keep
#include "job.h"
#include "mailbox.h"
#include <oneapi/tbb/detail/_small_object_pool.h>

#define TIMEOUT 10000



#define DEBUG1 0
#define DEBUG_2 1


template <typename Job>
struct scheduler_ism{

  using worker_id_type = unsigned int;


  static_assert(std::is_invocable_r_v<void, Job&>);

  struct workerInfo {
    static constexpr worker_id_type UNINITIALIZED = std::numeric_limits<worker_id_type>::max();

    worker_id_type worker_id;
    scheduler_ism* my_scheduler;

    workerInfo() : worker_id(UNINITIALIZED), my_scheduler(nullptr){}
    workerInfo(std::size_t worker_id_, scheduler_ism* s) 
            : worker_id(worker_id_), my_scheduler(s) {
                    }    
    workerInfo& operator=(const workerInfo&) = delete;
    workerInfo(const workerInfo&) = delete;

   workerInfo(workerInfo&& other) noexcept
            : worker_id(std::exchange(other.worker_id, UNINITIALIZED)),
              my_scheduler(std::exchange(other.my_scheduler, nullptr))  {}

        workerInfo& operator=(workerInfo&& other) noexcept {
            if (this != &other) {
                worker_id = std::exchange(other.worker_id, UNINITIALIZED);
                my_scheduler = std::exchange(other.my_scheduler, nullptr);
            }
            return *this;
        }  
  };

  // After YIELD_FACTOR * P unsuccessful steal attempts, a
  // a worker will sleep briefly for SLEEP_FACTOR * P nanoseconds
  // to give other threads a chance to work and save some cycles.
  constexpr static size_t YIELD_FACTOR = 200;
  constexpr static size_t SLEEP_FACTOR = 200;

  // The length of time that a worker must fail to steal anything
  // before it goes to sleep to save CPU time.
  constexpr static std::chrono::microseconds STEAL_TIMEOUT{TIMEOUT};

  static inline thread_local workerInfo worker_info{};


  
  size_t hash(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return static_cast<size_t>(x);
  }

  void set_steal() {
    can_steal.store(true, std::memory_order_release);
  }

  const worker_id_type num_threads;

  static scheduler_ism* get_current_scheduler() {
    return worker_info.my_scheduler;
  }
  tbb::detail::d1::small_object_allocator allocator;
  explicit scheduler_ism(size_t num_workers)
      : num_threads(num_workers),
        num_deques(num_threads),
        num_awake_workers(num_threads),
        deques(num_threads),
        attempts(num_deques),
        spawned_threads(),
        finished_flag(false),
        can_steal(false),
        mail_inboxes(num_workers),
        mail_outboxes(num_workers),
        parent_worker_info(std::exchange(worker_info, workerInfo{0,this})),
        num_of_tasks(num_workers),
        senders(num_workers)
  {
    
    for(auto i = 0; i < num_workers; ++i){
      mail_outboxes[i] = new mail_outbox();
      mail_outboxes[i]->construct();
      mail_inboxes[i] = new mail_inbox();  
      mail_inboxes[i]->attach(*mail_outboxes[i]);
    }
    for (worker_id_type i = 1; i < num_threads; ++i) {
      spawned_threads.emplace_back([&, i]() { worker_info = {i, this}; worker(); });
    }
  }

  ~scheduler_ism() {
    shutdown();
    worker_info = std::move(parent_worker_info); 
    #ifdef profiling_stats
    
    long long total_cas = 0;
    long long total_fence = 0;
    long long total_pushBottom = 0;
    long long total_popBottom = 0;
    long long total_extract = 0;
    long long total_popTop = 0;
    long long total_insert = 0;
    long long total_mailbox_cas = 0; 
    for (const auto& deque : deques) {  // Assuming you have a way to get access to the deques
        total_cas += deque.cas;
        total_fence += deque.fence;
        total_pushBottom += deque.pushBottom;  // Pop bottom
        total_popBottom += deque.popBottom;  // Steal
        total_popTop += deque.popTop;  // Pop top
    } 

    for(const auto& mailbox : mail_outboxes){
      total_mailbox_cas += mailbox->cas; 
      total_extract += mailbox->extract;
      total_insert += mailbox->insert;
    }
    std::cout << "Profiling stats:" << std::endl;
    std::cout << "CAS operations:   " << total_cas << std::endl;
    std::cout << "Fence operations: " << total_fence << std::endl;
    std::cout << "Pop bottom (c1):  " << total_pushBottom << std::endl;
    std::cout << "Pop Bottom:       " << total_popBottom << std::endl;
    std::cout << "Pop top (c3):     " << total_popTop << std::endl;
    std::cout << "mailbox insert:   " << total_insert<< std::endl;
    std::cout << "mailbox extract   " << total_extract<< std::endl;
    std::cout << "mailbox cas       " << total_mailbox_cas<< std::endl;


    
#endif
  }

  // Push onto local stack.
  void spawn(Job* job) {
    int id = worker_id();
    //if(deques[id].size() > 9990)
    //felicity::safe_cout << "The deque is " << deques[id].size() << " ,id: " << id<<   "\n";

    [[maybe_unused]] bool first = deques[id].push_bottom(job);

  }
    




  template <typename F>
  void wait_until(F&& done, bool conservative = false) {
    if (conservative) {
      while (!done())
        std::this_thread::yield();
    }
    else {
      do_work_until(std::forward<F>(done));
    }
  }

  Job* get_own_job() {
    auto id = worker_id();
    if(mail_inboxes[id]->empty()){
      
    //felicity::safe_cout << "my inbox is empty, id: " << id << "\n" 
    //                 << "size of my deque: " << deques[id].size() << "\n\n";
      while(auto* job = deques[id].pop_bottom()){
        task_proxy* tmp = dynamic_cast<task_proxy*>(job);
        if(tmp){
          if(auto* result = tmp->extract_task<task_proxy::pool_bit>()){
           // felicity::safe_cout << "extract_task Succesfully\n";
          //std::cout << "DEQUE\n";
            return result;
          }

        }
        //felicity::safe_cout << "extract_task failed\n";
        allocator.delete_object(tmp);
      }
      return nullptr;
    }
    else{
     //felicity::safe_cout << "my inbox is not empty im using it, id: " << id << "\n"
      //                 << "size of my deque: " << deques[id].size() << "\n\n";
      while (task_proxy* const tp = mail_inboxes[id]->pop()) {
       // felicity::safe_cout << "trying to get proxy "<<id <<"\n\n";
        if (auto* result = tp->extract_task<task_proxy::mailbox_bit>()) {
          //felicity::safe_cout << "Succesfully!\n";
          //std::cout << "MAILBOX\n";
          return result;
        }
        // We have exclusive access to the proxy, and can destroy it.
        //felicity::safe_cout << "Aborted\n";
        allocator.delete_object(tp); 
      }
      return nullptr;
    } 
  }


  worker_id_type num_workers() { return num_threads; }
  worker_id_type worker_id() { return worker_info.worker_id; }

  bool finished() const noexcept {
    return finished_flag.load(std::memory_order_acquire);
  }
 

  
  worker_id_type get_spawn_id_mailbox_random() {
        auto target_id = (hash(worker_id()+1) + hash(attempts[worker_id()].val++)+1) % (num_deques);
        target_id = target_id == worker_id() ? (target_id + 1) % (num_threads) : target_id;
        if(senders[target_id]) return get_spawn_id_mailbox_random();
        else return target_id;
  }


  std::vector<mail_outbox*> mail_outboxes; 
  std::vector<mail_inbox*> mail_inboxes; 
  int num_deques;
  
  std::vector<Deque<Job>> deques;

  std::vector<int> num_of_tasks;
  std::vector<int> senders;
private:
  // Align to avoid false sharing.
  struct alignas(128) attempt {
    size_t val;
  };
  std::atomic<size_t> num_awake_workers;
  workerInfo parent_worker_info;
   std::vector<attempt> attempts;
  std::vector<std::thread> spawned_threads;
  std::atomic<int> finished_flag;
  std::atomic<int> can_steal;
  std::atomic<size_t> wake_up_counter{0};
  std::atomic<size_t> num_finished_workers{0};



void worker() {
    while (!finished()) {
      Job* job = get_job([&]() { return finished(); },false);
      if (job)(*job)();
    }
    assert(finished());
    num_finished_workers.fetch_add(1);
  }


  // Runs tasks until done(), stealing work if necessary.
  //
  // Does not sleep or time out since this can be called
  // by the main thread and by join points, for which sleeping
  // would cause deadlock, and timing out could cause a join
  // point to resume execution before the job it was waiting
  // on has completed.
  template <typename F>
  void do_work_until(F&& done) {    
  #ifdef DEBUG 
    felicity::safe_cout << "[DBG]: Do work until has the id " << worker_id() << "\n";
    felicity::safe_cout << "[DBG]: The size of own work_deque is " << deques[worker_id()].size() << "\n";
  #endif
    while (true) {
      Job* job = get_job(done, false);  // timeout MUST BE false
      if (!job) return;
      (*job)();
    }
    assert(done());
  }

  template <typename F>
  Job* get_job(F&& break_early, bool timeout) {
    if (break_early()) return nullptr;
    Job* job = get_own_job();
    if (job) return job;
    //else std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    //job = get_own_job();
    //if(job) return job;
    //else std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
    //job = get_own_job();
    //if(job) return job;
    else{
      //std::cout << "STEALING\n";
      job = steal_job(std::forward<F>(break_early), timeout);
    }
    return job;
  }
  
  // Find a job with random steals.
  //
  // Returns nullptr if break_early() returns true before a job
  // is found, or, if timeout is true and it takes longer than
  // STEAL_TIMEOUT to find a job to steal.
  template<typename F>
  Job* steal_job(F&& break_early, bool timeout) {
    size_t id = worker_id();
    const auto start_time = std::chrono::steady_clock::now();
    do {
      // By coupon collector's problem, this should touch all.
      for (size_t i = 0; i <= YIELD_FACTOR * num_deques; i++) {
        if (break_early()) return nullptr;
        Job* job = try_steal(id);
        if (job) return job;
      }
      std::this_thread::sleep_for(std::chrono::nanoseconds(num_deques * 100));
    } while (!timeout || std::chrono::steady_clock::now() - start_time < STEAL_TIMEOUT);
    return nullptr;
  }

  Job* try_steal(size_t id) {
    // use hashing to get "random" target
    size_t target = (hash(id) + hash(attempts[id].val)) % num_deques;
    //felicity::safe_cout << "trying to steal\n";
    attempts[id].val++;
    while(1){
      auto [job, empty] = deques[target].pop_top();
      if(!job) break;
      task_proxy* tmp  = dynamic_cast<task_proxy*>(job);
      if(tmp) {
        if(auto* result =  tmp->extract_task<task_proxy::pool_bit>()){
          //felicity::safe_cout << "Succesfully!\n";
          return result; 
        }
        allocator.delete_object(tmp);
        //delete tmp;
      }
    }
    //felicity::safe_cout << "Aborted\n";
    return nullptr ;
  }

  void shutdown() {
    finished_flag.store(true, std::memory_order_release);
    for (worker_id_type i = 1; i < num_threads; ++i) {
      spawned_threads[i - 1].join();
    }
  }
};

static int depth = 0; 

class fork_join_scheduler {
  using Job = WorkStealingJob;
  using scheduler_t = scheduler_ism<Job>;

public:
  template <typename L, typename R>
  static void pardo(scheduler_t& scheduler, L&& left, R&& right, bool conservative = false, bool use_numa = false) {
   


    //std::cout << cnt++ << std::endl;

    //auto execute_right = [&]() { std::forward<R>(right)(); };
    auto right_job = make_job(right);

    // Create a task_proxy forthe right job
    task_proxy* proxy = scheduler.allocator.new_object<task_proxy>();
    // Decide which thread to send the proxy to
    auto target_id = scheduler.get_spawn_id_mailbox_random();
       // Set up the proxy
    //felicity::safe_cout << "the target_id is " << target_id << "\n";
    proxy->task_and_tag = (intptr_t)(&right_job) |  task_proxy::location_mask;
    proxy->outbox = (scheduler.mail_outboxes[target_id]);
    proxy->slot = target_id;
    //scheduler.mail_outboxes[target_id]->push(proxy);
    proxy->outbox->push(proxy);
    //cnt++;
    //if(cnt > 1) scheduler.set_steal(); 
    //felicity::safe_cout << "mailboxed to " << target_id << " by the tid: " << scheduler.worker_id() <<"\n";
    // Push the proxy to the target mailbox
    scheduler.spawn(proxy);
    //scheduler.num_of_tasks[target_id]++;
    //scheduler.senders[scheduler.worker_id()]++;
    // Execute the left job
    std::forward<L>(left)();

    // Wait for the right job to finish
    auto done = [&]() { return right_job.finished(); };
    scheduler.wait_until(done, conservative);
    assert(right_job.finished());

    // The proxy will be cleaned up by the thread that executes it
  }
 template <typename F>
  static void parfor(scheduler_t& scheduler, size_t start, size_t end, F&& f, size_t granularity = 0, bool conservative = false) {
    if (end <= start) return;
    if (granularity == 0) {
      size_t done = get_granularity(start, end, f);
      granularity = std::max(done, (end - start) / static_cast<size_t>(128 * scheduler.num_threads));
      start += done;
    }
    parfor_(scheduler, start, end, f,granularity, conservative);
    //std::cout << "Tasks\n";
    //for(auto i = 0; i<scheduler.num_of_tasks.size() ;i++){
    //  std::cout << scheduler.num_of_tasks[i] << " ";
    //}
    //std::cout << "\n Senders:\n";
    //for(auto i = 0; i<scheduler.num_of_tasks.size() ;i++){
    //  std::cout << scheduler.senders[i] << " ";
    //}
    

  } 

  
 private:
  template <typename F>
  static size_t get_granularity(size_t start, size_t end, F& f) {
    size_t done = 0;
    size_t sz = 1;
    unsigned long long int ticks = 0;
    do {
      sz = std::min(sz, end - (start + done));
      auto tstart = std::chrono::steady_clock::now();
//      for (size_t i = 0; i < sz; i++) f(start + done + i);
      f(tbb::blocked_range<size_t>(start + done, start + done + sz));
      auto tstop = std::chrono::steady_clock::now();
      ticks = static_cast<unsigned long long int>(std::chrono::duration_cast<
                std::chrono::nanoseconds>(tstop - tstart).count());
      done += sz;
      sz *= 2;
    } while (ticks < 1000 && done < (end - start));
    return done;
  }

  template <typename F>
  static void parfor_(scheduler_t& scheduler, size_t start, size_t end, F& f, size_t granularity, bool conservative) {
    if ((end - start) <= granularity){  
      #ifdef DEBUG_
      felicity::safe_cout << "Doing the range: [" << start << ", "<< end << "]\n";
      #endif // DEBUG
      //for (size_t i = start; i < end; i++) f(i);
      f(tbb::blocked_range<size_t>(start,end));
    }else {
      size_t n = end - start;
      // Not in middle to avoid clashes on set-associative caches on powers of 2.
      size_t mid = (start + granularity);
      pardo(scheduler,
            [&]() { parfor_(scheduler, mid, end, f, granularity, conservative); },
            [&]() { parfor_(scheduler, start,mid, f, granularity, conservative); },
            conservative);
    }
  }
 }; 

