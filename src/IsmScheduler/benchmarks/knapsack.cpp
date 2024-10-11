
#include <cwchar>
#include <iostream>
#include <ranges>
#include <vector>
#include <tbb/tbb.h>
#include <random>
#include <chrono>
#include "../parallel_for.h"
#include "oneapi/tbb/blocked_range.h"
using namespace std;
using namespace std::chrono;
int knapsack_parallel_tbb(const vector<int>& values, const vector<int>& weights, int capacity) {
    int n = values.size();
    vector<vector<int>> dp(n + 1, vector<int>(capacity + 1, 0));

  for(int i = 1; i <= n; ++i) {
        tbb::parallel_for(tbb::blocked_range<size_t>(1,  capacity+ 1),
            [&](const tbb::blocked_range<size_t>& range) {
                for (int w = range.begin(); w < range.end(); ++w) {
                    if (weights[i - 1] <= w) {
                        dp[i][w] = max(dp[i - 1][w], dp[i - 1][w - weights[i - 1]] + values[i - 1]);
                    } else {
                        dp[i][w] = dp[i - 1][w];
                    }
                }
            }
        );
    }
    return dp[n][capacity];
}
int knapsack_parallel(const vector<int>& values, const vector<int>& weights, int capacity) {
    int n = values.size();
    vector<vector<int>> dp(n + 1, vector<int>(capacity + 1, 0));

    for(int i = 1; i <= n; ++i) {
        parallel_for_morsel(1,  capacity+ 1,
            [&](const tbb::blocked_range<size_t>& range) {
                for (int w = range.begin(); w < range.end(); ++w) {
                    if (weights[i - 1] <= w) {
                        dp[i][w] = max(dp[i - 1][w], dp[i - 1][w - weights[i - 1]] + values[i - 1]);
                    } else {
                        dp[i][w] = dp[i - 1][w];
                    }
                }
            }
        ,0,0);
    }

    return dp[n][capacity];
}

int main() {
  //cout << "Knappsack size, tbb time, hybrid time" << endl;
  for(size_t n = 1e5; n < 2e5; n += 10000){
    int capacity = n/10;  // Knapsack capacity
    cout << n << ", ";
    cout.flush();
    // Random number generator
    mt19937 gen(42);
    uniform_int_distribution<> dis(1, 100);

    // Generate random values and weights
    vector<int> values(n);
    vector<int> weights(n);
    for (int i = 0; i < n; ++i) {
      values[i] = dis(gen);
      weights[i] = dis(gen);
    }
    // Time measurement
    auto start_time = high_resolution_clock::now();

    // Solve knapsack problem using parallel execution
    int max_value = knapsack_parallel_tbb(values,weights, capacity);

    auto end_time = high_resolution_clock::now();
    duration<double> diff = end_time - start_time;

    auto time_tbb = (diff).count();
    //cout << time_tbb << ", ";
    //cout.flush();
    //cout << "Maximum value in Knapsack = " << max_value << endl;
    mt19937 gen2(42);
    uniform_int_distribution<> dis2(1, 100);

    for (int i = 0; i < n; ++i) {
      values[i] = dis2(gen2);
      weights[i] = dis2(gen2);
    }
    // Time measurement
    start_time = high_resolution_clock::now();

    //olve knapsack problem using parallel execution
    //auto max_value2 = knapsack_parallel(values, weights, capacity);

    end_time = high_resolution_clock::now();
    diff = end_time - start_time;
    auto time_ism = diff.count();

    //cout << time_ism << endl; 

    //cout << "Maximum value in Knapsack = " << max_value << endl;
  }
  return 0;
}
