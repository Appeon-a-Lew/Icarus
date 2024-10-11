# Compiler settings
CXX = clang++
CXXFLAGS = -std=c++2a -O2
LDFLAGS = -ltbb

# List of benchmarks
BENCHMARKS = cilksort fib knapsack latency matmul pi_mc queens strassen

# Directory settings
BENCHMARKS_DIR = benchmarks
BUILD_DIR = build

# Generate lists of source and object files
SOURCES = $(addprefix $(BENCHMARKS_DIR)/, $(addsuffix .cpp, $(BENCHMARKS)))
OBJECTS = $(addprefix $(BUILD_DIR)/, $(addsuffix .o, $(BENCHMARKS)))
EXECUTABLES = $(addprefix $(BUILD_DIR)/, $(BENCHMARKS))

# Default target
all: $(EXECUTABLES)

# Rule to create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Rule to compile source files into object files
$(BUILD_DIR)/%.o: $(BENCHMARKS_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to link object files into executables
$(BUILD_DIR)/%: $(BUILD_DIR)/%.o
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

# Clean target
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all clean

# Print variables for debugging
print-%:
	@echo $* = $($*)
