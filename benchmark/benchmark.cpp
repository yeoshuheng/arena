#include "../include/arena.h"
#include "../include/allocator.h"

#include "utils.h"

#include <vector>
#include <map>
#include <unordered_map>
#include <random>
#include <string>

template<typename Allocator>
BenchmarkResult benchmark_vector(const std::string& name, Allocator alloc, size_t n_elements) {
    BenchmarkResult result;
    result.name = name;

    Timer timer;

    std::vector<int, Allocator> vec(alloc);
    vec.reserve(n_elements);

    for (size_t i = 0; i < n_elements; i++) {
        vec.push_back(static_cast<int>(i));
    }
    result.insert_time_ms = timer.elapsed_ms();
    timer.reset();

    volatile long long sum = 0;
    for (size_t i = 0; i < n_elements; i++) {
        sum += vec[i];
    }
    for (const auto& v : vec) {
        sum += v;
    }

    result.read_time_ms = timer.elapsed_ms();
    result.total_time_ms = result.insert_time_ms + result.read_time_ms;
    result.memory_used = vec.capacity() * sizeof(int);

    return result;
}

template<typename Allocator>
BenchmarkResult benchmark_unordered_map(const std::string& name, Allocator alloc, size_t n_elements) {
    BenchmarkResult result;
    result.name = name;

    Timer timer;

    using MapAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::pair<const int, int>>;
    using MapType = std::unordered_map<int, int, std::hash<int>, std::equal_to<int>, MapAlloc>;

    MapType map(n_elements, std::hash<int>(), std::equal_to<int>(), MapAlloc(alloc));

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(n_elements * 10));

    for (size_t i = 0; i < n_elements; i++) {
        int key = dist(rng);
        map[key] = static_cast<int>(i);
    }
    result.insert_time_ms = timer.elapsed_ms();

    timer.reset();
    volatile long long sum = 0;

    rng.seed(42);
    for (size_t i = 0; i < n_elements; i++) {
        int key = dist(rng);
        auto it = map.find(key);
        if (it != map.end()) {
            sum += it->second;
        }
    }
    result.read_time_ms = timer.elapsed_ms();

    result.total_time_ms = result.insert_time_ms + result.read_time_ms;
    result.memory_used = map.size() * (sizeof(int) * 2 + 16);

    return result;
}

template<typename Allocator>
BenchmarkResult benchmark_string_vector(const std::string& name, Allocator alloc, size_t n_elements) {
    BenchmarkResult result;
    result.name = name;

    Timer timer;

    using StringAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::string>;
    std::vector<std::string, StringAlloc> vec{StringAlloc(alloc)};
    vec.reserve(n_elements);

    for (size_t i = 0; i < n_elements; i++) {
        vec.push_back("test_string_" + std::to_string(i) + "_with_some_extra_data");
    }
    result.insert_time_ms = timer.elapsed_ms();

    timer.reset();
    volatile size_t total_len = 0;
    for (const auto& s : vec) {
        total_len += s.length();
    }
    result.read_time_ms = timer.elapsed_ms();

    result.total_time_ms = result.insert_time_ms + result.read_time_ms;
    result.memory_used = vec.capacity() * sizeof(std::string);

    return result;
}

int main() {
    const size_t TEST_SIZE = 5000000;
    const size_t ARENA_BLOCK_SIZE = 1024 * 1024;

    {
        const auto malloc_result = benchmark_vector("std::vector<int> (malloc)", std::allocator<int>(), TEST_SIZE);
        print_result(malloc_result);

        ArenaV2 arena(ARENA_BLOCK_SIZE);
        const auto arena_result = benchmark_vector("std::vector<int> (ArenaV2)", ArenaAllocator<int>(arena), TEST_SIZE);
        print_result(arena_result);
        print_speedup(arena_result, malloc_result);
    }

    {
        auto malloc_result = benchmark_unordered_map("std::unordered_map<int,int> (malloc)", std::allocator<int>(), TEST_SIZE);
        print_result(malloc_result);

        ArenaV2 arena(ARENA_BLOCK_SIZE);
        auto arena_result = benchmark_unordered_map("std::unordered_map<int,int> (ArenaV2)", ArenaAllocator<int>(arena), TEST_SIZE);
        print_result(arena_result);
        print_speedup(arena_result, malloc_result);
    }

    {
        auto malloc_result = benchmark_string_vector("std::vector<string> (malloc)", std::allocator<std::string>(), TEST_SIZE);
        print_result(malloc_result);

        ArenaV2 arena(ARENA_BLOCK_SIZE);
        auto arena_result = benchmark_string_vector("std::vector<string> (ArenaV2)", ArenaAllocator<std::string>(arena), TEST_SIZE);
        print_result(arena_result);
        print_speedup(arena_result, malloc_result);
    }

    return 0;
}