#include "tig/timer.h"

#include <windows.h>

#include <gtest/gtest.h>

class TimerTest : public testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(tig_timer_init(NULL), TIG_OK);
    }

    void TearDown() override
    {
        tig_timer_exit();
    }
};

TEST_F(TimerTest, Monotonic)
{
    tig_timestamp_t start;
    ASSERT_EQ(tig_timer_now(&start), TIG_OK);

    SleepEx(1000, FALSE);

    tig_timestamp_t end;
    ASSERT_EQ(tig_timer_now(&end), TIG_OK);

    EXPECT_GT(end, start);
}

TEST_F(TimerTest, Between)
{
    tig_timestamp_t start;
    ASSERT_EQ(tig_timer_now(&start), TIG_OK);

    SleepEx(1000, FALSE);

    tig_timestamp_t end;
    ASSERT_EQ(tig_timer_now(&end), TIG_OK);

    EXPECT_GE(tig_timer_between(end, start), 1000);
}
