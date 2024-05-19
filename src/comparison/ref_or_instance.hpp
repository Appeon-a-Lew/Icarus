// -----------------------------------------------------------------------------
// 2023 Maximilian Kuschewski
// -----------------------------------------------------------------------------
#pragma once
#include <variant>
#include <utility>

namespace libdb {

    template<typename T>
    struct RefOrInstance {

        using item_t = T;
        std::variant<item_t, item_t*> data;

        RefOrInstance() = default;

        explicit RefOrInstance(item_t* ptr) : data(ptr) {}
        explicit RefOrInstance(const item_t& ref) : data(ref) {}
        explicit RefOrInstance(item_t&& ref) : data(std::move(ref)) {}

        RefOrInstance& operator=(const RefOrInstance<item_t>& other) {
            //data = other.data;
            //return *this;
            return other.has_instance()
                ? (*this = *other.get_ptr())
                : (*this = const_cast<item_t*>(other.get_ptr()));
        }

        RefOrInstance& operator=(item_t* ptr) {
            data.template emplace<item_t*>(ptr);
            return *this;
        }

        RefOrInstance& operator=(const item_t& ref) {
            data.template emplace<item_t>(ref);
            return *this;
        }

        RefOrInstance& operator=(item_t&& ref) {
            data.template emplace<item_t>(std::move(ref));
            return *this;
        }

        [[nodiscard]] bool has_instance() const { return std::holds_alternative<item_t>(data); }
        item_t* get_ptr() { return std::holds_alternative<item_t>(data) ? &std::get<0>(data) : std::get<1>(data); }
        const item_t* get_ptr() const { return std::holds_alternative<item_t>(data) ? &std::get<0>(data) : std::get<1>(data); }

        item_t* operator->() { return get_ptr(); }
        item_t& operator*() { return *get_ptr(); }
        const item_t& operator*() const { return *get_ptr(); }
    };

}  // namespace libdb
