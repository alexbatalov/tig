#include "tig/rect.h"

#include <gtest/gtest.h>

#include "tig/memory.h"

class TigRectNodeTest : public testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(tig_memory_init(NULL), TIG_OK);
        ASSERT_EQ(tig_rect_init(NULL), TIG_OK);
    }

    void TearDown() override
    {
        tig_rect_exit();

        ASSERT_TRUE(tig_memory_validate_memory_leaks());
        tig_memory_exit();
    }
};

TEST_F(TigRectNodeTest, CreateDestroyNode)
{
    TigRectListNode* ptrs[50];

    for (int index = 0; index < 50; index++) {
        ptrs[index] = tig_rect_node_create();
        ASSERT_NE(ptrs[index], nullptr);
    }

    for (int index = 0; index < 50; index++) {
        tig_rect_node_destroy(ptrs[index]);
    }
}

TEST(TigRectTest, Intersection)
{
    TigRect a;
    TigRect b;
    TigRect c;

    a = { 3, 7, 10, 10 };
    b = { 5, 11, 10, 10 };
    EXPECT_EQ(tig_rect_intersection(&a, &b, &c), TIG_OK);
    EXPECT_EQ(c.x, 5);
    EXPECT_EQ(c.y, 11);
    EXPECT_EQ(c.width, 8);
    EXPECT_EQ(c.height, 6);

    a = { 1, 2, 3, 4 };
    b = { 5, 6, 7, 8 };
    EXPECT_EQ(tig_rect_intersection(&a, &b, &c), TIG_ERR_NO_INTERSECTION);
}

TEST(TigRectTest, Union)
{
    TigRect a;
    TigRect b;
    TigRect c;

    a = { 3, 7, 10, 10 };
    b = { 5, 11, 10, 10 };
    EXPECT_EQ(tig_rect_union(&a, &b, &c), TIG_OK);
    EXPECT_EQ(c.x, 3);
    EXPECT_EQ(c.y, 7);
    EXPECT_EQ(c.width, 12);
    EXPECT_EQ(c.height, 14);
}

// -----------------------------------------------------------------------------
// CLIP TESTS
// -----------------------------------------------------------------------------

// The following tests check behaviour of `tig_rect_clip` function in various
// configurations. The math behind clipping is very simple, these tests are
// just illustrations.

//  +-------+
//  |       |
//  |  (b)  |-------------+                          +-------------+
//  |       |             |                          |     (0)     |
//  +-------+   (a)       |          =>          +---+-------------+
//      |                 |                      |         (1)     |
//      +-----------------+                      +-----------------+
TEST(TigRectTest, ClipTopLeft)
{
    TigRect a = { 10, 10, 10, 10 };
    TigRect b = { 7, 9, 5, 4 };
    TigRect r[4];

    EXPECT_EQ(tig_rect_clip(&a, &b, r), 2);

    EXPECT_EQ(r[0].x, 12);
    EXPECT_EQ(r[0].y, 10);
    EXPECT_EQ(r[0].width, 8);
    EXPECT_EQ(r[0].height, 3);

    EXPECT_EQ(r[1].x, 10);
    EXPECT_EQ(r[1].y, 13);
    EXPECT_EQ(r[1].width, 10);
    EXPECT_EQ(r[1].height, 7);
}

//          +---------+
//          |         |
//      +---|   (b)   |---+                      +---+         +---+
//      |   |         |   |                      |(0)|         |(1)|
//      |   +---------+   |          =>          +---+---------+---+
//      |       (a)       |                      |       (2)       |
//      +-----------------+                      +-----------------+
TEST(TigRectTest, ClipTopCenter)
{
    TigRect a = { 10, 10, 10, 10 };
    TigRect b = { 12, 9, 5, 4 };
    TigRect r[4];

    EXPECT_EQ(tig_rect_clip(&a, &b, r), 3);

    EXPECT_EQ(r[0].x, 10);
    EXPECT_EQ(r[0].y, 10);
    EXPECT_EQ(r[0].width, 2);
    EXPECT_EQ(r[0].height, 3);

    EXPECT_EQ(r[1].x, 17);
    EXPECT_EQ(r[1].y, 10);
    EXPECT_EQ(r[1].width, 3);
    EXPECT_EQ(r[1].height, 3);

    EXPECT_EQ(r[2].x, 10);
    EXPECT_EQ(r[2].y, 13);
    EXPECT_EQ(r[2].width, 10);
    EXPECT_EQ(r[2].height, 7);
}

//                    +-------+
//                    |       |
//      +-------------|  (b)  |                  +-------------+
//      |             |       |                  |     (0)     |
//      |       (a)   +-------+      =>          +-------------+---+
//      |                 |                      |     (1)         |
//      +-----------------+                      +-----------------+
TEST(TigRectTest, ClipTopRight)
{
    TigRect a = { 10, 10, 10, 10 };
    TigRect b = { 18, 9, 5, 4 };
    TigRect r[4];

    EXPECT_EQ(tig_rect_clip(&a, &b, r), 2);

    EXPECT_EQ(r[0].x, 10);
    EXPECT_EQ(r[0].y, 10);
    EXPECT_EQ(r[0].width, 8);
    EXPECT_EQ(r[0].height, 3);

    EXPECT_EQ(r[1].x, 10);
    EXPECT_EQ(r[1].y, 13);
    EXPECT_EQ(r[1].width, 10);
    EXPECT_EQ(r[1].height, 7);
}

//      +-----------------+                      +-----------------+
//      |                 |                      |       (0)       |
//  +-------+             |                      +---+-------------+
//  |  (b)  |   (a)       |          =>              |   (1)       |
//  +-------+             |                      +---+-------------+
//      |                 |                      |       (2)       |
//      +-----------------+                      +-----------------+
TEST(TigRectTest, ClipCenterLeft)
{
    TigRect a = { 10, 10, 10, 10 };
    TigRect b = { 7, 14, 5, 4 };
    TigRect r[4];

    EXPECT_EQ(tig_rect_clip(&a, &b, r), 3);

    EXPECT_EQ(r[0].x, 10);
    EXPECT_EQ(r[0].y, 10);
    EXPECT_EQ(r[0].width, 10);
    EXPECT_EQ(r[0].height, 4);

    EXPECT_EQ(r[1].x, 12);
    EXPECT_EQ(r[1].y, 14);
    EXPECT_EQ(r[1].width, 8);
    EXPECT_EQ(r[1].height, 4);

    EXPECT_EQ(r[2].x, 10);
    EXPECT_EQ(r[2].y, 18);
    EXPECT_EQ(r[2].width, 10);
    EXPECT_EQ(r[2].height, 2);
}

//      +-----------------+                      +-----------------+
//      |                 |                      |       (0)       |
//      |   +---------+   |                      +---+---------+---+
//      |   |   (b)   |   |          =>          |(1)|         |(2)|
//      |   +---------+   |                      +---+---------+---+
//      |       (a)       |                      |       (3)       |
//      +-----------------+                      +-----------------+
TEST(TigRectTest, ClipCenter)
{
    TigRect a = { 10, 10, 10, 10 };
    TigRect b = { 12, 14, 5, 4 };
    TigRect r[4];

    EXPECT_EQ(tig_rect_clip(&a, &b, r), 4);

    EXPECT_EQ(r[0].x, 10);
    EXPECT_EQ(r[0].y, 10);
    EXPECT_EQ(r[0].width, 10);
    EXPECT_EQ(r[0].height, 4);

    EXPECT_EQ(r[1].x, 10);
    EXPECT_EQ(r[1].y, 14);
    EXPECT_EQ(r[1].width, 2);
    EXPECT_EQ(r[1].height, 4);

    EXPECT_EQ(r[2].x, 17);
    EXPECT_EQ(r[2].y, 14);
    EXPECT_EQ(r[2].width, 3);
    EXPECT_EQ(r[2].height, 4);

    EXPECT_EQ(r[3].x, 10);
    EXPECT_EQ(r[3].y, 18);
    EXPECT_EQ(r[3].width, 10);
    EXPECT_EQ(r[3].height, 2);
}

//      +-----------------+                      +-----------------+
//      |                 |                      |       (0)       |
//      |             +-------+                  +-------------+---+
//      |      (a)    |  (b)  |      =>          |       (1)   |
//      |             +-------+                  +-------------+---+
//      |                 |                      |       (2)       |
//      +-----------------+                      +-----------------+
TEST(TigRectTest, ClipCenterRight)
{
    TigRect a = { 10, 10, 10, 10 };
    TigRect b = { 18, 14, 5, 4 };
    TigRect r[4];

    EXPECT_EQ(tig_rect_clip(&a, &b, r), 3);

    EXPECT_EQ(r[0].x, 10);
    EXPECT_EQ(r[0].y, 10);
    EXPECT_EQ(r[0].width, 10);
    EXPECT_EQ(r[0].height, 4);

    EXPECT_EQ(r[1].x, 10);
    EXPECT_EQ(r[1].y, 14);
    EXPECT_EQ(r[1].width, 8);
    EXPECT_EQ(r[1].height, 4);

    EXPECT_EQ(r[2].x, 10);
    EXPECT_EQ(r[2].y, 18);
    EXPECT_EQ(r[2].width, 10);
    EXPECT_EQ(r[2].height, 2);
}

//      +-----------------+                      +-----------------+
//      |                 |                      |         (0)     |
//  +-------+   (a)       |          =>          +---+-------------+
//  |       |             |                          |     (1)     |
//  |  (b)  |-------------+                          +-------------+
//  |       |
//  +-------+
TEST(TigRectTest, ClipBottomRight)
{
    TigRect a = { 10, 10, 10, 10 };
    TigRect b = { 7, 18, 5, 4 };
    TigRect r[4];

    EXPECT_EQ(tig_rect_clip(&a, &b, r), 2);

    EXPECT_EQ(r[0].x, 10);
    EXPECT_EQ(r[0].y, 10);
    EXPECT_EQ(r[0].width, 10);
    EXPECT_EQ(r[0].height, 8);

    EXPECT_EQ(r[1].x, 12);
    EXPECT_EQ(r[1].y, 18);
    EXPECT_EQ(r[1].width, 8);
    EXPECT_EQ(r[1].height, 2);
}

//      +-----------------+                      +-----------------+
//      |       (a)       |                      |       (0)       |
//      |   +---------+   |          =>          +---+---------+---+
//      |   |         |   |                      |(1)|         |(2)|
//      +---|   (b)   |---+                      +---+         +---+
//          |         |
//          +---------+
TEST(TigRectTest, ClipBottomCenter)
{
    TigRect a = { 10, 10, 10, 10 };
    TigRect b = { 12, 18, 5, 4 };
    TigRect r[4];

    EXPECT_EQ(tig_rect_clip(&a, &b, r), 3);

    EXPECT_EQ(r[0].x, 10);
    EXPECT_EQ(r[0].y, 10);
    EXPECT_EQ(r[0].width, 10);
    EXPECT_EQ(r[0].height, 8);

    EXPECT_EQ(r[1].x, 10);
    EXPECT_EQ(r[1].y, 18);
    EXPECT_EQ(r[1].width, 2);
    EXPECT_EQ(r[1].height, 2);

    EXPECT_EQ(r[2].x, 17);
    EXPECT_EQ(r[2].y, 18);
    EXPECT_EQ(r[2].width, 3);
    EXPECT_EQ(r[2].height, 2);
}

//      +-----------------+                      +-----------------+
//      |                 |                      |       (0)       |
//      |   (a)       +-------+      =>          +-------------+---+
//      |             |       |                  |       (1)   |
//      +-------------|  (b)  |                  +-------------+
//                    |       |
//                    +-------+
TEST(TigRectTest, ClipBottomLeft)
{
    TigRect a = { 10, 10, 10, 10 };
    TigRect b = { 18, 18, 5, 4 };
    TigRect r[4];

    EXPECT_EQ(tig_rect_clip(&a, &b, r), 2);

    EXPECT_EQ(r[0].x, 10);
    EXPECT_EQ(r[0].y, 10);
    EXPECT_EQ(r[0].width, 10);
    EXPECT_EQ(r[0].height, 8);

    EXPECT_EQ(r[1].x, 10);
    EXPECT_EQ(r[1].y, 18);
    EXPECT_EQ(r[1].width, 8);
    EXPECT_EQ(r[1].height, 2);
}

// -----------------------------------------------------------------------------
// LINE TESTS
// -----------------------------------------------------------------------------

// The following tests check behaviour of `tig_line_intersection` function in
// various configurations. Some cases give strange results. All expected values
// were taken by running the function in the original binary.

//          + (1)
//          |
//      +---|-------------+
//      |   |             |
//      |   + (2)         |
//      |                 |
//      +-----------------+
TEST(TigLineTest, LineIntersectionVerticalFromTop)
{
    TigRect a = { 3, 7, 10, 10 };
    TigLine b = { 5, 3, 5, 15 };

    EXPECT_EQ(tig_line_intersection(&a, &b), TIG_OK);
    EXPECT_EQ(b.x1, 5);
    EXPECT_EQ(b.y1, 7);
    EXPECT_EQ(b.x2, 5);
    EXPECT_EQ(b.y2, 15);
}

//      +-----------------+
//      |                 |
//      |   + (2)         |
//      |   |             |
//      +---|-------------+
//          |
//          + (1)
TEST(TigLineTest, LineIntersectionVerticalFromBottom)
{
    TigRect a = { 3, 7, 10, 10 };
    TigLine b = { 5, 19, 5, 9 };

    EXPECT_EQ(tig_line_intersection(&a, &b), TIG_OK);
    EXPECT_EQ(b.x1, 5);
    EXPECT_EQ(b.y1, 17);
    EXPECT_EQ(b.x2, 5);
    EXPECT_EQ(b.y2, 9);
}

// FAIL DUE TO IMPLEMENTATION
//          + (1)
//          |
//      +---|-------------+
//      |   |             |
//      |   |             |
//      |   |             |
//      +---|-------------+
//          |
//          + (2)
TEST(TigLineTest, LineIntersectionVerticalLineThru)
{
    TigRect a = { 3, 7, 10, 10 };
    TigLine b = { 5, 3, 5, 19 };

    EXPECT_EQ(tig_line_intersection(&a, &b), TIG_ERR_NO_INTERSECTION);
    EXPECT_EQ(b.x1, 5);
    EXPECT_EQ(b.y1, 3);
    EXPECT_EQ(b.x2, 5);
    EXPECT_EQ(b.y2, 19);
}

//      +-----------------+
// (1)  |  (2)            |
//  +-------+             |
//      |                 |
//      +-----------------+
TEST(TigLineTest, LineIntersectionHorizontalFromLeft)
{
    TigRect a = { 3, 7, 10, 10 };
    TigLine b = { 0, 10, 9, 10 };

    EXPECT_EQ(tig_line_intersection(&a, &b), TIG_OK);
    EXPECT_EQ(b.x1, 3);
    EXPECT_EQ(b.y1, 10);
    EXPECT_EQ(b.x2, 9);
    EXPECT_EQ(b.y2, 10);
}

//      +-----------------+
//      |            (2)  |  (1)
//      |             +-------+
//      |                 |
//      +-----------------+
TEST(TigLineTest, LineIntersectionHorizontalFromRight)
{
    TigRect a = { 3, 7, 10, 10 };
    TigLine b = { 15, 9, 7, 9 };

    EXPECT_EQ(tig_line_intersection(&a, &b), TIG_OK);
    EXPECT_EQ(b.x1, 13);
    EXPECT_EQ(b.y1, 9);
    EXPECT_EQ(b.x2, 7);
    EXPECT_EQ(b.y2, 9);
}

// FAIL DUE TO IMPLEMENTATION
//      +-----------------+
// (1)  |                 |  (2)
//  +-------------------------+
//      |                 |
//      +-----------------+
TEST(TigLineTest, LineIntersectionHorizontalLineThru)
{
    TigRect a = { 3, 7, 10, 10 };
    TigLine b = { 0, 10, 15, 10 };

    EXPECT_EQ(tig_line_intersection(&a, &b), TIG_ERR_NO_INTERSECTION);
    EXPECT_EQ(b.x1, 0);
    EXPECT_EQ(b.y1, 10);
    EXPECT_EQ(b.x2, 15);
    EXPECT_EQ(b.y2, 10);
}

TEST(TigLineTest, LineBoundingBoxNegative)
{
    TigLine a;
    TigRect b;

    a = { 13, 17, 3, 7 };
    EXPECT_EQ(tig_line_bounding_box(&a, &b), TIG_OK);
    EXPECT_EQ(b.x, 3);
    EXPECT_EQ(b.y, 7);
    EXPECT_EQ(b.width, 10);
    EXPECT_EQ(b.height, 10);
}

TEST(TigLineTest, LineBoundingBoxPositive)
{
    TigLine a;
    TigRect b;

    a = { 3, 7, 13, 17 };
    EXPECT_EQ(tig_line_bounding_box(&a, &b), TIG_OK);
    EXPECT_EQ(b.x, 3);
    EXPECT_EQ(b.y, 7);
    EXPECT_EQ(b.width, 10);
    EXPECT_EQ(b.height, 10);
}
