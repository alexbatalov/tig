#include "tig/bmp.h"

#include "tig/color.h"
#include "tig/file.h"
#include "tig/memory.h"
#include "tig/video.h"
#include "tig/window.h"

// 0x535D80
int tig_bmp_create(TigBmp* bmp)
{
    TigFile* stream;
    BITMAPFILEHEADER file_hdr;
    BITMAPINFOHEADER info_hdr;
    int num_colors;
    int data_size;
    int x;
    int y;
    uint8_t byte;
    RGBQUAD palette[256];
    int index;

    if (bmp == NULL) {
        return TIG_ERR_INVALID_PARAM;
    }

    bmp->pixels = NULL;

    stream = tig_file_fopen(bmp->name, "rb");
    if (stream == NULL) {
        return TIG_ERR_IO;
    }

    static_assert(sizeof(file_hdr) == 0xE, "wrong size");
    if (tig_file_fread(&file_hdr, sizeof(file_hdr), 1, stream) != 1) {
        tig_file_fclose(stream);
        return TIG_ERR_IO;
    }

    if (file_hdr.bfType != 0x4D42) {
        tig_file_fclose(stream);
        return TIG_ERR_16;
    }

    static_assert(sizeof(info_hdr) == 0x28, "wrong size");
    if (tig_file_fread(&info_hdr, sizeof(info_hdr), 1, stream) != 1) {
        tig_file_fclose(stream);
        return TIG_ERR_IO;
    }

    if (info_hdr.biSize != sizeof(info_hdr)
        || info_hdr.biPlanes != 1
        || (info_hdr.biBitCount != 4
            && info_hdr.biBitCount != 8
            && info_hdr.biBitCount != 24)
        || info_hdr.biCompression != BI_RGB) {
        tig_file_fclose(stream);
        return TIG_ERR_16;
    }

    bmp->bpp = info_hdr.biBitCount;

    num_colors = info_hdr.biClrUsed;
    if (num_colors == 0) {
        switch (info_hdr.biBitCount) {
        case 1:
            num_colors = 2;
            break;
        case 4:
            num_colors = 16;
            break;
        case 8:
            num_colors = 256;
            break;
        case 24:
            num_colors = 0;
            break;
        default:
            tig_file_fclose(stream);
            return TIG_ERR_16;
        }
    }

    if (num_colors > 0) {
        if (info_hdr.biBitCount <= 8) {
            if (tig_file_fread(palette, sizeof(RGBQUAD), num_colors, stream) != num_colors) {
                tig_file_fclose(stream);
                return TIG_ERR_IO;
            }
        }

        for (index = 0; index < num_colors; ++index) {
            bmp->palette[index] = tig_color_to_24_bpp(palette[index].rgbRed,
                palette[index].rgbGreen,
                palette[index].rgbBlue);
        }
    }

    bmp->width = info_hdr.biWidth;
    bmp->height = info_hdr.biHeight;

    if (info_hdr.biSizeImage != 0) {
        bmp->pitch = info_hdr.biSizeImage / info_hdr.biHeight;
    } else {
        bmp->pitch = info_hdr.biWidth;
        if (bmp->pitch % 4 > 0) {
            bmp->pitch = bmp->pitch - bmp->pitch % 4 + 4;
        }
    }

    data_size = bmp->pitch * bmp->height;
    bmp->pixels = MALLOC(data_size);
    if (bmp->pixels == NULL) {
        tig_file_fclose(stream);
        return TIG_ERR_OUT_OF_MEMORY;
    }

    if (tig_file_fread(bmp->pixels, data_size, 1, stream) != 1) {
        tig_file_fclose(stream);
        return TIG_ERR_IO;
    }

    for (y = 0; y < bmp->height / 2; ++y) {
        for (x = 0; x < bmp->width; ++x) {
            byte = ((uint8_t*)bmp->pixels)[bmp->pitch * y + x];
            ((uint8_t*)bmp->pixels)[bmp->pitch * y + x] = ((uint8_t*)bmp->pixels)[bmp->pitch * (bmp->height - y - 1) + x];
            ((uint8_t*)bmp->pixels)[bmp->pitch * (bmp->height - y - 1) + x] = byte;
        }
    }

    tig_file_fclose(stream);
    return TIG_OK;
}

// 0x5360D0
int tig_bmp_destroy(TigBmp* bmp)
{
    if (bmp->pixels != NULL) {
        FREE(bmp->pixels);
    }

    return TIG_OK;
}

// 0x5368B0
int tig_bmp_copy_to_video_buffer(TigBmp* bmp, const TigRect* src_rect, TigVideoBuffer* video_buffer, const TigRect* dst_rect)
{
    int rc;
    TigVideoBufferData video_buffer_data;
    bool stretched;
    TigRect bounds;
    TigRect blit_src_rect;
    TigRect blit_dst_rect;
    TigRect tmp_rect;
    float width_ratio;
    float height_ratio;
    uint8_t* src_pixels;
    float src_step_x;
    float src_step_y;

    if (src_rect->x < 0
        || src_rect->y < 0
        || src_rect->x + src_rect->width > bmp->width
        || src_rect->y + src_rect->height > bmp->height) {
        return TIG_ERR_INVALID_PARAM;
    }

    rc = tig_video_buffer_lock(video_buffer);
    if (rc != TIG_OK) {
        return rc;
    }

    rc = tig_video_buffer_data(video_buffer, &video_buffer_data);
    if (rc != TIG_OK) {
        tig_video_buffer_unlock(video_buffer);
        return rc;
    }

    if (dst_rect->x < 0
        || dst_rect->y < 0
        || dst_rect->x + dst_rect->width > video_buffer_data.width
        || dst_rect->y + dst_rect->height > video_buffer_data.height) {
        tig_video_buffer_unlock(video_buffer);
        return TIG_ERR_INVALID_PARAM;
    }

    if (src_rect->width == dst_rect->width && src_rect->height == dst_rect->height) {
        stretched = false;
        width_ratio = 1.0f;
        height_ratio = 1.0f;
    } else {
        stretched = true;
        width_ratio = (float)src_rect->width / (float)dst_rect->width;
        height_ratio = (float)src_rect->height / (float)dst_rect->height;
    }

    bounds.x = 0;
    bounds.y = 0;
    bounds.width = bmp->width;
    bounds.height = bmp->height;

    if (tig_rect_intersection(src_rect, &bounds, &blit_src_rect) != TIG_OK) {
        tig_video_buffer_unlock(video_buffer);
        return TIG_OK;
    }

    tmp_rect = *dst_rect;

    if (stretched) {
        tmp_rect.x += (int)((float)(blit_src_rect.x - src_rect->x) / width_ratio);
        tmp_rect.y += (int)((float)(blit_src_rect.y - src_rect->y) / height_ratio);
        tmp_rect.width -= (int)((float)(src_rect->width - blit_src_rect.width) / width_ratio);
        tmp_rect.height -= (int)((float)(src_rect->height - blit_src_rect.height) / height_ratio);
    } else {
        tmp_rect.x += blit_src_rect.x - src_rect->x;
        tmp_rect.y += blit_src_rect.y - src_rect->y;
        tmp_rect.width -= src_rect->width - blit_src_rect.width;
        tmp_rect.height -= src_rect->height - blit_src_rect.height;
    }

    bounds.x = 0;
    bounds.y = 0;
    bounds.width = video_buffer_data.width;
    bounds.height = video_buffer_data.height;

    if (tig_rect_intersection(&tmp_rect, &bounds, &blit_dst_rect) != TIG_OK) {
        tig_video_buffer_unlock(video_buffer);
        return TIG_OK;
    }

    if (stretched) {
        blit_src_rect.x += (int)((float)(blit_dst_rect.x - tmp_rect.x) / width_ratio);
        blit_src_rect.y += (int)((float)(blit_dst_rect.y - tmp_rect.y) / height_ratio);
        blit_src_rect.width -= (int)((float)(tmp_rect.width - blit_dst_rect.width) / width_ratio);
        blit_src_rect.height -= (int)((float)(tmp_rect.height - blit_dst_rect.height) / height_ratio);
    } else {
        blit_src_rect.x += blit_dst_rect.x - tmp_rect.x;
        blit_src_rect.y += blit_dst_rect.y - tmp_rect.y;
        blit_src_rect.width -= tmp_rect.width - blit_dst_rect.width;
        blit_src_rect.height -= tmp_rect.height - blit_dst_rect.height;
    }

    src_pixels = (uint8_t*)bmp->pixels + blit_src_rect.y * bmp->pitch + blit_src_rect.x;
    src_step_x = (float)blit_src_rect.width / (float)blit_dst_rect.width;
    src_step_y = (float)blit_src_rect.height / (float)blit_dst_rect.height;

    if (bmp->bpp == 8) {
        switch (video_buffer_data.bpp) {
        case 8:
            // TODO: Incomplete.
            break;
        case 16:
            // TODO: Incomplete.
            break;
        case 24:
            // TODO: Incomplete.
            break;
        case 32: {
            uint32_t* palette = (uint32_t*)MALLOC(sizeof(*palette) * 256);
            for (int idx = 0; idx < 256; idx++) {
                palette[idx] = tig_color_index_of(bmp->palette[idx]);
            }

            uint32_t* dst = (uint32_t*)video_buffer_data.surface_data.pixels + blit_dst_rect.y * (video_buffer_data.pitch / 4) + blit_dst_rect.x;
            int skip = video_buffer_data.pitch / 4 - blit_dst_rect.width;

            float src_y = 0.0f;
            for (int y = 0; y < blit_dst_rect.height; y++) {
                float src_x = 0.0f;
                for (int x = 0; x < blit_dst_rect.width; x++) {
                    *dst++ = palette[src_pixels[bmp->pitch * (int)src_y + (int)src_x]];
                    src_x += src_step_x;
                }

                dst += skip;
                src_y += src_step_y;
            }

            FREE(palette);
            break;
        }
        default:
            break;
        }
    } else {
        // NOTE: This codepath looks totally broken. Instead of copying bmp to
        // video buffer, it copies video buffer to bmp, and it does so without
        // respecting src/dst rects, which leads to memory corruption.
        if (bmp->bpp == video_buffer_data.bpp) {
            switch (bmp->bpp) {
            case 8:
                for (int y = 0; y < bmp->height; y++) {
                    for (int x = 0; x < bmp->width; x++) {
                        *((uint8_t*)bmp->pixels + bmp->pitch * y + x) = *((uint8_t*)video_buffer_data.surface_data.pixels + video_buffer_data.pitch * y + x);
                    }
                }
                break;
            case 16:
                for (int y = 0; y < bmp->height; y++) {
                    for (int x = 0; x < bmp->width; x++) {
                        *((uint16_t*)bmp->pixels + bmp->pitch * y + x) = *((uint16_t*)video_buffer_data.surface_data.pixels + video_buffer_data.pitch * y + x);
                    }
                }
                break;
            case 24:
                for (int y = 0; y < bmp->height; y++) {
                    for (int x = 0; x < bmp->width; x++) {
                        *((uint32_t*)bmp->pixels + bmp->pitch * y + x) = *((uint32_t*)video_buffer_data.surface_data.pixels + video_buffer_data.pitch * y + x);
                    }
                }
                break;
            case 32:
                for (int y = 0; y < bmp->height; y++) {
                    for (int x = 0; x < bmp->width; x++) {
                        *((uint32_t*)bmp->pixels + bmp->pitch * y + x) = *((uint32_t*)video_buffer_data.surface_data.pixels + video_buffer_data.pitch * y + x);
                    }
                }
                break;
            }
        } else if (bmp->bpp == 24 && video_buffer_data.bpp == 16) {
            for (int y = 0; y < bmp->height; y++) {
                for (int x = 0; x < bmp->width; x++) {
                    *((uint16_t*)video_buffer_data.surface_data.pixels + video_buffer_data.pitch * y + x) = (uint16_t)tig_color_index_of(*((uint16_t*)bmp->pixels + bmp->pitch * y + x));
                }
            }
        }
    }

    tig_video_buffer_unlock(video_buffer);

    return TIG_OK;
}

// 0x537520
int tig_bmp_create_from_video_buffer(TigVideoBuffer* video_buffer, int a2, TigBmp* bmp)
{
    // NOTE: This param is not used and this function is never called. It's
    // impossible to guess its meaning.
    (void)a2;

    TigVideoBufferData video_buffer_data;
    int x;
    int y;
    int rc = TIG_ERR_16;

    if (tig_video_buffer_lock(video_buffer) != TIG_OK) {
        return TIG_ERR_7;
    }

    if (tig_video_buffer_data(video_buffer, &video_buffer_data) == TIG_OK) {
        bmp->name[0] = '\0';
        if (tig_video_get_bpp(&(bmp->bpp)) == TIG_OK
            && tig_video_get_palette(bmp->palette) == TIG_OK) {
            bmp->pitch = video_buffer_data.pitch;
            bmp->width = video_buffer_data.width;
            bmp->height = video_buffer_data.height;
            bmp->pixels = MALLOC(video_buffer_data.height * video_buffer_data.pitch);
            if (bmp->pixels != NULL) {
                switch (bmp->bpp) {
                case 8:
                    for (y = 0; y < bmp->height; ++y) {
                        for (x = 0; x < bmp->width; ++x) {
                            *((uint8_t*)bmp->pixels + bmp->pitch * y + x) = *((uint8_t*)video_buffer_data.surface_data.pixels + video_buffer_data.pitch * y + x);
                        }
                    }
                    break;
                case 16:
                    for (y = 0; y < bmp->height; ++y) {
                        for (x = 0; x < bmp->width; ++x) {
                            *((uint16_t*)bmp->pixels + bmp->pitch * y + x) = *((uint16_t*)video_buffer_data.surface_data.pixels + video_buffer_data.pitch * y + x);
                        }
                    }
                    break;
                case 24:
                    for (y = 0; y < bmp->height; ++y) {
                        for (x = 0; x < bmp->width; ++x) {
                            *((uint32_t*)bmp->pixels + bmp->pitch * y + x) = *((uint32_t*)video_buffer_data.surface_data.pixels + video_buffer_data.pitch * y + x);
                        }
                    }
                    break;
                case 32:
                    for (y = 0; y < bmp->height; ++y) {
                        for (x = 0; x < bmp->width; ++x) {
                            *((uint32_t*)bmp->pixels + bmp->pitch * y + x) = *((uint32_t*)video_buffer_data.surface_data.pixels + video_buffer_data.pitch * y + x);
                        }
                    }
                    break;
                }
            }
        }
    }

    if (tig_video_buffer_unlock(video_buffer) != TIG_OK) {
        return TIG_ERR_7;
    }

    return rc;
}

// 0x5377A0
int tig_bmp_copy_to_bmp(TigBmp* src, TigBmp* dst)
{
    float width_ratio;
    float height_ratio;
    int x;
    int y;

    if (src->bpp != 8) {
        return TIG_ERR_INVALID_PARAM;
    }

    if (dst->width <= 0) {
        dst->width = src->width;
    }

    if (dst->height <= 0) {
        dst->height = src->height;
    }

    dst->pitch = src->width;
    if ((dst->width % 4) > 0) {
        dst->pitch = dst->pitch - dst->pitch % 4 + 4;
    }

    dst->bpp = 8;
    memcpy(dst->palette, src->palette, sizeof(src->palette));

    dst->pixels = MALLOC(dst->pitch * dst->height);

    width_ratio = (float)src->width / (float)dst->width;
    height_ratio = (float)src->height / (float)dst->height;

    for (y = 0; y < dst->height; ++y) {
        for (x = 0; x < dst->width; ++x) {
            ((uint8_t*)dst->pixels)[dst->pitch * y + x] = ((uint8_t*)src->pixels)[src->pitch * (int)(y * height_ratio) + (int)(x * width_ratio)];
        }
    }

    return TIG_OK;
}

// 0x537900
int tig_bmp_copy_to_window(TigBmp* bmp, const TigRect* src_rect, tig_window_handle_t window_handle, const TigRect* dst_rect)
{
    TigVideoBuffer* video_buffer;

    if (tig_window_vbid_get(window_handle, &video_buffer)) {
        return TIG_ERR_16;
    }

    return tig_bmp_copy_to_video_buffer(bmp, src_rect, video_buffer, dst_rect);
}
