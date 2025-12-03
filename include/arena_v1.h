//
// Created by Yeo Shu Heng on 2/12/25.
//

#ifndef ARENA_H
#define ARENA_H

#include <vector>
#include <functional>
#include <memory>

inline constexpr size_t DEFAULT_BLOCK_SIZE = 1024;

class Arena {
public:
    explicit Arena() : tail(nullptr), block_size(DEFAULT_BLOCK_SIZE), arena_size(DEFAULT_BLOCK_SIZE) {
        add_mem_block(DEFAULT_BLOCK_SIZE);
    }

    explicit Arena(const size_t size): tail(nullptr), block_size(size), arena_size(size) {
        add_mem_block(size);
    };

    ~Arena() {
        cleanup();
    }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&& other) noexcept : tail(other.tail), mem_blocks(std::move(other.mem_blocks)), block_size(other.block_size), arena_size(other.arena_size) {
        other.tail = nullptr;
        other.arena_size = 0;
    };

    Arena& operator=(Arena&& other) noexcept {
        if (&other != this) {
            clear();
            this->block_size = other.block_size;
            this->arena_size = other.arena_size;
            this->tail = other.tail;
            this->mem_blocks = std::move(other.mem_blocks);

            other.tail = nullptr;
            other.arena_size = 0;
        }
        return *this;
    }

    template<typename T, typename ...Args>
    T* create(Args&& ... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        T* obj = new (ptr) T(std::forward<Args>(args)...);

        if constexpr (!std::is_trivially_destructible_v<T>) {
            append_new_destructor(obj, &destruct<T>);
        }

        return obj;
    }

    void clear() {
        DestructorBlock* d = tail;
        while (d) {
            d->fn(d->obj);
            d = d->next;
        }

        tail = nullptr;

        for (MemBlock* blk : mem_blocks) {
            blk->offset = 0;
        }
    }

    [[nodiscard]] size_t get_arena_size() const {return arena_size;}
    [[nodiscard]] size_t get_single_block_size() const {return block_size;}
    [[nodiscard]] size_t get_number_of_allocated_blocks() const {return mem_blocks.size();}

private:
    template<typename T>
    static void destruct(void* p) noexcept {
        static_cast<T*>(p)->~T();
    }

    struct DestructorBlock {
        void (*fn)(void*);
        void* obj;
        DestructorBlock* next;
    };

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

    DestructorBlock* tail;
    std::vector<MemBlock*> mem_blocks;
    size_t block_size;
    size_t arena_size;

    inline void add_mem_block(const size_t size) noexcept {
        mem_blocks.push_back(new MemBlock(size));
    };

    inline void cleanup() noexcept {
        clear();
        for (const MemBlock* blk : mem_blocks) {
            delete blk;
        }
    }

    inline void* allocate(const size_t size, const size_t align) noexcept {
        void* p;
        if ((p = allocate_from_mem_block(*mem_blocks.back(), size, align))) {
            return p;
        }

        const size_t new_block_size = std::max(size + align, block_size * 2);
        add_mem_block(new_block_size);
        arena_size += new_block_size;

        p = allocate_from_mem_block(*mem_blocks.back(), size, align);
        return p;
    }

    static inline void* allocate_from_mem_block(MemBlock& mem_block, const size_t size, const size_t align) noexcept {
        const size_t curr_offset = mem_block.offset;
        size_t remaining_space = mem_block.size - mem_block.offset;
        void* ptr = mem_block.buffer + curr_offset;

        void* aligned_ptr = ptr;

        if (!std::align(align, size, aligned_ptr, remaining_space)) return nullptr;

        const size_t new_offset = static_cast<char*>(aligned_ptr) - mem_block.buffer + size;

        if (new_offset > mem_block.size) return nullptr;

        mem_block.offset = new_offset;
        return aligned_ptr;
    }

    inline void append_new_destructor(void* obj, void (*fn)(void*)) noexcept {
        DestructorBlock* dn = static_cast<DestructorBlock *>(allocate(sizeof(DestructorBlock), alignof(DestructorBlock)));
        dn->fn = fn;
        dn->next = tail;
        dn->obj = obj;
        tail = dn;
    }
};

#endif //ARENA_H
