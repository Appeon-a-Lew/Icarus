#pragma once
#include <atomic>
#include <cstdint>
#include <thread>
#include "job.h"
#include "cacheline.h"

#define profiling_stats1 1
class mail_outbox;

struct task_proxy : public WorkStealingJob {
    static const intptr_t pool_bit = 1<<0;
    static const intptr_t mailbox_bit = 1<<1;
    static const intptr_t location_mask = pool_bit | mailbox_bit;
    using worker_id_type = unsigned long; 
    std::atomic<intptr_t> task_and_tag;
    std::atomic<task_proxy*> next_in_mailbox;
    std::atomic<bool> is_accessed{false};
    mail_outbox* outbox;
    worker_id_type slot;

    static bool is_shared(intptr_t tat) {
        return (tat & location_mask) == location_mask;
    }

    static WorkStealingJob* task_ptr(intptr_t tat) {
        return (WorkStealingJob*) (tat & ~location_mask) ;
    }
    bool is_accessed_(){
      return is_accessed.load() == true;
    }

    template<intptr_t from_bit>
    WorkStealingJob* extract_task() {
        intptr_t tat = task_and_tag.load(std::memory_order_acquire);
        if (tat != from_bit) {
            const intptr_t cleaner_bit = location_mask & ~from_bit;
            if (task_and_tag.compare_exchange_strong(tat, cleaner_bit)) {
                //is_accessed.store(true);
                return task_ptr(tat);
            }
        }
        return nullptr;
    }

    void execute() override {
     // std::cerr << "internal error: scheduler tried to execute a task_proxy\n";
      std::abort();

    }
};

class unpadded_mail_outbox {
public:
    typedef std::atomic<task_proxy*> atomic_proxy_ptr;

    //! Pointer to first task_proxy in mailbox, or nullptr if box is empty.
    atomic_proxy_ptr my_first;

    //! Pointer to pointer that will point to next item in the queue.  Never nullptr.
    std::atomic<atomic_proxy_ptr*> my_last;

    //! Owner of mailbox is not executing a task, and has drained its own task pool.
    std::atomic<bool> my_is_idle;


};



// TODO: - consider moving to arena slot
//! Class representing where mail is put.
/** Padded to occupy a cache line. */
class mail_outbox : libdb::pad_to_cacheline<unpadded_mail_outbox> {
public:

#ifdef profiling_stats 
  long long cas = 0ll;
  long long fence = 0ll; 
  long long extract = 0ll;
  long long insert = 0ll;
#endif // 

    void construct() {
        my_first.store(nullptr, std::memory_order_relaxed);
        my_last.store(&my_first, std::memory_order_relaxed);
        my_is_idle.store(false, std::memory_order_relaxed);
    }

    void push(task_proxy* t) {
        t->next_in_mailbox.store(nullptr, std::memory_order_relaxed);
        atomic_proxy_ptr* const link = my_last.exchange(&t->next_in_mailbox);
        link->store(t, std::memory_order_release);
        #ifdef profiling_stats
          insert++;
          
        #endif 
    }

    bool empty() {
        
        return my_first.load(std::memory_order_relaxed) == nullptr;
    }

    void drain() {
        for (task_proxy* t = my_first.load(std::memory_order_relaxed); t; ) {
            task_proxy* next = t->next_in_mailbox.load(std::memory_order_relaxed);
            //delete t;
            t = next;
        }
        my_first.store(nullptr, std::memory_order_relaxed);
        my_last.store(&my_first, std::memory_order_relaxed);
    }

    bool recipient_is_idle() {
        return my_is_idle.load(std::memory_order_relaxed);
    }
    
  friend class mail_inbox;
private:

    task_proxy* internal_pop() {
        task_proxy* curr = my_first.load(std::memory_order_acquire);
        if (!curr)
            return nullptr;
        atomic_proxy_ptr* prev_ptr = &my_first;
        
        // There is a first item in the mailbox. See if there is a second.
        task_proxy* second = curr->next_in_mailbox.load(std::memory_order_acquire);
        if (second) {
            // There are at least two items, so first item can be popped easily.
            prev_ptr->store(second, std::memory_order_relaxed);
        } else {
            // There is only one item. Some care is required to pop it.
            prev_ptr->store(nullptr, std::memory_order_relaxed);
            atomic_proxy_ptr* expected = &curr->next_in_mailbox;
            if (my_last.compare_exchange_strong(expected, prev_ptr)) {
                // Successfully transitioned mailbox from having one item to having none.
                assert(curr->next_in_mailbox.load(std::memory_order_relaxed) == nullptr);
            } else {
                // Some other thread updated my_last but has not filled in first->next_in_mailbox
                // Wait until first item points to second item.
                while (!(second = curr->next_in_mailbox.load(std::memory_order_acquire))) {
                    std::this_thread::yield();  // Or use a more sophisticated backoff strategy
                }
                prev_ptr->store(second, std::memory_order_relaxed);
            }
        }
        assert(curr != nullptr);
        #ifdef profiling_stats
         extract++; 
         cas++;
        #endif // DEBUG
        return curr;
    }

};



class mail_inbox{
  //! Corresponding sink where mail that we receive will be put.
    mail_outbox* my_putter;
public:
    //! Construct unattached inbox
    mail_inbox() : my_putter(nullptr) {}

    //! Attach inbox to a corresponding outbox.
    void attach( mail_outbox& putter ) {
        my_putter = &putter;
    }
    //! Detach inbox from its outbox
    void detach() {
        //if previosly non null wtf are you trying to achieve
        assert(my_putter != nullptr);
        my_putter = nullptr;
    }
    //! Get next piece of mail, or nullptr if mailbox is empty.
    task_proxy* pop(  ) {
        return my_putter->internal_pop( );
    }
    //! Return true if mailbox is empty
    bool empty() {
        return my_putter->empty();
    }
    //! Indicate whether thread that reads this mailbox is idle.
    /** Raises assertion failure if mailbox is redundantly marked as not idle. */
    void set_is_idle( bool value ) {
        if( my_putter ) {
            //cannot redundantly mark as not idle.
            assert(my_putter->my_is_idle.load(std::memory_order_relaxed) || value);
            my_putter->my_is_idle.store(value, std::memory_order_relaxed);
        }
    }
    //! Indicate whether thread that reads this mailbox is idle.
    bool is_idle_state ( bool value ) const {
        return !my_putter || my_putter->my_is_idle.load(std::memory_order_relaxed) == value;
    }
};
