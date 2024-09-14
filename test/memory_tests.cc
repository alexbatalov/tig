#include "tig/memory.h"

#include <gtest/gtest.h>

#define my_calloc(count, size) tig_memory_calloc(count, size, __FILE__, __LINE__)
#define my_malloc(size) tig_memory_alloc(size, __FILE__, __LINE__)
#define my_realloc(ptr, size) tig_memory_realloc(ptr, size, __FILE__, __LINE__)
#define my_free(ptr) tig_memory_free(ptr, __FILE__, __LINE__)

static const unsigned char expected_start_guard[4] = { 0xAA, 0xAA, 0xAA, 0xAA };
static const unsigned char expected_end_guard[4] = { 0xBB, 0xBB, 0xBB, 0xBB };

class TigMemoryTest : public testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(tig_memory_init(nullptr), TIG_OK);
    }

    void TearDown() override
    {
        ASSERT_TRUE(tig_memory_validate_memory_leaks());
        tig_memory_exit();
    }
};

TEST_F(TigMemoryTest, Alloc)
{
    void* mem = my_malloc(4);
    ASSERT_NE(mem, nullptr);

    EXPECT_EQ(memcmp((unsigned char*)mem - 4, expected_start_guard, 4), 0);
    EXPECT_EQ(memcmp((unsigned char*)mem + 4, expected_end_guard, 4), 0);

    my_free(mem);
}

TEST_F(TigMemoryTest, Realloc)
{
    void* mem = my_malloc(2);
    ASSERT_NE(mem, nullptr);

    mem = my_realloc(mem, 4);
    ASSERT_NE(mem, nullptr);

    EXPECT_EQ(memcmp((unsigned char*)mem + 4, expected_end_guard, 4), 0);

    my_free(mem);
}

TEST_F(TigMemoryTest, Calloc)
{
    void* mem = my_calloc(256, 4);
    ASSERT_NE(mem, nullptr);

    unsigned char expected[1024];
    memset(expected, 0, sizeof(expected));

    ASSERT_EQ(memcmp(mem, expected, 1024), 0);

    my_free(mem);
}

TEST_F(TigMemoryTest, Stats)
{
    TigMemoryStats stats;

    tig_memory_reset_stats();

    void* mem1 = my_malloc(1);
    void* mem5 = my_malloc(5);
    void* mem2 = my_malloc(2);
    void* mem4 = my_malloc(4);
    void* mem3 = my_malloc(3);

    my_free(mem2);
    my_free(mem1);

    tig_memory_stats(&stats);
    ASSERT_EQ(stats.current_blocks, 3);
    ASSERT_EQ(stats.current_allocated, 5 + 4 + 3);
    ASSERT_EQ(stats.max_blocks, 5);
    ASSERT_EQ(stats.max_allocated, 1 + 2 + 3 + 4 + 5);

    my_free(mem5);
    my_free(mem4);
    my_free(mem3);

    tig_memory_stats(&stats);
    ASSERT_EQ(stats.current_blocks, 0);
    ASSERT_EQ(stats.current_allocated, 0);
}
