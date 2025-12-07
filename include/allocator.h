//
// Created by Yeo Shu Heng on 6/12/25.
//

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "arena.h"

/**
 * @class ArenaAllocator
 * @brief STL-compatible allocator using an ArenaV2 for fast monotonic allocation.
 *
 * ArenaAllocator provides an adapter that allows STL containers to allocate memory directly from an `ArenaV2` instance.
 *
 * @tparam T Value type the allocator handles.
 */
template<typename T>
class ArenaAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    /**
     * @brief Propagation policy for container move assignment.
     */
    using propagate_on_container_move_assignment = std::true_type;

    /**
     * @brief Allocators are considered equal if the house references to the same ArenaV2.
     */
    using is_always_equal = std::false_type;

    ArenaV2* arena;

    template<typename U>
    friend class ArenaAllocator;

    ArenaAllocator() = delete;

    /**
     * @brief Constructs an allocator bound to a specific ArenaV2.
     *
     * @param arena The arena from which memory will be allocated.
     */
    explicit ArenaAllocator(ArenaV2& arena) noexcept : arena(&arena) {};

    /**
     * @brief Copy constructor.
     *
     * Allows `ArenaAllocator<T>` to be converted to `ArenaAllocator<U>`.
     * Mainly to allow rebinding operations for internal types within STL containers, such as:
     *      `std::pair<const K, V>` & `std::pair<K, V>`
     * This allows the same `ArenaV2` to be used across different internal types.
     * Allowing our implementation to adhere to memory ownership consistency.
     *
     * @tparam U Other value type.
     * @param other Allocator to convert from.
     */
    template<typename U>
    explicit ArenaAllocator(const ArenaAllocator<U>& other) noexcept : arena(other.arena) {};

    /**
     * @brief Allocates memory for `n` objects of type `T` from the arena.
     *
     * @param n Number of objects to allocate.
     * @return Pointer to uninitialized memory sufficient for `n` objects.
     * @throws std::bad_array_new_length if `n * sizeof(T)` overflows.
     *
     * @note Alignment is strictly enforced via `alignof(T)`.
     * @note Returned memory is automatically freed only when the arena is cleared.
     */
    T* allocate(const size_type n) {
        if (n == 0) return nullptr;
        if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
        return static_cast<T*>(arena->allocate_raw(n * sizeof(T), alignof(T)));
    }

    /**
     * @brief No-op.
     *
     * STL containers may call `deallocate(ptr, n)` during resizing.
     * `ArenaV2` is monotonic and assumes all allocated objects have the same lifetime.
     * Hence, individual frees should not return memory to the arena.
     *
     * @param ptr Pointer to memory previously returned by allocate().
     * @param n   Number of T elements originally allocated.
     */
    void deallocate(const T* ptr, const size_type n) noexcept {
        (void) ptr;
        (void) n;
    }

    /**
     * @brief Allocators are equal if they use the same arena.
     */
    template<typename U>
    bool operator==(const ArenaAllocator<U>& other) const noexcept {
        return arena == other.arena;
    }

    /**
     * @brief Check if non-equal.
     */
    template<typename U>
    bool operator!=(const ArenaAllocator<U>& other) const noexcept {
        return !(*this == other);
    }
};

#endif //ALLOCATOR_H
