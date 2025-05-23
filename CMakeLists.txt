cmake_minimum_required(VERSION 3.13)

project(tig)

option(TIG_BUILD_TESTS "Build tests." ON)
option(TIG_BUILD_EXAMPLE "Build example." ON)

# ------------------------------------------------------------------------------
# DEPENDENCIES
# ------------------------------------------------------------------------------

add_subdirectory("third_party/fpattern" EXCLUDE_FROM_ALL)
add_subdirectory("third_party/sdl3" EXCLUDE_FROM_ALL)
add_subdirectory("third_party/zlib" EXCLUDE_FROM_ALL)

if(TIG_BUILD_TESTS)
    add_subdirectory("third_party/googletest" EXCLUDE_FROM_ALL)
endif()

add_subdirectory("first_party/bink_compat" EXCLUDE_FROM_ALL)
add_subdirectory("first_party/mss_compat" EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
# PROJECT FILES
# ------------------------------------------------------------------------------

# Keep alphabetically sorted.
set(HDRS
    "include/tig/art.h"
    "include/tig/bmp_utils.h"
    "include/tig/bmp.h"
    "include/tig/bsearch.h"
    "include/tig/button.h"
    "include/tig/color.h"
    "include/tig/core.h"
    "include/tig/database.h"
    "include/tig/debug.h"
    "include/tig/draw.h"
    "include/tig/file_cache.h"
    "include/tig/file.h"
    "include/tig/find_file.h"
    "include/tig/font.h"
    "include/tig/guid.h"
    "include/tig/idxtable.h"
    "include/tig/kb.h"
    "include/tig/memory.h"
    "include/tig/menu.h"
    "include/tig/message.h"
    "include/tig/mouse.h"
    "include/tig/movie.h"
    "include/tig/palette.h"
    "include/tig/rect.h"
    "include/tig/sound.h"
    "include/tig/str_parse.h"
    "include/tig/tig.h"
    "include/tig/timer.h"
    "include/tig/video.h"
    "include/tig/window.h"
)

# Keep alphabetically sorted.
set(SRCS
    "src/art.c"
    "src/bmp_utils.c"
    "src/bmp.c"
    "src/bsearch.c"
    "src/button.c"
    "src/color.c"
    "src/core.c"
    "src/database.c"
    "src/debug.c"
    "src/draw.c"
    "src/file_cache.c"
    "src/file.c"
    "src/find_file.c"
    "src/font.c"
    "src/guid.c"
    "src/idxtable.c"
    "src/kb.c"
    "src/memory.c"
    "src/menu.c"
    "src/message.c"
    "src/mouse.c"
    "src/movie.c"
    "src/palette.c"
    "src/rect.c"
    "src/sound.c"
    "src/str_parse.c"
    "src/timer.c"
    "src/video.c"
    "src/window.c"
)

# ------------------------------------------------------------------------------
# COMMON LIBRARY
# ------------------------------------------------------------------------------

add_library(tig_common INTERFACE)

target_include_directories(tig_common INTERFACE
    ${ZLIB_INCLUDE_DIRS}
    ${BINK_COMPAT_INCLUDE_DIR}
    ${MSS_COMPAT_INCLUDE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(tig_common INTERFACE
    fpattern::fpattern
    SDL3::SDL3
    ${ZLIB_LIBRARIES}
    ${BINK_COMPAT_LIBRARY}
    ${MSS_COMPAT_LIBRARY}
)

if(WIN32)
    target_compile_definitions(tig_common INTERFACE
        _CRT_NONSTDC_NO_WARNINGS
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# ------------------------------------------------------------------------------
# TIG
# ------------------------------------------------------------------------------

add_library(tig STATIC
    ${HDRS}
    ${SRCS}
)

# Derive settings from common library.
target_link_libraries(tig PUBLIC tig_common)

# Turn on strict mode (but only for the library itself, hence PRIVATE).
if(MSVC)
    target_compile_options(tig PRIVATE /W4)
else()
    target_compile_options(tig PRIVATE -Wall -Wextra)
endif()

# ------------------------------------------------------------------------------
# TIG TESTS
# ------------------------------------------------------------------------------

if(TIG_BUILD_TESTS)
    enable_testing()

    # Keep alphabetically sorted.
    set(TSTS
        "test/art_tests.cc"
        "test/bmp_utils_tests.cc"
        "test/bsearch_tests.cc"
        "test/color_tests.cc"
        "test/database_tests.cc"
        "test/idxtable_tests.cc"
        "test/memory_tests.cc"
        "test/palette_tests.cc"
        "test/rect_tests.cc"
        "test/str_parse_tests.cc"
    )

    add_executable(tig_tests
        ${TSTS}
    )

    target_link_libraries(tig_tests PUBLIC tig)

    # Let GoogleTest provide synthesized `main`.
    target_link_libraries(tig_tests PUBLIC GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(tig_tests)
endif()

# ------------------------------------------------------------------------------
# EXAMPLE
# ------------------------------------------------------------------------------

if(TIG_BUILD_EXAMPLE)
    add_executable(example WIN32
        "example/main.c"
    )

    target_link_libraries(example PUBLIC tig)
endif()
