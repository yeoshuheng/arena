//
// Created by Yeo Shu Heng on 12/12/25.
//

#include <gtest/gtest.h>
#include "../include/arena.h"

struct TestStruct {
    int x, y;
    static int destruct_count;

    TestStruct(const int a, const int b) : x(a), y(b) {}
    ~TestStruct() { destruct_count++; }
};

int TestStruct::destruct_count = 0;

TEST(ArenaTest, TestBasicAllocation) {
    ArenaV2 arena(1024);

    int* a = arena.create<int>(42);
    EXPECT_EQ(*a, 42);

    auto* d = arena.create<double>(3.14);
    EXPECT_DOUBLE_EQ(*d, 3.14);
}

TEST(ArenaTest, TestStructAllocation) {
    TestStruct::destruct_count = 0;

    ArenaV2 arena(1024);

    auto* t1 = arena.create<TestStruct>(1, 2);
    EXPECT_EQ(t1->x, 1);
    EXPECT_EQ(t1->y, 2);

    auto* t2 = arena.create<TestStruct>(3, 4);
    EXPECT_EQ(t2->x, 3);
    EXPECT_EQ(t2->y, 4);

    arena.clear();
    EXPECT_EQ(TestStruct::destruct_count, 2);
}

TEST(ArenaTest, TestMemoryBlockAutoExpansion) {
    ArenaV2 arena(32);

    for (int i = 0; i < 100; ++i) {
        int* p = arena.create<int>(i);
        EXPECT_EQ(*p, i);
    }

    EXPECT_GT(arena.get_number_of_allocated_blocks(), 1);
}

TEST(ArenaTest, TestRawAllocation) {
    ArenaV2 arena(64);

    void* mem = arena.allocate_raw(32, alignof(double));
    EXPECT_NE(mem, nullptr);

    double* d = new (mem) double(2.71);
    EXPECT_DOUBLE_EQ(*d, 2.71);
}


