
// LCWS SIGNAL
//

#ifndef PARLAY_SCHEDULER_H_
#define PARLAY_SCHEDULER_H_

#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits> // IWYU pragma: keep
#include <vector>
#include <condition_variable>
#include <mutex>
#include <fstream>
#include <csignal>
#include <functional>
#include <execinfo.h>
#include <unistd.h>
#include <signal.h>

#include "../job.h"

#define RACE nullptr
#define PRIVATE_WORK reinterpret_cast<Job *>(1)
#define profiling_stats 
#define ABORT nullptr

namespace parlay {

    // Deque from Arora, Blumofe, and Plaxton (SPAA, 1998).
    template <typename Job>
    struct Deque {
        using qidx = unsigned int;
        using tag_t = unsigned int;

        // use std::atomic<age_t> for atomic access.
        // Note: Explicit alignment specifier required
        // to ensure that Clang inlines atomic loads.
        struct alignas(int64_t) age_t {
            tag_t tag;
            qidx top;
        };

        // align to avoid false sharing
        struct alignas(64) padded_job {
            Job *job;
        };

        static constexpr int q_size = 1000;
        volatile qidx public_bot;
        volatile qidx bot;
        std::atomic<age_t> age;
        std::array<padded_job, q_size> deq;

        /**
         * Flag that indicates if a worker has been targeted for work exposure
         */
        bool targeted;

#ifdef profiling_stats
        int c1, c2, c3, c4;
        long long cas, fence;
#endif

        int id;

        Deque(): public_bot(1), bot(1),
#ifdef profiling_stats
            c1(0), c2(0), c3(0), c4(0), cas(0), fence(0),
#endif
            targeted(false), age(age_t { 0,1}) {}

        /**
         * Synchronization-free push of a job to the bottom of the private part of the deque
         * @param job
         */
  void push_bottom(Job * job) {
    deq[bot].job = job;
    bot += 1;
    targeted = false;

    if (bot >= q_size) {
      throw std::runtime_error("internal error: scheduler queue overflow");
    }
  }


  /**
         * Synchronization-free removal and return of the bottom-most node of the deque’s private part.
         * If the deque is empty, returns a null pointer
         *
         */
  Job* pop_bottom() {
    --bot;
    if (bot < public_bot) {
      targeted = true;
      return nullptr;
    }

#ifdef profiling_stats
    c3++;
#endif

    return deq[bot].job;
  }

  /**
         * Attempts to remove and return the top-most node of the deque’s public part.
         * If the operation aborts, it has no effect and returns ABORT.
         * If only the public part of the deque is empty it returns the PRIVATE_WORK special value
         */
  Job* pop_top() {
    auto old_age = age.load(std::memory_order_relaxed);

    if (public_bot > old_age.top) {
      auto job = deq[old_age.top].job;
      auto new_age = old_age;
      new_age.top = new_age.top + 1;
      if (age.compare_exchange_strong(old_age, new_age, std::memory_order_relaxed, std::memory_order_relaxed)) {
#ifdef profiling_stats
        c2++;
        cas++;

#endif
        targeted = false;
        return job;
      }
#ifdef profiling_stats
      cas++;
#endif // DEBUG
      return ABORT;
    }
    return (public_bot < bot) ? PRIVATE_WORK : nullptr;
  }

  /**
         * Transfer the top-most node of the deque’s private part to the bottom of the public part.
         */
  void update_public_bottom() {

    if (public_bot < bot) {

#ifdef profiling_stats
      c4++;
#endif
      public_bot++;
    }
  }

  /** 
         * Pop a job from the public part of the deque.
         * The targeted flag is to false if a job is popped.
         */
  Job* pop_public_bottom() {
    Job* result = nullptr;

    if(public_bot != 1) {
      public_bot--;
      auto b = public_bot;

#ifdef profiling_stats
      fence++;
#endif
      std::atomic_thread_fence(std::memory_order_seq_cst);
      auto job = deq[b].job;
      auto old_age = age.load(std::memory_order_relaxed);

      if (b > old_age.top) {
        bot = b;
        result = job;
        targeted = false;

      } else {
        bot = 1;
        auto age_new = age_t { old_age.tag + 1, 1 };
        public_bot = 1;

        if (b == old_age.top){
          if(age.compare_exchange_strong(old_age, age_new, std::memory_order_relaxed, std::memory_order_relaxed)){
#ifdef profiling_stats
            cas++;
#endif // DEBUG
            result = job;
            targeted = false;
          } 
          #ifdef profiling_stats 
          else{
            cas++;
          }
          #endif // 

        } else {
          age.store(age_new, std::memory_order_relaxed);
          result = nullptr;
        }
#ifdef profiling_stats
        fence++;
#endif
        std::atomic_thread_fence(std::memory_order_seq_cst);
      }
    }
    else bot = 1;

#ifdef profiling_stats
    if (result != nullptr)
      c1++;
#endif

    return result;
  }
};

    template <typename Job >
    struct scheduler {
    public:
        // see comments under wait(..)
        static bool
        const conservative = false;
        unsigned int num_threads;

        static thread_local unsigned int thread_id;

        static void signal_handler(int sig) {
            static_deques->at(thread_id).update_public_bottom();
        }

        scheduler(): num_threads(init_num_workers()),
                     num_deques( num_threads),
                     deques(num_deques),
#ifdef profiling_stats
                     file("lcws_stats.txt", std::ios_base::app),
                     c_private_work(num_deques, 0),
                     print1(false),
                     c_sleep(num_deques, 0),
#endif
                     attempts(num_deques),
                     spawned_threads(),
                     thread_handles(num_threads),
                     finished_flag(false)
        {

            static_deques = &deques;
            std::signal(SIGUSR1, scheduler<Job>::signal_handler);

            auto finished = [this]() {
                return finished_flag.load(std::memory_order_relaxed);
            };

            thread_id = 0;
            thread_handles[0] = pthread_self();

            for (unsigned long i = 1; i < num_threads; i++) {
                spawned_threads.emplace_back([ & , i, finished]() {
                    thread_id = i;
                    start(finished);
                });
                thread_handles[i] = spawned_threads[i - 1].native_handle();
            }
        }

        ~scheduler() {
            finished_flag.store(true, std::memory_order_relaxed);
            std::signal(SIGUSR1, SIG_IGN);
            for (unsigned int i = 1; i < num_threads; i++) {
                spawned_threads[i - 1].join();
            }


#ifdef profiling_stats
            auto a = 0; //pop_public_bottom
            auto b = 0; //steal
            auto c = 0; //failed steals
            auto d = 0; //pop
            auto e = 0; //update_public_bottom

            long long cas = 0;
            long long fence = 0;

            auto private_work = 0;

            for(int i = 0; i < num_deques; i++) {
                a += deques[i].c1;
                b += deques[i].c2;
                c += c_sleep[i];
                d += deques[i].c3;
                e += deques[i].c4;

                cas += deques[i].cas;
                fence += deques[i].fence;
                private_work += c_private_work[i];
            }

            file << "profiling stats\n";
            file << std::to_string(fence) + ", " + std::to_string(cas) + ", " + std::to_string(a) + ", " +
                            std::to_string(b) + ", " + std::to_string(c) + ", " + std::to_string(d) + ", " + 
                            std::to_string(e) + ", " + std::to_string(private_work) + "\n";
            file.close();
#endif
        }

        // Push onto local stack.
        void spawn(Job * job) {
            int id = worker_id();
            deques[id].push_bottom(job);
        }

        // Wait for condition: finished().
        template <typename F>
        void wait(F finished, bool conservative = false) {
            // Conservative avoids deadlock if scheduler is used in conjunction
            // with user locks enclosing a wait.
            if (conservative) {
                while (!finished())
                    std::this_thread::yield();
            }
                // If not conservative, schedule within the wait.
                // Can deadlock if a stolen job uses same lock as encloses the wait.
            else {
                start(finished);
            }
        }

        // All scheduler threads quit after this is called.
        void finish() {
            finished_flag.store(true, std::memory_order_relaxed);
        }

        /**
         * Try to pop from local stack. When not successful resort to the public part of the deque
         *
         */
        Job * try_pop() {
            auto id = worker_id();
            auto job = deques[id].pop_bottom();

            if (!job)
                job = deques[id].pop_public_bottom();

            return job;
        }

#ifdef _MSC_VER
        #pragma warning(push)
      #pragma warning(disable: 4996) // 'getenv': This function or variable may be unsafe.
#endif

        // Determine the number of workers to spawn
        unsigned int init_num_workers() {
            if (const auto env_p = std::getenv("PARLAY_NUM_THREADS")) {
                return std::stoi(env_p);
            } else {
                return std::thread::hardware_concurrency();
            }
        }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

        unsigned int num_workers() {
            return num_threads;
        }
        unsigned int worker_id() {
            return thread_id;
        }
        void set_num_workers(unsigned int) {
            std::cout << "Unsupported" << std::endl;
            exit(-1);
        }

        std::vector<Deque<Job>>* get_deques(){
          return &deques;
        }


    private:
        // Align to avoid false sharing.
        struct alignas(128) attempt {
            size_t val;
        };
        int num_deques;
        std::vector<Deque<Job>> deques;
        std::vector<attempt> attempts;
        std::vector<std::thread> spawned_threads;
        std::vector<pthread_t> thread_handles;
        std::atomic<int> finished_flag;

#ifdef profiling_stats
        std::vector<int> c_private_work;
        std::vector<int> c_sleep;
        std::atomic<bool> print1;
        std::ofstream file;
#endif

        static std::vector<Deque<Job>>* static_deques;

        template <typename F >
        void start(F finished) {
            while (true) {
                Job* job = get_job(finished);

                if (!job)
                    return;
                (*job)();
            }
        }

        /**
         * Try to steal a task. Returns nullptr when not successful
         */
        Job* try_steal(size_t id) {
            // use hashing to get "random" target
            size_t target = (hash(id) + hash(attempts[id].val)) % num_deques;
            attempts[id].val++;
            auto job = deques[target].pop_top();

            if (job == PRIVATE_WORK) {
                if (!deques[target].targeted) {
                    deques[target].targeted = true;
                    pthread_kill(thread_handles[target], SIGUSR1);
                }
#ifdef profiling_stats
                c_private_work[id]++;
#endif
                return nullptr;
            }

#ifdef profiling_stats1
            if (job)
                deques[id].cas++;
            else if (job == ABORT) {
                deques[id].cas++;
                return nullptr;
            }
#endif
            return job;
        }

        // Find a job, first trying local stack, then random steals.
        template <typename F>
        Job * get_job(F finished) {

            if (finished())
                return nullptr;
            Job * job = try_pop();
            if (job)
                return job;

            auto id = worker_id();
            while (true) {
                // By coupon collector's problem, this should touch all.
                for (int i = 0; i <= num_deques * 100; i++) {
                    if (finished())
                        return nullptr;

                    job = try_steal(id);
                    if (job)
                        return job;

#ifdef profiling_stats
                    c_sleep[id]++;
#endif
                }
                // If haven't found anything, take a breather.
                std::this_thread::sleep_for(std::chrono::nanoseconds(num_deques * 100));
            }
        }

        size_t hash(uint64_t x) {
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            x = x ^ (x >> 31);
            return static_cast < size_t > (x);
        }
    };

    template <typename T >
    thread_local unsigned int scheduler < T > ::thread_id = 0;

    template <typename T >
    std::vector<Deque<T>>* scheduler<T>::static_deques = nullptr;

    class fork_join_scheduler {
        using Job = WorkStealingJob;

        // Underlying scheduler object
        std::unique_ptr < scheduler < Job >> sched;


    public:
        fork_join_scheduler(): sched(std::make_unique < scheduler < Job >> ()) {}


        unsigned int num_workers() {
            return sched -> num_workers();
        }
        unsigned int worker_id() {
            return sched -> worker_id();
        }
        void set_num_workers(int n) {
            sched -> set_num_workers(n);
        }
        auto get_deques(){
          return sched->get_deques();
        }

        // Fork two thunks and wait until they both finish.
        template <typename L, typename R >
        void pardo(L left, R right, bool conservative = false) {
            auto right_job = make_job(right);
            sched -> spawn( & right_job);
            left();
            if (sched -> try_pop() != nullptr)
                right();
            else {
                auto finished = [ & ]() {
                    return right_job.finished();
                };
                sched -> wait(finished, conservative);
            }
        }

#ifdef _MSC_VER
        #pragma warning(push)
    #pragma warning(disable: 4267) // conversion from 'size_t' to *, possible loss of data
#endif

        template <typename F >
        size_t get_granularity(size_t start, size_t end, F f) {
            size_t done = 0;
            size_t sz = 1;
            int ticks = 0;
            do {
                sz = std::min(sz, end - (start + done));
                auto tstart = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < sz; i++)
                    f(start + done + i);
                auto tstop = std::chrono::high_resolution_clock::now();
                ticks = static_cast < int > ((tstop - tstart).count());
                done += sz;
                sz *= 2;
            } while (ticks < 1000 && done < (end - start));
            return done;
        }

        template <typename F>
        void parfor(size_t start, size_t end, F f, size_t granularity = 0,
                    bool conservative = false) {
            if (end <= start)
                return;
            if (granularity == 0) {
                size_t done = get_granularity(start, end, f);
                granularity = 16;
                // granularity = std::max(done, (end - start) / (128 * sched->num_threads));
                parfor_(start + done, end, f, granularity, conservative);
            } else
                parfor_(start, end, f, granularity, conservative);
        }

    private:
        template <typename F >
        void parfor_(size_t start, size_t end, F f, size_t granularity,
                     bool conservative) {
            if ((end - start) <= granularity)
                for (size_t i = start; i < end; i++)
                    f(i);
            else {
                size_t n = end - start;
                // Not in middle to avoid clashes on set-associative
                // caches on powers of 2.
                size_t mid = (start + (9 * (n + 1)) / 16);
                pardo([ & ]() {
                          parfor_(start, mid, f, granularity, conservative);
                      },
                      [ & ]() {
                          parfor_(mid, end, f, granularity, conservative);
                      },
                      conservative);
            }
        }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
    };

} // namespace parlay

#endif // PARLAY_SCHEDULER_H_
