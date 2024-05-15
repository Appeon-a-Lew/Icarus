#include <iostream>
#include <vector>
#include <sycl/sycl.hpp>

constexpr size_t N = 1024*1024; // size of the array

// Define a kernel that will be executed in parallel
void kernel_func(sycl::queue &q, sycl::buffer<int, 1> &buf) {
  sycl::parallel_for(sycl::range<1>{N}, [=](sycl::id<1> idx) {
    buf[idx] = idx;
  });
}

int main() {
  sycl::queue q;
  sycl::buffer<int, 1> buf(sycl::range<1>{N});

  // Warm up the GPU
  for (int i = 0; i < 10; i++) {
    kernel_func(q, buf);
  }

  // Measure the latency
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 100; i++) {
    kernel_func(q, buf);
  }
  auto end = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  std::cout << "Average latency: " << duration/100 << " us" << std::endl;

  return 0;
}