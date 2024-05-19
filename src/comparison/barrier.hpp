// -----------------------------------------------------------------------------
// 2023 Maximilian Kuschewski
// -----------------------------------------------------------------------------
#pragma once
#include <atomic>

namespace libdb {

class Barrier {
    int max;
    std::atomic<int> arrived{0}, epoch{0};

public:
    explicit Barrier(int max)
        : max(max) {}

    void arrive_and_wait() {
        int cur_epoch = epoch.load();
        int expected = arrived.load();
        int next;

        // arrive; increase epcoh
        do {
            next = expected + 1 == max ? 0 : expected + 1;
        } while (!arrived.compare_exchange_weak(expected, next));

        if (next == 0) {
            ++epoch;
            epoch.notify_all();
            return;
        }
        // wait
        int seen_epoch{cur_epoch};
        while (seen_epoch == cur_epoch) {
            // XXX boost::yield-style backoff
            epoch.wait(seen_epoch);
            seen_epoch = epoch.load();
        }
    }
};
}  // namespace libdb
