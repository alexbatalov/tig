#include "tig/idxtable.h"

#include <gtest/gtest.h>

#include "tig/file.h"
#include "tig/memory.h"

class TigIdxTableTest : public testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(tig_memory_init(NULL), TIG_OK);
    }

    void TearDown() override
    {
        ASSERT_TRUE(tig_memory_validate_memory_leaks());
        tig_memory_exit();
    }
};

TEST_F(TigIdxTableTest, Init)
{
    TigIdxTable idxtable;
    tig_idxtable_init(&idxtable, 1);

    EXPECT_GT(idxtable.buckets_count, 0);
    EXPECT_NE(idxtable.buckets, nullptr);

    for (int index = 0; index < idxtable.buckets_count; index++) {
        EXPECT_EQ(idxtable.buckets[index], nullptr) << "should be NULL at " << index;
    }

    EXPECT_EQ(idxtable.count, 0);

    tig_idxtable_exit(&idxtable);
}

TEST_F(TigIdxTableTest, Copy)
{
    TigIdxTable idxtable;
    tig_idxtable_init(&idxtable, sizeof(int));

    int value = 1;
    tig_idxtable_set(&idxtable, 100, &value);

    TigIdxTable copy;
    tig_idxtable_copy(&copy, &idxtable);

    EXPECT_EQ(copy.buckets_count, idxtable.buckets_count);
    EXPECT_NE(copy.buckets, nullptr);
    EXPECT_EQ(copy.value_size, sizeof(int));
    EXPECT_EQ(copy.count, 1);

    for (int index = 0; index < copy.buckets_count; index++) {
        if (index != 100 % copy.buckets_count) {
            EXPECT_EQ(idxtable.buckets[index], nullptr) << "should be NULL at " << index;
        } else {
            EXPECT_NE(idxtable.buckets[index], nullptr);
        }
    }

    EXPECT_NE(copy.buckets[100 % copy.buckets_count], nullptr);
    EXPECT_EQ(copy.buckets[100 % copy.buckets_count]->key, 100);
    EXPECT_NE(copy.buckets[100 % copy.buckets_count]->value, nullptr);
    EXPECT_EQ(memcmp(copy.buckets[100 % copy.buckets_count]->value, &value, sizeof(int)), 0);

    tig_idxtable_exit(&copy);
    tig_idxtable_exit(&idxtable);
}

TEST_F(TigIdxTableTest, Set)
{
    TigIdxTable idxtable;
    tig_idxtable_init(&idxtable, sizeof(int));

    int value = 13;
    tig_idxtable_set(&idxtable, 20, &value);

    EXPECT_EQ(idxtable.count, 1);
    EXPECT_NE(idxtable.buckets[20 % idxtable.buckets_count], nullptr);
    EXPECT_EQ(idxtable.buckets[20 % idxtable.buckets_count]->key, 20);
    EXPECT_NE(idxtable.buckets[20 % idxtable.buckets_count]->value, nullptr);
    EXPECT_EQ(memcmp(idxtable.buckets[20 % idxtable.buckets_count]->value, &value, sizeof(int)), 0);

    tig_idxtable_exit(&idxtable);
}

TEST_F(TigIdxTableTest, Get)
{
    TigIdxTable idxtable;
    tig_idxtable_init(&idxtable, sizeof(int));

    int value = 29;
    tig_idxtable_set(&idxtable, 7, &value);

    int stored;
    EXPECT_TRUE(tig_idxtable_get(&idxtable, 7, &stored));
    EXPECT_EQ(stored, 29);

    tig_idxtable_exit(&idxtable);
}

TEST_F(TigIdxTableTest, Remove)
{
    TigIdxTable idxtable;
    tig_idxtable_init(&idxtable, sizeof(int));

    int value = 31;
    tig_idxtable_set(&idxtable, 5, &value);

    tig_idxtable_remove(&idxtable, 5);

    EXPECT_EQ(idxtable.count, 0);
    EXPECT_EQ(idxtable.buckets[31 % idxtable.buckets_count], nullptr);

    tig_idxtable_exit(&idxtable);
}
