#!/bin/bash

# Function to compile and run the OneAPI version
run_oneapi() {
    echo "Compiling and running OneAPI version..."
    oneapi_source="oneapi.cpp"
    oneapi_executable="oneapi_program"
     icpx -fsycl -o $oneapi_executable $oneapi_source
    ./$oneapi_executable
}

# Function to compile and run the OpenMP version
run_openmp() {
    echo "Compiling and running OpenMP version..."
    openmp_source="openmp.cpp"
    openmp_executable="openmp_program"
    g++ -fopenmp -o $openmp_executable $openmp_source
    ./$openmp_executable
}

# Function to compile and run the custom scheduler version
run_scheduler() {
    echo "Compiling and running custom scheduler version..."
    scheduler_source="maxi.cpp"
    scheduler_executable="scheduler_program"
    g++ $scheduler_source -std=c++20 -ltbb -lpthread -latomic -o $scheduler_executable
    ./$scheduler_executable
}

# Main function to run all versions
run_all() {
    run_oneapi
    run_openmp
    run_scheduler
}

# Check for arguments and run accordingly
if [ "$1" == "oneapi" ]; then
    run_oneapi
elif [ "$1" == "openmp" ]; then
    run_openmp
elif [ "$1" == "scheduler" ]; then
    run_scheduler
else
    run_all
fi
