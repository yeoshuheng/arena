#ifndef BENCHMARK_UTILS_H
#define BENCHMARK_UTILS_H

#include <chrono>
#include <iostream>
#include <iomanip>

struct BenchmarkResult {
    std::string name;
    double insert_time_ms;
    double read_time_ms;
    double total_time_ms;
    size_t memory_used;
};

class Timer {
    std::chrono::high_resolution_clock::time_point start;
public:
    Timer() : start(std::chrono::high_resolution_clock::now()) {}

    [[nodiscard]] double elapsed_ms() const {
        const auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    void reset() {
        start = std::chrono::high_resolution_clock::now();
    }
};

inline void print_header() {
    std::cout << std::left << std::setw(45) << "Benchmark"
              << std::right
              << std::setw(12) << "Insert"
              << std::setw(12) << "Read"
              << std::setw(12) << "Total"
              << std::setw(15) << "Memory"
              << std::endl;
}

inline void print_result(const BenchmarkResult& r) {
    std::cout << std::left << std::setw(45) << r.name
              << std::right
              << std::setw(10) << std::fixed << std::setprecision(2) << r.insert_time_ms << " ms"
              << std::setw(10) << r.read_time_ms << " ms"
              << std::setw(10) << r.total_time_ms << " ms"
              << std::setw(12) << (r.memory_used / 1024.0) << " KB"
              << std::endl;
}

inline void print_speedup(const BenchmarkResult& arena, const BenchmarkResult& malloc) {
    const double insert_speedup = malloc.insert_time_ms / arena.insert_time_ms;
    const double read_speedup = malloc.read_time_ms / arena.read_time_ms;
    const double total_speedup = malloc.total_time_ms / arena.total_time_ms;

    std::cout << std::left << std::setw(45) << "  → Speedup"
              << std::right
              << std::setw(9) << std::fixed << std::setprecision(2) << insert_speedup << "x"
              << std::setw(9) << read_speedup << "x"
              << std::setw(9) << total_speedup << "x";

    if (total_speedup > 1.0) {
        std::cout << "  ✓";
    }
    std::cout << std::endl << std::endl;
}

#endif

