
// -----------------------------------------------------------------------------
// 2023 Maximilian Kuschewski
// -----------------------------------------------------------------------------
#pragma once
#include <cstddef>
#include <cstdint>
#include <numeric>

namespace libdb {

inline constexpr size_t CACHELINE_BYTES = 64;
inline constexpr size_t NO_FALSE_SHARING_BYTES = CACHELINE_BYTES*2;
//Here we have defined the cacheline size 64 
//We are also using NO_FALSE_SHARING_BYTES so cores dont false share 

namespace impl {
    template<class T, size_t Size, size_t Rest>
    struct padded_extend : T {
        char pad[Size - Rest];
    };
//This point we are calling the constructor of T 
// and padding with char arrays 

    template<class T, size_t Size>
    struct padded_extend<T, Size, 0> : T {};
}  // namespace impl

/**
 * pad_to: uses the padded_extend to pad to a specific size
 * pad_to_cacheline: pads to 128B. Bc Some processors use 2 cachelines as fetchj
 * * */
template<class T, size_t Size>
struct pad_to : impl::padded_extend<T, Size, sizeof(T) % Size> {};

template<typename T>
using pad_to_cacheline = pad_to<T, NO_FALSE_SHARING_BYTES>;

template<typename T, std::size_t N>
struct type_aligned_array {
    alignas(alignof(T)) std::uint8_t memory[N * sizeof(T)];  // NOLINT
    T* begin() const { return std::bit_cast<T*>(&memory); }
    T* end() const { return begin() + N; }
};

template<typename T>
struct type_aligned_memory {
    alignas(alignof(T)) std::uint8_t memory[sizeof(T)];  // NOLINT
    T* data() { return std::bit_cast<T*>(&memory); }
    const T* data() const { return std::bit_cast<const T*>(&memory); }
    operator const T&() const { return *data(); }
    operator T&() { return *data(); }
};
/*
 *cacheline memory: type aligned memory called with type T padded to 128B 
 *this way it is padded and aligned.
 * */
template<typename T>
using cacheline_memory = type_aligned_memory<pad_to_cacheline<T>>;

}  // namespace libdb
