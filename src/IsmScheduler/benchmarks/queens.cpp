#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <tbb/parallel_for.h>
#include "../parallel_for.h"
class NQueens {
private:
    int n;
    std::atomic<int> solutionCount;

    bool isSafe(const std::vector<int>& board, int row, int col) {
        for (int i = 0; i < row; i++) {
            if (board[i] == col || 
                board[i] - i == col - row || 
                board[i] + i == col + row) {
                return false;
            }
        }
        return true;
    }

    void solveNQueensUtil(std::vector<int>& board, int row) {
        if (row == n) {
            solutionCount.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        for (int col = 0; col < n; col++) {
            if (isSafe(board, row, col)) {
                board[row] = col;
                solveNQueensUtil(board, row + 1);
                board[row] = -1;  // Backtrack
            }
        }
    }

public:
    NQueens(int size) : n(size), solutionCount(0) {}

    int solve_tbb() {
        tbb::parallel_for(0, n, [&](int firstCol) {
            std::vector<int> board(n, -1);
            board[0] = firstCol;
            solveNQueensUtil(board, 1);
        });

        return solutionCount;
    }

    int solve_ism() {
        parallel_for(0, n, [&](int firstCol) {
            std::vector<int> board(n, -1);
            board[0] = firstCol;
            solveNQueensUtil(board, 1);
        });

        return solutionCount;
    }
    void reset(){
      solutionCount = 0;
    }
};

int main() {
    std::vector<int> boardSizes = {8, 10, 12,13,14,15};

    for (int n : boardSizes) {
        NQueens nQueens(n);
        auto start = std::chrono::high_resolution_clock::now();
        int solutions = nQueens.solve_tbb();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto time_tbb = duration.count();
        nQueens.reset();
        start = std::chrono::high_resolution_clock::now();
        //solutions = nQueens.solve_ism();
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto time_ism = duration.count();
        //std::cout << n << ", " << time_tbb << ", " << time_ism << std::endl;
    }

    return 0;
}
