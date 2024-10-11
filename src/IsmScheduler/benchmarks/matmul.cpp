
#include <cstddef>
#include <iostream>
#include <vector>
#include <tbb/tbb.h>
#include <chrono>
#include "../parallel_for.h"
#include "oneapi/tbb/concurrent_hash_map.h"

using namespace std;

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

// Parallel matrix multiplication using TBB
void parallel_matrix_multiply(const vector<vector<int>>& A, const vector<vector<int>>& B, vector<vector<int>>& C) {
    size_t N = A.size();

    // Use parallel_for to parallelize the outer loop
    tbb::parallel_for(tbb::blocked_range<size_t>(0, N), [&](const tbb::blocked_range<size_t>& r) {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            for (size_t j = 0; j < N; ++j) {
                C[i][j] = 0;
                for (size_t k = 0; k < N; ++k) {
                    C[i][j] += A[i][k] * B[k][j];
                }
            }
        }
    });
}
void parallel_matrix_multiply_ism(const vector<vector<int>>& A, const vector<vector<int>>& B, vector<vector<int>>& C) {
    size_t N = A.size();

    // Use parallel_for to parallelize the outer loop
    parallel_for_morsel(0, N, [&](const tbb::blocked_range<size_t>& r) {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            for (size_t j = 0; j < N; ++j) {
                C[i][j] = 0;
                for (size_t k = 0; k < N; ++k) {
                    C[i][j] += A[i][k] * B[k][j];
                }
            }
        }
    },0,0);
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
    for(size_t N = 1000; N <=  2200;N+=100 ){
    vector<vector<int>> A(N, vector<int>(N));
    vector<vector<int>> B(N, vector<int>(N));
    vector<vector<int>> C1(N, vector<int>(N));
    vector<vector<int>> C2(N, vector<int>(N));

    vector<vector<int>> C3(N, vector<int>(N));
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
    // Perform parallel matrix multiplication
    //parallel_matrix_multiply_ism(a_clone, b_clone, C1);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    //std::cout << "The parallel time: " << diff.count() << std::endl;  
    auto time_ism = diff.count();
    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < N; j++) {
            A[i][j] = i + j;
            B[i][j] = i * j;
        }
    }

    a_clone = A;
    b_clone = B;
    
    start = std::chrono::high_resolution_clock::now(); 
    // Perform parallel matrix multiplication
    parallel_matrix_multiply(a_clone,b_clone,C2);
    end = std::chrono::high_resolution_clock::now();
    diff = end - start;
    //std::cout << "The TBB time: " << diff.count() << std::endl;  
    auto time_tbb = diff.count();
    //std::cout << N << ", " << time_tbb << ", " << time_ism << std::endl;
    }
    return 0;
}
