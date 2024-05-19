// -----------------------------------------------------------------------------
// 2023 Maximilian Kuschewski
// -----------------------------------------------------------------------------
#pragma once
#include <type_traits>
#include <utility>
#include <cstddef>
// ------------------------------------------------------------------------------
template <typename T>
concept is_trivially_movable = (std::is_nothrow_copy_constructible<T>::value &&
                                std::is_nothrow_move_constructible<T>::value);
// ------------------------------------------------------------------------------
template <typename F>
requires is_trivially_movable<F>
class deferred {
    F my_func;
    bool active{};

public:
    explicit deferred(F f) noexcept
        : my_func(f) {}

    deferred(deferred&& o) noexcept
        : my_func(std::move(o.my_func))
        , active(o.is_active) {
        o.active = false;
    }

    ~deferred() {
        if (active) { my_func(); }
    }
    void cancel() { active = false; }
};  // class deferred
template <typename F>
deferred<F> defer(F f) {
    return deferred<F>(f);
}
// ------------------------------------------------------------------------------
template <typename T>
struct value_wrap {
    using self_t = value_wrap<T>;
    T value;
    auto operator<=>(const self_t& o) const = default;
    T& operator*() { return value; }
    self_t& operator++() {
        ++value;
        return *this;
    }
    self_t operator++(int) {
        self_t o = *this;
        ++value;
        return o;
    }
    operator T() const { return value; }
};  // struct value_wrap
// ------------------------------------------------------------------------------
template<class T, size_t Size, size_t Rest>
struct padded_base : T {
    char pad[Size - Rest];
};
template<class T, size_t Size> struct padded_base<T, Size, 0> : T {};
template<class T, size_t Size = 64>
struct padded : padded_base<T, Size, sizeof(T) % Size> {};
// ------------------------------------------------------------------------------
// overload lambdas
// from https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts>
struct make_overload : Ts... { using Ts::operator()...; };
template<class... Ts>
make_overload(Ts...) -> make_overload<Ts...>;
// ------------------------------------------------------------------------------
template<typename F, typename... Args>
concept is_callable_as = requires(F&& f, Args&&... args) { f(std::forward<Args>(args)...); };
// ------------------------------------------------------------------------------
template<typename F, typename... Args> requires is_callable_as<F, unsigned, Args...>
auto invoke_indexed_if_possible(F&& f, unsigned index, Args&&... args) {
    return f(index, std::forward<Args>(args)...);
}
template<typename F, typename... Args> requires is_callable_as<F, Args...>
auto invoke_indexed_if_possible(F&& f, unsigned index, Args&&... args) {
    return f(std::forward<Args>(args)...);
}
// ------------------------------------------------------------------------------
template<typename T>
class constantly {
    const T value;
public:
    constexpr explicit constantly(const T value) noexcept : value(value) {}
    constexpr T operator()() const noexcept { return value; }
};
template<typename T, T Value>
struct constant_fn {
    constexpr T operator()() const noexcept { return Value; }
};
// ------------------------------------------------------------------------------
