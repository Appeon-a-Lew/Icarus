#include <iostream>
#include <cstdlib>
#include <chrono>
#include <tbb/tbb.h>
#include <vector>
#include <functional>

static int w, n;

int loop() {
    int s = 0;
    for (int i = 0; i < n; i++) {
        s += i;
    }
    return s;
}

void tree(int depth) {
    if (depth > 0) {
       tbb::parallel_invoke(
            [=] { tree(depth - 1); },
            [=] { tree(depth - 1); },
            [=] { tree(depth - 1); },
            [=] { tree(depth - 1); }
        );    } else {
        loop();
    }
}

double wctime() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() * 1e-9;
}

void usage(const char* s) {
    std::cerr << s << " <depth> <width> <grain> <reps>\n";
}

int main(int argc, char** argv) {
    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }

    int d = std::atoi(argv[1]);
    w = std::atoi(argv[2]);
    n = std::atoi(argv[3]);
    int m = std::atoi(argv[4]);

    std::cout << "Running parallel depth first search on " << m 
              << " balanced trees with depth " << d 
              << ", width " << w << ", grain " << n << ".\n";

    double t1 = wctime();


    tbb::parallel_for(0, m, [d](int) {
        tree(d);
    });

    double t2 = wctime();
    std::cout << "Time: " << t2 - t1 << " seconds\n";

    return 0;
}
