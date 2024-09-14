#include "tig/palette.h"

#include <gtest/gtest.h>

#include "tig/color.h"
#include "tig/memory.h"

class TigPaletteTest : public testing::Test {
protected:
    void SetUp() override
    {
        TigInitInfo init_info = { 0 };
        init_info.bpp = 16;

        ASSERT_EQ(tig_color_init(&init_info), TIG_OK);
        ASSERT_EQ(tig_palette_init(&init_info), TIG_OK);

        ASSERT_EQ(tig_color_set_rgb_settings(0xF800, 0x7E0, 0x1F), TIG_OK);
    }

    void TearDown() override
    {
        tig_palette_exit();
        tig_color_exit();
        ASSERT_TRUE(tig_memory_validate_memory_leaks());
    }
};

TEST_F(TigPaletteTest, SystemMemorySize)
{
    ASSERT_EQ(tig_palette_system_memory_size(), 516);
}

TEST_F(TigPaletteTest, CreateDestroy)
{
    TigPalette palette = tig_palette_create();
    ASSERT_NE(palette, nullptr);
    tig_palette_destroy(palette);
}

TEST_F(TigPaletteTest, SetColor)
{
    TigPalette palette = tig_palette_create();
    ASSERT_NE(palette, nullptr);
    tig_palette_fill(palette, tig_color_make(64, 128, 192));
    for (int index = 0; index < 256; index++) {
        ASSERT_EQ(((uint16_t*)palette)[index], tig_color_make(64, 128, 192));
    }
    tig_palette_destroy(palette);
}

TEST_F(TigPaletteTest, ModifyGrayscale)
{
    TigPalette src_palette = tig_palette_create();
    ASSERT_NE(src_palette, nullptr);
    tig_palette_fill(src_palette, tig_color_make(192, 64, 128));

    TigPalette dst_palette = tig_palette_create();
    ASSERT_NE(dst_palette, nullptr);

    TigPaletteModifyInfo modify_info = { 0 };
    modify_info.flags = TIG_PALETTE_MODIFY_GRAYSCALE;
    modify_info.src_palette = src_palette;
    modify_info.dst_palette = dst_palette;
    tig_palette_modify(&modify_info);

    tig_color_t gray = tig_color_rgb_to_grayscale(tig_color_make(192, 64, 128));
    for (int index = 0; index < 256; index++) {
        ASSERT_EQ(((uint16_t*)dst_palette)[index], gray);
    }

    tig_palette_destroy(src_palette);
    tig_palette_destroy(dst_palette);
}

TEST_F(TigPaletteTest, ModifyTint)
{
    TigPalette src_palette = tig_palette_create();
    ASSERT_NE(src_palette, nullptr);
    tig_palette_fill(src_palette, tig_color_make(192, 64, 128));

    TigPalette dst_palette = tig_palette_create();

    TigPaletteModifyInfo modify_info = { 0 };
    modify_info.flags = TIG_PALETTE_MODIFY_TINT;
    modify_info.tint_color = tig_color_make(64, 192, 128);
    modify_info.src_palette = src_palette;
    modify_info.dst_palette = dst_palette;
    tig_palette_modify(&modify_info);

    tig_color_t mult = tig_color_mul(tig_color_make(192, 64, 128), tig_color_make(64, 192, 128));
    for (int index = 0; index < 256; index++) {
        ASSERT_EQ(((uint16_t*)dst_palette)[index], mult);
    }

    tig_palette_destroy(src_palette);
    tig_palette_destroy(dst_palette);
}
