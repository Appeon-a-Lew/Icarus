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

namespace impl {
    template<class T, size_t Size, size_t Rest>
    struct padded_extend : T {
        char pad[Size - Rest];
    };

    template<class T, size_t Size>
    struct padded_extend<T, Size, 0> : T {};
}  // namespace impl

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

template<typename T>
using cacheline_memory = type_aligned_memory<pad_to_cacheline<T>>;

}  // namespace libdb
