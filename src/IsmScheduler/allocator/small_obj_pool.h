
#include <cstddef>
#include <cstdint>
#include <atomic>
#include "../schedule.h"

class small_object_pool {
protected:
    small_object_pool() = default;
};


class small_object_allocator {
public:
    template <typename Type, typename... Args>
    Type* new_object(workerInfo& ed, Args&&... args) {
        void* allocated_object = allocate(m_pool, sizeof(Type), ed);
        auto constructed_object = new(allocated_object) Type(std::forward<Args>(args)...);
        return constructed_object;
    }

    template <typename Type, typename... Args>
    Type* new_object(Args&&... args) {
        void* allocated_object = allocate(m_pool, sizeof(Type));
        auto constructed_object = new(allocated_object) Type(std::forward<Args>(args)...);
        return constructed_object;
    }

    template <typename Type>
    void delete_object(Type* object, const workerInfo& ed) {
        small_object_allocator alloc = *this;
        object->~Type();
        alloc.deallocate(object, ed);
    }

    template <typename Type>
    void delete_object(Type* object) {
        small_object_allocator alloc = *this;
        object->~Type();
        alloc.deallocate(object);
    }

    template <typename Type>
    void deallocate(Type* ptr, const workerInfo& ed) {
        assert(m_pool != nullptr);
        deallocate(*m_pool, ptr, sizeof(Type), ed);
    }

    template <typename Type>
    void deallocate(Type* ptr) {
        assert(m_pool != nullptr);
        deallocate(*m_pool, ptr, sizeof(Type));
    }

private:
    small_object_pool* m_pool{};
};


