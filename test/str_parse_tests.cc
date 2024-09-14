#include "tig/str_parse.h"

#include <gtest/gtest.h>

class StrParseTest : public testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(tig_str_parse_init(NULL), TIG_OK);
    }

    void TearDown() override
    {
        tig_str_parse_exit();
    }
};

TEST_F(StrParseTest, ParseStrValue)
{
    char buffer[] = "foo, bar";

    char* str = buffer;
    char value[32];

    tig_str_parse_str_value(&str, value);
    EXPECT_STREQ(value, "foo");

    tig_str_parse_str_value(&str, value);
    EXPECT_STREQ(value, "bar");

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, ParseValue)
{
    char buffer[] = "1, 2147483647, -2147483648";
    char* str = buffer;
    int value;

    tig_str_parse_value(&str, &value);
    EXPECT_EQ(value, 1);

    tig_str_parse_value(&str, &value);
    EXPECT_EQ(value, 2147483647);

    tig_str_parse_value(&str, &value);
    EXPECT_EQ(value, -2147483647 - 1);

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, ParseValue64)
{
    char buffer[] = "9223372036854775807, -9223372036854775808";
    char* str = buffer;
    int64_t value;

    tig_str_parse_value_64(&str, &value);
    EXPECT_EQ(value, 9223372036854775807ll);

    tig_str_parse_value_64(&str, &value);
    EXPECT_EQ(value, -9223372036854775808ll);

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, ParseRange)
{
    char buffer[] = "1-3, 5";
    char* str = buffer;
    int start;
    int end;

    tig_str_parse_range(&str, &start, &end);
    EXPECT_EQ(start, 1);
    EXPECT_EQ(end, 3);

    tig_str_parse_range(&str, &start, &end);
    EXPECT_EQ(start, 5);
    EXPECT_EQ(end, 5);

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, ParseComplexValue)
{
    char buffer[] = "(1: 2), (3), (2147483647:-2147483648)";

    char* str = buffer;
    int value1;
    int value2;

    tig_str_parse_complex_value(&str, ':', &value1, &value2);
    EXPECT_EQ(value1, 1);
    EXPECT_EQ(value2, 2);

    tig_str_parse_complex_value(&str, ':', &value1, &value2);
    EXPECT_EQ(value1, 3);
    EXPECT_EQ(value2, 3);

    tig_str_parse_complex_value(&str, ':', &value1, &value2);
    EXPECT_EQ(value1, 2147483647);
    EXPECT_EQ(value2, -2147483647 - 1);

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, ParseComplexValue3)
{
    char buffer[] = "(1;2;3), (5; 6), (7), (0; 2147483647; -2147483648)";

    char* str = buffer;
    int value1;
    int value2;
    int value3;

    tig_str_parse_complex_value3(&str, ';', &value1, &value2, &value3);
    EXPECT_EQ(value1, 1);
    EXPECT_EQ(value2, 2);
    EXPECT_EQ(value3, 3);

    tig_str_parse_complex_value3(&str, ';', &value1, &value2, &value3);
    EXPECT_EQ(value1, 5);
    EXPECT_EQ(value2, 6);
    EXPECT_EQ(value3, 6);

    tig_str_parse_complex_value3(&str, ';', &value1, &value2, &value3);
    EXPECT_EQ(value1, 7);
    EXPECT_EQ(value2, 7);
    EXPECT_EQ(value3, 7);

    tig_str_parse_complex_value3(&str, ';', &value1, &value2, &value3);
    EXPECT_EQ(value1, 0);
    EXPECT_EQ(value2, 2147483647);
    EXPECT_EQ(value3, -2147483647 - 1);

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, ParseComplexStrValue)
{
    char buffer[] = "(foo*1), (bar*3), (baz*5)";
    const char* values[] = {
        "foo",
        "bar",
        "baz",
    };

    char* str = buffer;
    int value1;
    int value2;

    tig_str_parse_complex_str_value(&str, '*', values, 3, &value1, &value2);
    EXPECT_EQ(value1, 0);
    EXPECT_EQ(value2, 1);

    tig_str_parse_complex_str_value(&str, '*', values, 3, &value1, &value2);
    EXPECT_EQ(value1, 1);
    EXPECT_EQ(value2, 3);

    tig_str_parse_complex_str_value(&str, '*', values, 3, &value1, &value2);
    EXPECT_EQ(value1, 2);
    EXPECT_EQ(value2, 5);

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, MatchStrToList)
{
    char buffer[] = "bar, foo, baz";
    const char* identifiers[] = {
        "foo",
        "bar",
        "baz",
    };

    char* str = buffer;
    int value;

    tig_str_match_str_to_list(&str, identifiers, 3, &value);
    EXPECT_EQ(value, 1);

    tig_str_match_str_to_list(&str, identifiers, 3, &value);
    EXPECT_EQ(value, 0);

    tig_str_match_str_to_list(&str, identifiers, 3, &value);
    EXPECT_EQ(value, 2);

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, ParseFlagList)
{
    char buffer[] = "foo, bar|baz, foo | bar | baz";
    const char* keys[] = {
        "foo",
        "bar",
        "baz",
    };
    const unsigned int values[] = {
        0x1,
        0x2,
        0x4,
    };

    char* str = buffer;
    unsigned int value;

    tig_str_parse_flag_list(&str, keys, values, 3, &value);
    EXPECT_EQ(value, 0x1);

    tig_str_parse_flag_list(&str, keys, values, 3, &value);
    EXPECT_EQ(value, 0x2 | 0x4);

    tig_str_parse_flag_list(&str, keys, values, 3, &value);
    EXPECT_EQ(value, 0x1 | 0x2 | 0x4);

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, ParseFlagList64)
{
    char buffer[] = "foo, bar|baz, foo | bar | baz";
    const char* keys[] = {
        "foo",
        "bar",
        "baz",
    };
    const uint64_t values[] = {
        0x1000000000ull,
        0x200000000000ull,
        0x8000000000000000ull,
    };

    char* str = buffer;
    uint64_t value;

    tig_str_parse_flag_list_64(&str, keys, values, 3, &value);
    EXPECT_EQ(value, 0x1000000000ull);

    tig_str_parse_flag_list_64(&str, keys, values, 3, &value);
    EXPECT_EQ(value, 0x8000000000000000ull | 0x200000000000ull);

    tig_str_parse_flag_list_64(&str, keys, values, 3, &value);
    EXPECT_EQ(value, 0x8000000000000000ull | 0x200000000000ull | 0x1000000000ull);

    EXPECT_EQ(str, nullptr);
}

TEST_F(StrParseTest, ParseNamedValue)
{
    char buffer[] = "one 1, two 2147483647, three -2147483648";

    char* str = buffer;
    int value;

    EXPECT_FALSE(tig_str_parse_named_value(&str, "zero", &value));
    EXPECT_TRUE(tig_str_parse_named_value(&str, "one", &value));
    EXPECT_FALSE(tig_str_parse_named_value(&str, "three", &value));
    EXPECT_TRUE(tig_str_parse_named_value(&str, "two", &value));
    EXPECT_TRUE(tig_str_parse_named_value(&str, "three", &value));
    EXPECT_EQ(str, nullptr);
    EXPECT_FALSE(tig_str_parse_named_value(&str, "four", &value));
}

TEST_F(StrParseTest, MatchNamedStrToList)
{
    char buffer[] = "one bar, two foo, three baz";
    const char* identifiers[] = {
        "foo",
        "bar",
        "baz",
    };

    char* str = buffer;
    int value;

    EXPECT_FALSE(tig_str_match_named_str_to_list(&str, "zero", identifiers, 3, &value));
    EXPECT_TRUE(tig_str_match_named_str_to_list(&str, "one", identifiers, 3, &value));
    EXPECT_FALSE(tig_str_match_named_str_to_list(&str, "three", identifiers, 3, &value));
    EXPECT_TRUE(tig_str_match_named_str_to_list(&str, "two", identifiers, 3, &value));
    EXPECT_TRUE(tig_str_match_named_str_to_list(&str, "three", identifiers, 3, &value));
    EXPECT_EQ(str, nullptr);
    EXPECT_FALSE(tig_str_match_named_str_to_list(&str, "four", identifiers, 3, &value));
}

TEST_F(StrParseTest, ParseNamedRange)
{
    char buffer[] = "one 1-3, two -1-4, three -4--1";

    char* str = buffer;
    int start;
    int end;

    EXPECT_FALSE(tig_str_parse_named_range(&str, "zero", &start, &end));
    EXPECT_TRUE(tig_str_parse_named_range(&str, "one", &start, &end));
    EXPECT_FALSE(tig_str_parse_named_range(&str, "three", &start, &end));
    EXPECT_TRUE(tig_str_parse_named_range(&str, "two", &start, &end));
    EXPECT_TRUE(tig_str_parse_named_range(&str, "three", &start, &end));
    EXPECT_EQ(str, nullptr);
    EXPECT_FALSE(tig_str_parse_named_range(&str, "four", &start, &end));
}

TEST_F(StrParseTest, StrParseNamedFlagList)
{
    char buffer[] = "one foo, two bar|baz, three foo | bar | baz";
    const char* keys[] = {
        "foo",
        "bar",
        "baz",
    };
    const unsigned int values[] = {
        0x1,
        0x2,
        0x4,
    };

    char* str = buffer;
    unsigned int value;

    EXPECT_FALSE(tig_str_parse_named_flag_list(&str, "zero", keys, values, 3, &value));
    EXPECT_TRUE(tig_str_parse_named_flag_list(&str, "one", keys, values, 3, &value));
    EXPECT_FALSE(tig_str_parse_named_flag_list(&str, "three", keys, values, 3, &value));
    EXPECT_TRUE(tig_str_parse_named_flag_list(&str, "two", keys, values, 3, &value));
    EXPECT_TRUE(tig_str_parse_named_flag_list(&str, "three", keys, values, 3, &value));
    EXPECT_EQ(str, nullptr);
    EXPECT_FALSE(tig_str_parse_named_flag_list(&str, "four", keys, values, 3, &value));
}

TEST_F(StrParseTest, StrParseNamedComplexValue)
{
    char buffer[] = "one (1: 2), two (3), three (2147483647:-2147483648)";

    char* str = buffer;
    int value1;
    int value2;

    EXPECT_FALSE(tig_str_parse_named_complex_value(&str, "zero", ':', &value1, &value2));
    EXPECT_TRUE(tig_str_parse_named_complex_value(&str, "one", ':', &value1, &value2));
    EXPECT_FALSE(tig_str_parse_named_complex_value(&str, "three", ':', &value1, &value2));
    EXPECT_TRUE(tig_str_parse_named_complex_value(&str, "two", ':', &value1, &value2));
    EXPECT_TRUE(tig_str_parse_named_complex_value(&str, "three", ':', &value1, &value2));
    EXPECT_EQ(str, nullptr);
    EXPECT_FALSE(tig_str_parse_named_complex_value(&str, "four", ':', &value1, &value2));
}

TEST_F(StrParseTest, StrParseNamedComplexValue3)
{
    char buffer[] = "one (1;2;3), two (5; 6), three (7)";

    char* str = buffer;
    int value1;
    int value2;
    int value3;

    EXPECT_FALSE(tig_str_parse_named_complex_value3(&str, "zero", ';', &value1, &value2, &value3));
    EXPECT_TRUE(tig_str_parse_named_complex_value3(&str, "one", ';', &value1, &value2, &value3));
    EXPECT_FALSE(tig_str_parse_named_complex_value3(&str, "three", ';', &value1, &value2, &value3));
    EXPECT_TRUE(tig_str_parse_named_complex_value3(&str, "two", ';', &value1, &value2, &value3));
    EXPECT_TRUE(tig_str_parse_named_complex_value3(&str, "three", ';', &value1, &value2, &value3));
    EXPECT_EQ(str, nullptr);
    EXPECT_FALSE(tig_str_parse_named_complex_value3(&str, "four", ';', &value1, &value2, &value3));
}
