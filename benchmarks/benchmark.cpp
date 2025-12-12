#include <benchmark/benchmark.h>
#include <vector>
#include <list>
#include <unordered_map>
#include "../include/arena.h"
#include "../include/allocator.h"

constexpr int64_t BENCHMARK_RANGE_START = 1<<10;
constexpr int64_t BENCHMARK_RANGE_END = 1<<12;

static void benchmark_vector_malloc(benchmark::State& state) {
    const size_t n = state.range(0);

    for (auto _ : state) {
        std::vector<int> vec;

        for (size_t i = 0; i < n; ++i) {
            vec.push_back(static_cast<int>(i));
        }

        benchmark::DoNotOptimize(vec.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * n);
}

static void benchmark_vector_arena(benchmark::State& state) {
    const size_t n = state.range(0);

    for (auto _ : state) {
        ArenaV2 arena(8192);
        std::vector<int, ArenaAllocator<int>> vec{ArenaAllocator<int>(arena)};

        for (size_t i = 0; i < n; ++i) {
            vec.push_back(static_cast<int>(i));
        }

        benchmark::DoNotOptimize(vec.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * n);
}

BENCHMARK(benchmark_vector_malloc)->Range(BENCHMARK_RANGE_START, BENCHMARK_RANGE_END);
BENCHMARK(benchmark_vector_arena)->Range(BENCHMARK_RANGE_START, BENCHMARK_RANGE_END);

static void benchmark_list_malloc(benchmark::State& state) {
    const size_t n = state.range(0);

    for (auto _ : state) {
        std::list<int> lst;

        for (size_t i = 0; i < n; ++i) {
            lst.push_back(static_cast<int>(i));
        }

        benchmark::DoNotOptimize(lst.size());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * n);
}

static void benchmark_list_arena(benchmark::State& state) {
    const size_t n = state.range(0);

    for (auto _ : state) {
        ArenaV2 arena(32768);
        std::list<int, ArenaAllocator<int>> lst{ArenaAllocator<int>(arena)};

        for (size_t i = 0; i < n; ++i) {
            lst.push_back(static_cast<int>(i));
        }

        benchmark::DoNotOptimize(lst.size());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * n);
}

BENCHMARK(benchmark_list_malloc)->Range(BENCHMARK_RANGE_START, BENCHMARK_RANGE_END);
BENCHMARK(benchmark_list_arena)->Range(BENCHMARK_RANGE_START, BENCHMARK_RANGE_END);

static void benchmark_unordered_map_malloc(benchmark::State& state) {
    const size_t n = state.range(0);

    for (auto _ : state) {
        std::unordered_map<int, int> map;

        for (size_t i = 0; i < n; ++i) {
            map[static_cast<int>(i)] = static_cast<int>(i * 2);
        }

        benchmark::DoNotOptimize(map.size());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * n);
}

static void benchmark_unordered_map_arena(benchmark::State& state) {
    const size_t n = state.range(0);

    for (auto _ : state) {
        ArenaV2 arena(65536);
        std::unordered_map<int, int, std::hash<int>, std::equal_to<int>, ArenaAllocator<std::pair<const int, int>>> map(10, std::hash<int>(), std::equal_to<int>(), ArenaAllocator<std::pair<const int, int>>(arena));

        for (size_t i = 0; i < n; ++i) {
            map[static_cast<int>(i)] = static_cast<int>(i * 2);
        }

        benchmark::DoNotOptimize(map.size());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * n);
}

BENCHMARK(benchmark_unordered_map_malloc)->Range(BENCHMARK_RANGE_START, BENCHMARK_RANGE_END);
BENCHMARK(benchmark_unordered_map_arena)->Range(BENCHMARK_RANGE_START, BENCHMARK_RANGE_END);

static void benchmark_map_malloc(benchmark::State& state) {
    const size_t n = state.range(0);

    for (auto _ : state) {
        std::map<int, int> map;

        for (size_t i = 0; i < n; ++i) {
            map[static_cast<int>(i)] = static_cast<int>(i * 2);
        }

        benchmark::DoNotOptimize(map.size());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * n);
}

static void benchmark_map_arena(benchmark::State& state) {
    const size_t n = state.range(0);

    for (auto _ : state) {
        ArenaV2 arena(65536);
        std::map<int, int, std::less<int>, ArenaAllocator<std::pair<const int, int>>> map{std::less<int>(),ArenaAllocator<std::pair<const int, int>>(arena)};

        for (size_t i = 0; i < n; ++i) {
            map[static_cast<int>(i)] = static_cast<int>(i * 2);
        }

        benchmark::DoNotOptimize(map.size());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * n);
}

BENCHMARK(benchmark_map_malloc)->Range(BENCHMARK_RANGE_START, BENCHMARK_RANGE_END);
BENCHMARK(benchmark_map_arena)->Range(BENCHMARK_RANGE_START, BENCHMARK_RANGE_END);

BENCHMARK_MAIN();