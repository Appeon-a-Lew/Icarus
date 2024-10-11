#include <iostream>
#include <oneapi/tbb/parallel_invoke.h>
#include <vector>
#include <cmath>
#include <chrono>
#include <tbb/parallel_for.h>
#include <tbb/parallel_invoke.h>
#include "../parallel_for.h"
using Matrix = std::vector<std::vector<int>>;

// Function to add two matrices
Matrix add_tbb(const Matrix& A, const Matrix& B) {
    int n = A.size();
    Matrix C(n, std::vector<int>(n, 0));
    tbb::parallel_for(0, n, [&](int i) {
        for (int j = 0; j < n; j++) {
            C[i][j] = A[i][j] + B[i][j];
        }
    });
    return C;
}

// Function to subtract two matrices
Matrix sub_tbb(const Matrix& A, const Matrix& B) {
    int n = A.size();
    Matrix C(n, std::vector<int>(n, 0));
    tbb::parallel_for(0, n, [&](int i) {
        for (int j = 0; j < n; j++) {
            C[i][j] = A[i][j] - B[i][j];
        }
    });
    return C;
}

// Function to split a matrix into four submatrices
void split_tbb(const Matrix& A, Matrix& A11, Matrix& A12, Matrix& A21, Matrix& A22) {
    int n = A.size() / 2;
    tbb::parallel_for(0, n, [&](int i) {
        for (int j = 0; j < n; j++) {
            A11[i][j] = A[i][j];
            A12[i][j] = A[i][j + n];
            A21[i][j] = A[i + n][j];
            A22[i][j] = A[i + n][j + n];
        }
    });
}

// Function to join four submatrices into a single matrix
void join_tbb(Matrix& A, const Matrix& A11, const Matrix& A12, const Matrix& A21, const Matrix& A22) {
    int n = A11.size();
    tbb::parallel_for(0, n, [&](int i) {
        for (int j = 0; j < n; j++) {
            A[i][j] = A11[i][j];
            A[i][j + n] = A12[i][j];
            A[i + n][j] = A21[i][j];
            A[i + n][j + n] = A22[i][j];
        }
    });
}

// Strassen's algorithm implementation
Matrix strassen_tbb(const Matrix& A, const Matrix& B) {
    int n = A.size();
    if (n <= 64) {  // Base case: use standard multiplication for small matrices
        Matrix C(n, std::vector<int>(n, 0));
        tbb::parallel_for(0, n, [&](int i) {
            for (int j = 0; j < n; j++) {
                for (int k = 0; k < n; k++) {
                    C[i][j] += A[i][k] * B[k][j];
                }
            }
        });
        return C;
    }

    int new_size = n / 2;
    Matrix A11(new_size, std::vector<int>(new_size)),
           A12(new_size, std::vector<int>(new_size)),
           A21(new_size, std::vector<int>(new_size)),
           A22(new_size, std::vector<int>(new_size));
    Matrix B11(new_size, std::vector<int>(new_size)),
           B12(new_size, std::vector<int>(new_size)),
           B21(new_size, std::vector<int>(new_size)),
           B22(new_size, std::vector<int>(new_size));

    split_tbb(A, A11, A12, A21, A22);
    split_tbb(B, B11, B12, B21, B22);

    Matrix P1, P2, P3, P4, P5, P6, P7;
    auto split1  = [&](){
    tbb::parallel_invoke(
        [&]{ P6 = strassen_tbb(sub_tbb(A21, A11), add_tbb(B11, B12)); },
        [&]{ P7 = strassen_tbb(sub_tbb(A12, A22), add_tbb(B21, B22)); });
    };
    auto split2 = [&](){
    tbb::parallel_invoke(
        [&]{ P4 = strassen_tbb(A22, sub_tbb(B21, B11)); },
        [&]{ P5 = strassen_tbb(add_tbb(A11, A12), B22); });
    };
    auto split3 = [&](){
      tbb::parallel_invoke(
        [&]{ P2 = strassen_tbb(add_tbb(A21, A22), B11); },
        [&]{ P3 = strassen_tbb(A11, sub_tbb(B12, B22)); }
      );
    };
    auto split4 = [&](){
      tbb::parallel_invoke(
        [&]{ P1 = strassen_tbb(add_tbb(A11, A22), add_tbb(B11, B22)); },
        split3);
    };
    auto split5 = [&](){
      tbb::parallel_invoke(split1,split2);
    };
    tbb::parallel_invoke(split4,split5);
    
    Matrix C11 = add_tbb(sub_tbb(add_tbb(P1, P4), P5), P7);
    Matrix C12 = add_tbb(P3, P5);
    Matrix C21 = add_tbb(P2, P4);
    Matrix C22 = add_tbb(sub_tbb(add_tbb(P1, P3), P2), P6);

    Matrix C(n, std::vector<int>(n));
    join_tbb(C, C11, C12, C21, C22);

    return C;
}

// Function to generate a random matrix
Matrix generate_random_matrix(int n) {
    Matrix A(n, std::vector<int>(n));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A[i][j] = rand() % 10;
        }
    }
    return A;
}
// Function to add two matrices
Matrix add_matrices(const Matrix& A, const Matrix& B) {
    int n = A.size();
    Matrix C(n, std::vector<int>(n, 0));
    parallel_for(0, n, [&](int i) {
        for (int j = 0; j < n; j++) {
            C[i][j] = A[i][j] + B[i][j];
        }
    });
    return C;
}

// Function to subtract two matrices
Matrix subtract_matrices(const Matrix& A, const Matrix& B) {
    int n = A.size();
    Matrix C(n, std::vector<int>(n, 0));
    parallel_for(0, n, [&](int i) {
        for (int j = 0; j < n; j++) {
            C[i][j] = A[i][j] - B[i][j];
        }
    });
    return C;
}

// Function to split a matrix into four submatrices
void split_matrix(const Matrix& A, Matrix& A11, Matrix& A12, Matrix& A21, Matrix& A22) {
    int n = A.size() / 2;
    parallel_for(0, n, [&](int i) {
        for (int j = 0; j < n; j++) {
            A11[i][j] = A[i][j];
            A12[i][j] = A[i][j + n];
            A21[i][j] = A[i + n][j];
            A22[i][j] = A[i + n][j + n];
        }
    });
}

// Function to join four submatrices into a single matrix
void join_matrices(Matrix& A, const Matrix& A11, const Matrix& A12, const Matrix& A21, const Matrix& A22) {
    int n = A11.size();
    parallel_for(0, n, [&](int i) {
        for (int j = 0; j < n; j++) {
            A[i][j] = A11[i][j];
            A[i][j + n] = A12[i][j];
            A[i + n][j] = A21[i][j];
            A[i + n][j + n] = A22[i][j];
        }
    });
}

// Strassen's algorithm implementation
Matrix strassen(const Matrix& A, const Matrix& B) {
    int n = A.size();
    if (n <= 256) {  // Base case: use standard multiplication for small matrices
        Matrix C(n, std::vector<int>(n, 0));
        parallel_for(0, n, [&](int i) {
            for (int j = 0; j < n; j++) {
                for (int k = 0; k < n; k++) {
                    C[i][j] += A[i][k] * B[k][j];
                }
            }
        });
        return C;
    }

    int new_size = n / 2;
    Matrix A11(new_size, std::vector<int>(new_size)),
           A12(new_size, std::vector<int>(new_size)),
           A21(new_size, std::vector<int>(new_size)),
           A22(new_size, std::vector<int>(new_size));
    Matrix B11(new_size, std::vector<int>(new_size)),
           B12(new_size, std::vector<int>(new_size)),
           B21(new_size, std::vector<int>(new_size)),
           B22(new_size, std::vector<int>(new_size));

    split_matrix(A, A11, A12, A21, A22);
    split_matrix(B, B11, B12, B21, B22);

    Matrix P1, P2, P3, P4, P5, P6, P7;
    auto split1  = [&](){
      parallel_do(
        [&]{ P6 = strassen(subtract_matrices(A21, A11), add_matrices(B11, B12)); },
        [&]{ P7 = strassen(subtract_matrices(A12, A22), add_matrices(B21, B22)); });
    };
    auto split2 = [&](){
      parallel_do(
        [&]{ P4 = strassen(A22, subtract_matrices(B21, B11)); },
        [&]{ P5 = strassen(add_matrices(A11, A12), B22); });
    };
    auto split3 = [&](){
      parallel_do(
        [&]{ P2 = strassen(add_matrices(A21, A22), B11); },
        [&]{ P3 = strassen(A11, subtract_matrices(B12, B22)); }
      );
    };
    auto split4 = [&](){
      parallel_do(
        [&]{ P1 = strassen(add_matrices(A11, A22), add_matrices(B11, B22)); },
        split3);
    };
    auto split5 = [&](){
      parallel_do(split1,split2);
    };
    parallel_do(split4,split5);
    
    Matrix C11 = add_matrices(subtract_matrices(add_matrices(P1, P4), P5), P7);
    Matrix C12 = add_matrices(P3, P5);
    Matrix C21 = add_matrices(P2, P4);
    Matrix C22 = add_matrices(subtract_matrices(add_matrices(P1, P3), P2), P6);

    Matrix C(n, std::vector<int>(n));
    join_matrices(C, C11, C12, C21, C22);

    return C;
}
int main() {
    std::vector<int> sizes = {1024, 2048,4096,8192};
    //std::cout << "Matrix size, tbb time, hybrid time\n";
    for (int n : sizes) {
        Matrix A = generate_random_matrix(n);
        Matrix B = generate_random_matrix(n);
        //std::cout << n << ", "; 
        //std::cout.flush();
        auto start = std::chrono::high_resolution_clock::now();
        //Matrix C = strassen(A, B);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto time_tbb = duration.count();
        std::cout << time_tbb; 
        std::cout.flush();
        start = std::chrono::high_resolution_clock::now();
        Matrix C = strassen_tbb(A, B);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto time_ism = duration.count();

        //std::cout << time_ism <<  std::endl;

    }

    return 0;
}
