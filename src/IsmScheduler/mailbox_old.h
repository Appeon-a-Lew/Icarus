#include <queue>
#include <mutex>
#include <condition_variable>
#include "mailbox_queue.h"
template <typename Job>
class Mailbox {
private:
    riften::Deque<Job*> tasks;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void send(Job* job) {
        if(job != nullptr)
        tasks.emplace(job);
    }

    Job* receive() {
        std::optional job = tasks.steal();
        return job.value_or(nullptr);
    }

    bool try_receive(Job*& job) {
        if (!tasks.empty()) {
            std::optional job_ptr = tasks.steal();
            if(job_ptr.has_value()){job= job_ptr.value();}
            else{job=nullptr; return false;}
            return true;
        }
        return false;
    }

   size_t size(){return tasks.size();}
};
