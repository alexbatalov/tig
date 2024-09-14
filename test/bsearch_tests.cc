#include "tig/bsearch.h"

#include <gtest/gtest.h>

static constexpr int kData[] = { 1, 3, 5, 7, 9 };
static constexpr size_t kDataSize = sizeof(kData) / sizeof(kData[0]);

static int compare(const void* a, const void* b)
{
    return *(const int*)a - *(const int*)b;
}

TEST(TigBinarySearch, FindsExisting)
{
    int value;
    void* ptr;
    int exists;

    value = 1;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[0]));
    EXPECT_EQ(exists, 1);

    value = 3;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[1]));
    EXPECT_EQ(exists, 1);

    value = 5;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[2]));
    EXPECT_EQ(exists, 1);

    value = 7;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[3]));
    EXPECT_EQ(exists, 1);

    value = 9;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[4]));
    EXPECT_EQ(exists, 1);
}

TEST(TigBinarySearch, FindsInsertionPoints)
{
    int value;
    void* ptr;
    int exists;

    value = 0;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[0]));
    EXPECT_EQ(exists, 0);

    value = 2;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[1]));
    EXPECT_EQ(exists, 0);

    value = 4;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[2]));
    EXPECT_EQ(exists, 0);

    value = 6;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[3]));
    EXPECT_EQ(exists, 0);

    value = 8;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[4]));
    EXPECT_EQ(exists, 0);

    value = 10;
    ptr = tig_bsearch(&value, kData, kDataSize, sizeof(kData[0]), compare, &exists);
    EXPECT_EQ(ptr, &(kData[5]));
    EXPECT_EQ(exists, 0);
}
