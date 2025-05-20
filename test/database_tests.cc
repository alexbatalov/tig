#include "tig/database.h"

#include <gtest/gtest.h>

#include "tig/memory.h"

class TigDatabaseTest : public testing::Test {
protected:
    void SetUp() override
    {
        arcanum_dir = getenv("ARCANUM_DIR");
        if (arcanum_dir == nullptr) {
            GTEST_SKIP() << "'ARCANUM_DIR' is not set, skipping TigDatabaseTest";
            return;
        }

        ASSERT_EQ(tig_memory_init(nullptr), 0);
    }

    void TearDown() override
    {
        ASSERT_TRUE(tig_memory_validate_memory_leaks());
        tig_memory_exit();
    }

    const char* build_path(const char* file_name)
    {
        static char path[TIG_MAX_PATH];
        sprintf(path, "%s\\%s", arcanum_dir, file_name);
        return path;
    }

protected:
    const char* arcanum_dir;
};

TEST_F(TigDatabaseTest, OpenClose)
{
    TigDatabase* database;

    database = tig_database_open(build_path("tig.dat"));
    ASSERT_NE(database, nullptr);
    EXPECT_EQ(database->entries_count, 28);

    const TigGuid expected_guid = { 0x47CEC38A, 0x838C, 0xB1F9, { 0x00, 0xC3, 0x0E, 0x61, 0x31, 0x54, 0x41, 0x44 } };
    EXPECT_TRUE(tig_guid_is_equal(&(database->guid), &(expected_guid)));

    const struct {
        const char* file_name;
        unsigned int flags;
        int size;
        int compressed_size;
    } expected_entries[] = {
        { "art", 0x500, 0, 0 },
        { "art\\blank.ART", 0x102, 1410, 1039 },
        { "art\\button.ART", 0x102, 2089, 485 },
        { "art\\cancel.ART", 0x102, 1596, 901 },
        { "art\\down.ART", 0x102, 1484, 839 },
        { "art\\grey_tile.ART", 0x102, 1344, 187 },
        { "art\\grey_tile2.ART", 0x102, 2332, 610 },
        { "art\\lens.ART", 0x102, 1370, 796 },
        { "art\\minus.ART", 0x102, 1419, 1074 },
        { "art\\morph15font.ART", 0x102, 24934, 7429 },
        { "art\\mouse.ART", 0x102, 3174, 644 },
        { "art\\node1.ART", 0x102, 1210, 138 },
        { "art\\pat_inv.ART", 0x102, 1346, 198 },
        { "art\\pat_inv2.ART", 0x102, 2342, 622 },
        { "art\\pat_inv2.bmp", 0x102, 52278, 555 },
        { "art\\pattern.bmp", 0x102, 4278, 186 },
        { "art\\plus.ART", 0x102, 1499, 1113 },
        { "art\\TileStamp.ART", 0x102, 1346, 189 },
        { "art\\TileStamp.bmp", 0x102, 4278, 179 },
        { "art\\up.ART", 0x102, 1484, 843 },
        { "art\\vssver.scc", 0x102, 352, 278 },
        { "art\\x.ART", 0x102, 1554, 1174 },
        { "sound", 0x500, 0, 0 },
        { "sound\\hover.snd", 0x102, 7038, 2850 },
        { "sound\\press.snd", 0x102, 11884, 6330 },
        { "sound\\release.snd", 0x102, 26884, 16077 },
        { "sound\\sound.snd", 0x102, 20202, 11205 },
        { "sound\\vssver.scc", 0x102, 96, 87 },
    };
    for (unsigned int index = 0; index < database->entries_count; index++) {
        EXPECT_STREQ(database->entries[index].path, expected_entries[index].file_name) << "at index: " << index;
        EXPECT_EQ(database->entries[index].flags, expected_entries[index].flags) << "at index: " << index;
        EXPECT_EQ(database->entries[index].size, expected_entries[index].size) << "at index: " << index;
        EXPECT_EQ(database->entries[index].compressed_size, expected_entries[index].compressed_size) << "at index: " << index;
    }

    tig_database_close(database);
}

TEST_F(TigDatabaseTest, FindFile)
{
    TigDatabase* database;
    TigDatabaseFindFileData ffd;

    database = tig_database_open(build_path("tig.dat"));
    ASSERT_NE(database, nullptr);

    EXPECT_FALSE(tig_database_find_first_entry(database, "not_found", &ffd));
    EXPECT_GE(ffd.index, 28);
    tig_database_find_close(&ffd);

    EXPECT_TRUE(tig_database_find_first_entry(database, "art\\pattern.bmp", &ffd));
    EXPECT_STREQ(ffd.name, "art\\pattern.bmp");
    EXPECT_FALSE(tig_database_find_next_entry(&ffd));
    tig_database_find_close(&ffd);

    EXPECT_TRUE(tig_database_find_first_entry(database, "art\\pat_*.*", &ffd));
    EXPECT_STREQ(ffd.name, "art\\pat_inv.ART");
    EXPECT_TRUE(tig_database_find_next_entry(&ffd));
    EXPECT_STREQ(ffd.name, "art\\pat_inv2.ART");
    EXPECT_TRUE(tig_database_find_next_entry(&ffd));
    EXPECT_STREQ(ffd.name, "art\\pat_inv2.bmp");
    EXPECT_FALSE(tig_database_find_next_entry(&ffd));
    tig_database_find_close(&ffd);

    // Database does not support nested paths lookup.
    EXPECT_FALSE(tig_database_find_first_entry(database, "*.snd", &ffd));
    tig_database_find_close(&ffd);

    EXPECT_TRUE(tig_database_find_first_entry(database, "sound\\*.snd", &ffd));
    EXPECT_STREQ(ffd.name, "sound\\hover.snd");
    EXPECT_TRUE(tig_database_find_next_entry(&ffd));
    EXPECT_STREQ(ffd.name, "sound\\press.snd");
    EXPECT_TRUE(tig_database_find_next_entry(&ffd));
    EXPECT_STREQ(ffd.name, "sound\\release.snd");
    EXPECT_TRUE(tig_database_find_next_entry(&ffd));
    EXPECT_STREQ(ffd.name, "sound\\sound.snd");
    EXPECT_FALSE(tig_database_find_next_entry(&ffd));
    tig_database_find_close(&ffd);

    tig_database_close(database);
}
