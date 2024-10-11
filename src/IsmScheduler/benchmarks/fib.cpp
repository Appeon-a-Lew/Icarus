#include <oneapi/tbb/blocked_range.h>
#define Arr_size  1e8

#include <vector>
#include "../parallel_for.h"
#include <tbb/blocked_range.h>
#include <chrono> 
#include <oneapi/tbb.h> 
unsigned long long fibonacci_seq(size_t n){

  if(n < 2 ) return n;

  return fibonacci_seq(n-1) + fibonacci_seq(n-2);
}

unsigned long long fibonacci(size_t n){
  if (n <= 20)  // Threshold to avoid too fine-grained tasks
        return fibonacci_seq(n);
  unsigned long long x=0, y=0; 
  parallel_do(
    [&](){x = fibonacci(n-1);},
    [&](){y = fibonacci(n-2);},
    false
  );
  return x + y;
} 

unsigned long long fibonacci_tbb(unsigned long long n) {
   if (n <= 20)  // Threshold to avoid too fine-grained tasks
        return fibonacci_seq(n);
    unsigned long long x = 0, y = 0;

    // Use tbb::parallel_invoke to parallelize the recursive calls
    tbb::parallel_invoke(
        [&]() { x = fibonacci_tbb(n - 1); },
        [&]() { y = fibonacci_tbb(n - 2); }
    );
    return x + y;
}

#include <future>

// Function to calculate Fibonacci number using std::async for parallelism
unsigned long long fibonacci_future(unsigned long long n) {
   if (n <= 10)  // Threshold to avoid too fine-grained tasks
        return fibonacci_seq(n);

    // Launch a task to calculate fibonacci(n - 1) in parallel
    auto future_x = std::async(std::launch::async, fibonacci_future, n - 1);

    // Calculate fibonacci(n - 2) in the current thread
    unsigned long long y = fibonacci_future(n - 2);

    // Get the result of the parallel computation and combine it with y
    return future_x.get() + y;
}

#define FIB_NUM 45
int main() {

 std::vector<int> data(Arr_size,-1);
  
  

  auto start =  std::chrono::high_resolution_clock::now();
  auto sonuc = 1;//fibonacci_seq(FIB_NUM);
  auto end = std::chrono::high_resolution_clock::now();
  
  std::chrono::duration<double> diff = end - start;
  //std::cout << "fibonacci_seq was " << diff.count() << std::endl;


  //std::cout << sonuc << std::endl;
  for(size_t i = 25; i < FIB_NUM; ++i){
  //std::cout << "Fib: " << i << ", ";
  start = std::chrono::high_resolution_clock::now();
  auto res = fibonacci_tbb(i);
  end = std::chrono::high_resolution_clock::now();
  diff = end - start;
//  std::cout << "The final result: " << res << "\n";
  auto tbb_time = diff.count();
//  std::cout << "Time TBB: " <<tbb_time<< std::endl;
  //start = std::chrono::high_resolution_clock::now();
  //auto res2 = fibonacci(i);
  //end = std::chrono::high_resolution_clock::now();
  //diff = end - start;
  //auto ism_time = diff.count();
//  std::cout << "The final result: " << res << "\n";
  //std::cout << tbb_time<<", " <<ism_time<< std::endl;
//  if(tbb_time < ism_time)
//    std::cout << "TBB FASTER\n\n";
//  else 
//    std::cout << "ISM FASTER\n\n";
  }
    

  return 0;
  

}
