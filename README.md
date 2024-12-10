# Low-Latency Scheduling on Manycore CPUs

This project introduces a hybrid scheduling algorithm designed for low-latency task management on many-core CPUs. It combines the efficiency of work-stealing with a mailboxing technique to address challenges posed by increasing core counts and NUMA memory hierarchies. The implementation achieves significant reductions in scheduling latency while optimizing cache performance.

## Features

- **Hybrid Scheduling Algorithm**: Combines work-stealing and mailboxing for improved task management.
- **Cache-Aware Optimizations**: Enhances task locality using cache-aware partitioning and the morsels approach.
- **Advanced Data Structures**: Implements state-of-the-art algorithms like Arora-Blumofe-Plaxton, Chase-Lev deques, and split deques for efficient task handling.
- **Benchmark Evaluation**: Outperforms classical work-stealing and Intel TBB on various benchmarks, including recursive algorithms and parallel for-loop tasks.

## Key Contributions

- Significant reduction in scheduling latency for task startup and completion phases.
- Optimized concurrent deque implementations for reduced contention and synchronization overhead.
- Enhanced cache performance validated through memory fence and compare-and-swap operation analysis.

## Use Cases

- High-performance computing systems requiring low-latency scheduling.
- Applications running on many-core architectures with NUMA hierarchies.
- Developers looking to optimize task scheduling for modern parallel workloads.

## Getting Started
Use the parallel_for.h or scheduler.h in your code. 
