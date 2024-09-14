#include "tig/art.h"

#include <gtest/gtest.h>

TEST(TigArtIdTest, MiscIdCreate)
{
    tig_art_id_t art_id;

    EXPECT_EQ(tig_art_misc_id_create(10, 0, &art_id), TIG_OK);
    EXPECT_EQ(art_id, 0x80500000);
}

TEST(TigArtIdTest, TileIdCreate)
{
    tig_art_id_t art_id;

    EXPECT_EQ(tig_art_tile_id_create(1, 1, 15, 0, 0, 1, 1, 0, &art_id), TIG_OK);
    EXPECT_EQ(art_id, 0x41F0C0);
}

TEST(TigArtIdTest, RoofIdCreate)
{
    tig_art_id_t art_id;

    EXPECT_EQ(tig_art_roof_id_create(15, 0, 0, 0, &art_id), TIG_OK);
    EXPECT_EQ(art_id, 0xA0780000);
}

TEST(TigArtIdTest, EyeCandyIdCreate)
{
    tig_art_id_t art_id;

    EXPECT_EQ(tig_art_eye_candy_id_create(27, 0, 0, 1, 0, 1, 4, &art_id), TIG_OK);
    EXPECT_EQ(art_id, 0xE0D80118);
}

TEST(TigArtIdTest, LightIdCreate)
{
    tig_art_id_t art_id;

    EXPECT_EQ(tig_art_light_id_create(6, 0, 0, 1, &art_id), TIG_OK);
    EXPECT_EQ(art_id, 0x90300001);
}
