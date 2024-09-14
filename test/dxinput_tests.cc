#include "tig/dxinput.h"

#include <gtest/gtest.h>

class DxInputTest : public testing::Test {
protected:
    void SetUp() override
    {
        TigInitInfo init_info = { 0 };
        init_info.instance = GetModuleHandle(nullptr);

        ASSERT_EQ(tig_dxinput_init(&init_info), TIG_OK);
    }

    void TearDown() override
    {
        tig_dxinput_exit();
    }
};

TEST_F(DxInputTest, GetInstance)
{
    LPDIRECTINPUTA di;
    ASSERT_EQ(tig_dxinput_get_instance(&di), TIG_OK);
    ASSERT_NE(di, nullptr);
}
