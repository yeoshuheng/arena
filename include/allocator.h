//
// Created by Yeo Shu Heng on 6/12/25.
//

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "arena.h"

template<typename T>
class ArenaAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::false_type;

    ArenaV2* arena;

    template<typename U>
    friend class ArenaAllocator;

    ArenaAllocator() = delete;

    explicit ArenaAllocator(ArenaV2& arena) noexcept : arena(&arena) {};

    template<typename U>
    explicit ArenaAllocator(const ArenaAllocator<U>& other) noexcept : arena(other.arena) {};

    T* allocate(const size_type n) {
        if (n == 0) return nullptr;
        if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
        return static_cast<T*>(arena->allocate_raw(n * sizeof(T), alignof(T)));
    }

    void deallocate(const T* ptr, const size_type n) noexcept {
        (void) ptr;
        (void) n;
    }

    template<typename U>
    bool operator==(const ArenaAllocator<U>& other) const noexcept {
        return arena == other.arena;
    }

    template<typename U>
    bool operator!=(const ArenaAllocator<U>& other) const noexcept {
        return !(*this == other);
    }
};

#endif //ALLOCATOR_H
