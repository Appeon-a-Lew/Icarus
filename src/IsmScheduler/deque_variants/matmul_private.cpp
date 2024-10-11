
#include <cstddef>
#include <iostream>
#include <vector>
#include <chrono>
#include "signalvariant.h"  // Include the parlay scheduler header

using namespace std;
using namespace parlay;  // Use the parlay namespace for scheduler

// Function for matrix multiplication in a serial manner (for reference)
void serial_matrix_multiply(const vector<vector<int>>& A, const vector<vector<int>>& B, vector<vector<int>>& C) {
    size_t N = A.size();
    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < N; j++) {
            C[i][j] = 0;
            for (size_t k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// Parallel matrix multiplication using parlay scheduler
void parallel_matrix_multiply_parlay(const vector<vector<int>>& A, const vector<vector<int>>& B, vector<vector<int>>& C, fork_join_scheduler& scheduler) {
    size_t N = A.size();

    // Use parlay::scheduler's parfor to parallelize the outer loop
    scheduler.parfor(0, N, [&](size_t i) {
        for (size_t j = 0; j < N; ++j) {
            C[i][j] = 0;
            for (size_t k = 0; k < N; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    });
}

bool verify_results(const vector<vector<int>>& C, const vector<vector<int>>& C_serial) {
    size_t N = C.size();
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
            if (C[i][j] != C_serial[i][j]) {
                return false;  // Mismatch found
            }
        }
    }
    return true;  // All elements match
}

int main() {
    fork_join_scheduler scheduler;  // Initialize parlay scheduler

    for (size_t N = 1000; N <= 2200; N += 100) {
        vector<vector<int>> A(N, vector<int>(N));
        vector<vector<int>> B(N, vector<int>(N));
        vector<vector<int>> C1(N, vector<int>(N));
        vector<vector<int>> C2(N, vector<int>(N));

        // Initialize matrices A and B
        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < N; j++) {
                A[i][j] = i + j;
                B[i][j] = i * j;
            }
        }

        auto a_clone = A;
        auto b_clone = B;

        auto start = std::chrono::high_resolution_clock::now();
        // Perform parallel matrix multiplication using parlay scheduler
        parallel_matrix_multiply_parlay(a_clone, b_clone, C1, scheduler);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        auto time_parlay = diff.count();

        // Output the time taken for the parlay scheduler multiplication
       
        // Verify results (optional, for debugging)
           }
    
    return 0;
}
