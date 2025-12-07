//
// Created by Yeo Shu Heng on 2/12/25.
//

#ifndef ARENAV2_H
#define ARENAV2_H

#include <vector>
#include <functional>
#include <memory>

inline constexpr size_t DEFAULT_BLOCK_SIZE = 1024;
inline constexpr size_t DESTRUCTOR_CHUNK_SIZE = 32;

/**
 * @class ArenaV2
 * @brief A monotonic, bump-pointer arena allocator supporting automatic growth.
 *
 * ArenaV2 manages memory using a sequence of contiguous memory blocks (``MemBlock``).
 * Allocations occur via simple bump-pointer arithmetic, making allocation extremely fast (O(1)) and cache-friendly.
 * The arena grows automatically by allocating new blocks when required.
 *
 * Unlike fixed-size arenas, ArenaV2 does *not* fail when the current block is exhausted.
 * Instead, it allocates a new block of at least `block_size` bytes.
 *
 * Destruction of allocated objects is managed through an internal destructor list.
 * Objects with non-trivial destructors are registered in small batches (`DestructorChunk`)
 * and destroyed during `clear()` or at arena destruction to improve cache locality.
 *
 * @warning ArenaV2 is **not thread-safe**.
 * @warning Deallocation of individual objects is not supported.
 * @note All objects become invalid after `clear()` or move-assignment.
 */
class ArenaV2 {
public:
    /**
     * @brief Constructs a new arena with DEFAULT_BLOCK_SIZE.
     *
     * Constructs the first memory block immediately based on the default block size.
     */
    explicit ArenaV2() : mem_block_latest_idx(0), block_size(DEFAULT_BLOCK_SIZE), arena_size(0) {
        add_mem_block(DEFAULT_BLOCK_SIZE);
    }

    /**
    * @brief Constructs an arena with a specified initial block size.
    *
    * Constructs the first block immediately based on the initial block size.
    *
    * @param size Minimum size (in bytes) of each newly allocated block.
    */
    explicit ArenaV2(const size_t size) : mem_block_latest_idx(0), block_size(size), arena_size(0) {
        add_mem_block(size);
    };


    /**
     * @brief Destroys the arena and all allocated objects.
     */
    ~ArenaV2() {
        clear();
    }

    ArenaV2(const ArenaV2&) = delete;
    ArenaV2& operator=(const ArenaV2&) = delete;

    /**
    * @brief Moves ownership of memory blocks and destructor lists.
    */
    ArenaV2(ArenaV2&& other) noexcept :
        destructor_block_tail(other.destructor_block_tail),
        destructor_block_latest(other.destructor_block_latest),
        mem_blocks(std::move(other.mem_blocks)),
        mem_block_latest_idx(other.mem_block_latest_idx),
        block_size(other.block_size),
        arena_size(other.arena_size) {
        other.destructor_block_latest = nullptr;
        other.destructor_block_tail = nullptr;
        other.mem_block_latest_idx = 0;
        other.arena_size = 0;
    };

    /**
     * @brief Move assignment.
     *
     * Clears current allocations first before conducting move assignment.
     */
    ArenaV2& operator=(ArenaV2&& other) noexcept {
        if (&other != this) {
            clear();
            this->block_size = other.block_size;
            this->arena_size = other.arena_size;
            this->destructor_block_latest = other.destructor_block_latest;
            this->destructor_block_tail = other.destructor_block_tail;
            this->mem_blocks = std::move(other.mem_blocks);
            this->mem_block_latest_idx = other.mem_block_latest_idx;

            other.mem_block_latest_idx = 0;
            other.destructor_block_latest = nullptr;
            other.destructor_block_tail = nullptr;
            other.arena_size = 0;
        }
        return *this;
    }

    /**
    * @brief Constructs an object of type `T` within the arena.
    *
    * Allocates storage for object of type `T` and registers object's destructor.
    *
    * @tparam T Type of object to construct.
    * @tparam Args Constructor parameter types.
    * @param args Arguments forwarded to `T`'s constructor.
    *
    * @return Pointer to the constructed object.
    */
    template<typename T, typename ...Args>
    T* create(Args&& ... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        T* obj = new (ptr) T(std::forward<Args>(args)...);

        if constexpr (!std::is_trivially_destructible_v<T>) {
            append_new_destructor(obj, &destruct<T>);
        }

        return obj;
    }

    /**
    * @brief Clears the arena: destroys registered objects and resets offsets.
    *
    * All allocated objects become invalid. Memory blocks are retained for reuse.
    *
    * @post mem_block_latest_idx == 0
    * @post All destructors have been invoked.
    */
    void clear() {
        DestructorChunk* curr = destructor_block_latest;
        while (curr) {
            for (size_t i = curr->n_nodes; i > 0; i--) {
                curr->nodes[i - 1].fn(curr->nodes[i - 1].obj);
            }
            curr = curr->prev;
        }

        destructor_block_latest = nullptr;
        destructor_block_tail = nullptr;

        for (MemBlock& mb : mem_blocks) {
            mb.offset = 0;
        }

        mem_block_latest_idx = 0;
    }

    /**
     * @brief Allocates raw memory with user-specified alignment.
     *
     * This function is primarily used to work with STL allocator requirements.
     *
     * @param size Number of bytes to allocate.
     * @param align Required alignment.
     *
     * @return Pointer to an aligned memory region within the arena.
     */
    void* allocate_raw(const size_t size, const size_t align) noexcept {
        return allocate(size, align);
    }


    /** @return Total bytes of all allocated memory blocks. */
    [[nodiscard]] size_t get_arena_size() const {return arena_size;}

    /** @return Default bytes of a newly created memory block. */
    [[nodiscard]] size_t get_single_block_size() const {return block_size;}

    /** @return Number of memory blocks allocated so far. */
    [[nodiscard]] size_t get_number_of_allocated_blocks() const {return mem_blocks.size();}

private:
    /**
     * @brief Destroys objects of type T.
     *
     * Pointers are stored as `void*` internally, so this re-casts and calls the destructor manually.
     * Primary purpose of this template is to allow for compile-time dispatch, reducing the need to maintain vtable.
     */
    template<typename T>
    static void destruct(void* p) noexcept {
        static_cast<T*>(p)->~T();
    }

    /**
     * @brief Node storing a destructor function and the associated object.
     */
    struct DestructorNode {
        void (*fn)(void*);
        void* obj;
    };

    /**
    * @brief Chunk of destructor records, maintained as a linked list.
    *
    * Chunks are allocated inside the arena and chained backwards.
    * Deallocation is also conducted backwards to align to lifetime semantics.
    */
    struct DestructorChunk {
        DestructorNode nodes[DESTRUCTOR_CHUNK_SIZE]{};
        size_t n_nodes = 0;
        DestructorChunk* prev{};
    };

    /**
     * @brief Represents one contiguous memory block owned by the arena.
     */
    struct MemBlock {
        char* buffer;
        size_t size;
        size_t offset;

        explicit MemBlock(const size_t size): buffer(static_cast<char*>(::operator new(size))), size(size), offset(0) {}

        ~MemBlock() {
            ::operator delete(buffer);
        }

        MemBlock(MemBlock&& other) noexcept : buffer(other.buffer), size(other.size), offset(other.offset) {
            other.buffer = nullptr;
            other.size = 0;
            other.offset = 0;
        }

        MemBlock& operator=(MemBlock&& other) noexcept {
            if (this != &other) {
                ::operator delete(buffer);
                buffer = other.buffer;
                size = other.size;
                offset = other.offset;
                other.buffer = nullptr;
                other.size = 0;
                other.offset = 0;
            }
            return *this;
        }

        MemBlock(const MemBlock&) = delete;
        MemBlock& operator=(const MemBlock&) = delete;
    };

    DestructorChunk* destructor_block_tail = nullptr;
    DestructorChunk* destructor_block_latest = nullptr;

    std::vector<MemBlock> mem_blocks;
    size_t mem_block_latest_idx;

    size_t block_size;
    size_t arena_size;

    /**
     * @brief Allocates a new block.
     *
     * Updates current arena size and sets new memory block as the latest memory block to allocate from.
     */
    inline void add_mem_block(const size_t size) noexcept {
        mem_blocks.emplace_back(size);
        mem_block_latest_idx = mem_blocks.size() - 1;
        arena_size += size;
    };

    /**
     * @brief Allocation routine for the bump-pointer allocator.
     *
     * Attempts to allocate within the current block.
     * If current block is full, we create a new memory block.
     *
     * @return Pointer to the allocated memory.
     */
    inline void* allocate(const size_t size, const size_t align) noexcept {
        MemBlock& mb = mem_blocks[mem_block_latest_idx];

        // cheap allocation if alignment matches
        if (const size_t potential_offset = mb.offset + size; potential_offset <= mb.size) [[likely]] {

            // manual alignment check
            // 1. `align` is always a power of two.
            //      align = 16 → binary 00010000
            //
            // 2. `align - 1` produces a mask of the lower bits.
            //       align - 1 = 15 → binary 00001111
            //
            // 3. For any aligned address `A`, the lower bits must be zero:
            //       A % 16 == 0
            //       A & 0x0F == 0

            if (void *const ptr = mb.buffer + mb.offset; (reinterpret_cast<uintptr_t>(ptr) & (align - 1)) == 0) [[likely
            ]] {
                mb.offset = potential_offset;
                return ptr;
            }
        }

        if (void *ptr = allocate_from_mem_block(mb, size, align)) [[likely]] {
            return ptr;
        }

        // memory block is full.
        return add_new_block_and_allocate(size, align);
    }

    /**
    * @brief Allocates a larger block if needed and retries allocation.
    *
    * We assume most allocation to the latest `MemBlock` will succeed.
    * Hence, this function should not be called often and is marked as non-inline for better code layout.
    *
    * @return Pointer to aligned memory.
    */
    __attribute__((noinline))
    void* add_new_block_and_allocate(const size_t size, const size_t align) noexcept {
        const size_t new_block_size = std::max(size + align - 1, block_size);
        add_mem_block(new_block_size);

        void* p = allocate_from_mem_block(mem_blocks[mem_block_latest_idx], size, align);
        return p;
    }

    /**
     * @brief Attempts to allocate memory from a specific block.
     *
     * Applies alignment padding and updates the block offset on success.
     *
     * @return Pointer to aligned memory, or nullptr if the block overflows.
     */
    static inline void* allocate_from_mem_block(MemBlock& mem_block, const size_t size, const size_t align) noexcept {
        const uintptr_t curr = reinterpret_cast<uintptr_t>(mem_block.buffer + mem_block.offset);

        // manual alignment logic
        // 1. `align` is always a power of two.
        //
        // 2. `align - 1` creates a mask of the lower bits:
        //       align - 1 = 15 → binary 00001111
        //
        // 3. `~(align - 1)` inverts those bits to create an "alignment mask":
        //       ~(00001111) = 11110000
        //    This mask clears the lower 4 bits when AND-ed with a pointer,
        //    effectively rounding *down* to a multiple of 16.
        //
        // 4. Adding `align - 1` before masking converts "round down" into a "round up":
        //       curr = 0x1013                 (not aligned)
        //       curr + 15 = 0x1022            (guaranteed ≥ next boundary)
        //       0x1022 & 0xFFFFFFF0 = 0x1020  (aligned address)
        //    So the next valid 16-byte-aligned address is 0x1020.
        //
        // 5. The bytes from 0x1013 → 0x1020 become padding.

        const uintptr_t aligned = (curr + align - 1) & ~(align - 1);
        const size_t padding = aligned - curr;
        const size_t new_offset = mem_block.offset + padding + size;

        if (new_offset > mem_block.size) [[unlikely]] return nullptr;

        mem_block.offset = new_offset;
        return reinterpret_cast<void*>(aligned);
    }

    /**
     * @brief Registers a destructor a new object.
     *
     * Destructors are stored in a linked list of chunks. A new chunk is created if the current chunk if full.
     *
     * @param obj Pointer to the object.
     * @param fn Pointer to the destructor function.
     */
    inline void append_new_destructor(void* obj, void (*fn)(void*)) noexcept {
        if (!destructor_block_latest || destructor_block_latest->n_nodes == DESTRUCTOR_CHUNK_SIZE) [[unlikely]] {
            void* ptr = allocate(sizeof(DestructorChunk), alignof(DestructorChunk));
            DestructorChunk* dest_mb = new (ptr) DestructorChunk();
            dest_mb->n_nodes = 0;
            dest_mb->prev = destructor_block_latest;

            if (!destructor_block_latest) {
                destructor_block_tail = dest_mb;
            }

            destructor_block_latest = dest_mb;
        }

        DestructorNode& node = destructor_block_latest->nodes[destructor_block_latest->n_nodes++];
        node.fn = fn;
        node.obj = obj;
    }
};

#endif //ARENAV2_H
