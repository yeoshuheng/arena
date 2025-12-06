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

class ArenaV2 {
public:
    explicit ArenaV2() : mem_block_latest_idx(0), block_size(DEFAULT_BLOCK_SIZE), arena_size(0) {
        add_mem_block(DEFAULT_BLOCK_SIZE);
    }

    explicit ArenaV2(const size_t size) : mem_block_latest_idx(0), block_size(size), arena_size(0) {
        add_mem_block(size);
    };

    ~ArenaV2() {
        clear();
    }

    ArenaV2(const ArenaV2&) = delete;
    ArenaV2& operator=(const ArenaV2&) = delete;

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

    void* allocate_raw(const size_t size, const size_t align) noexcept {
        return allocate(size, align);
    }

    [[nodiscard]] size_t get_arena_size() const {return arena_size;}
    [[nodiscard]] size_t get_single_block_size() const {return block_size;}
    [[nodiscard]] size_t get_number_of_allocated_blocks() const {return mem_blocks.size();}

private:
    template<typename T>
    static void destruct(void* p) noexcept {
        static_cast<T*>(p)->~T();
    }

    struct DestructorNode {
        void (*fn)(void*);
        void* obj;
    };

    struct DestructorChunk {
        DestructorNode nodes[DESTRUCTOR_CHUNK_SIZE]{};
        size_t n_nodes = 0;
        DestructorChunk* prev{};
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

    DestructorChunk* destructor_block_tail = nullptr;
    DestructorChunk* destructor_block_latest = nullptr;

    std::vector<MemBlock> mem_blocks;
    size_t mem_block_latest_idx;

    size_t block_size;
    size_t arena_size;

    inline void add_mem_block(const size_t size) noexcept {
        mem_blocks.emplace_back(size);
        mem_block_latest_idx = mem_blocks.size() - 1;
        arena_size += size;
    };

    inline void* allocate(const size_t size, const size_t align) noexcept {
        MemBlock& mb = mem_blocks[mem_block_latest_idx];

        if (const size_t potential_offset = mb.offset + size; potential_offset <= mb.size) [[likely]] {
            if (void* const ptr = mb.buffer + mb.offset; (reinterpret_cast<uintptr_t>(ptr) & (align - 1)) == 0) [[likely]] {
                mb.offset = potential_offset;
                return ptr;
            }
        }

        if (void* ptr = allocate_from_mem_block(mb, size, align)) [[likely]] {
            return ptr;
        }

        return add_new_block_and_allocate(size, align);
    }

    __attribute__((noinline))
    void* add_new_block_and_allocate(const size_t size, const size_t align) noexcept {
        const size_t new_block_size = std::max(size + align - 1, block_size);
        add_mem_block(new_block_size);

        void* p = allocate_from_mem_block(mem_blocks[mem_block_latest_idx], size, align);
        return p;
    }

    static inline void* allocate_from_mem_block(MemBlock& mem_block, const size_t size, const size_t align) noexcept {
        const uintptr_t curr = reinterpret_cast<uintptr_t>(mem_block.buffer + mem_block.offset);
        const uintptr_t aligned = (curr + align - 1) & ~(align - 1);
        const size_t padding = aligned - curr;
        const size_t new_offset = mem_block.offset + padding + size;

        if (new_offset > mem_block.size) [[unlikely]] return nullptr;

        mem_block.offset = new_offset;
        return reinterpret_cast<void*>(aligned);
    }

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
