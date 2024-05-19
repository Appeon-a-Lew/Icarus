// -----------------------------------------------------------------------------
// 2023 Maximilian Kuschewski
// -----------------------------------------------------------------------------
#pragma once

#include <stdexcept>
#include <cassert>

#ifdef NDEBUG
#define ASSERT_HELP(...)
#define DEBUGGING(x...)
#else
#define ASSERT_HELP(x...) x
#define DEBUGGING(x...) x
#endif


#ifdef NDEBUG
#define TMP_DEBUGGING(x...)
#else
//#define TMP_DEBUGGING(x...) x
#define TMP_DEBUGGING(x...)
#endif

// ------------------------------------------------------------------------
#ifdef DEBUG_PRINT
#define DPR(expr) (std::cerr << expr << std::endl)
#define DPR_HELP(x...) x
#else
#define DPR(expr)
#define DPR_HELP(x...)
#endif

#ifdef NDEBUG
#define ENSURE(expr) if (!(expr)) throw std::logic_error("Error: " #expr " was not fulfilled (" __FILE__ ":" + std::to_string(__LINE__) + "); ")
#else
#define ENSURE(expr) assert(expr)
#endif

#define ensure(expr) ENSURE(expr)

#define ensure_eq(a, b) if (!((a) == (b))) throw std::logic_error("Error: " + std::to_string(a) + " was not equal to " + std::to_string(b) + " (" __FILE__ ":" + std::to_string(__LINE__) + "); ")

#ifdef MEASURE_PERFORMANCE
#define MEASUREMENT(x...) x
#else
#define MEASUREMENT(...)
#endif

#ifdef DO_LOG
#define LOG(x...) x
#else
#define LOG(x...)
#endif

#if defined(__clang__)
#define DIRTY_MEMACCESS(x...) x
#else
#define DIRTY_MEMACCESS(x...) \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wclass-memaccess\"") \
    x \
    _Pragma("GCC diagnostic pop")
#endif


#ifndef NDEBUG
#include <unordered_set>
#include <mutex>
namespace libdb {
template<typename T>
struct DebugSet {
    std::mutex m;
    std::unordered_set<T> set;

    bool contains(const T& t) {
        std::unique_lock<std::mutex> lck(m);
        return set.contains(t);
    }

    void insert(const T& t) {
        std::unique_lock<std::mutex> lck(m);
        set.insert(t);
    }
};  // struct DebugSet
}  // namespace libdb
#endif
