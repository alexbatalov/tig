#include "tig/net_xfer.h"

#include <stdio.h>

#include "tig/debug.h"
#include "tig/file.h"
#include "tig/memory.h"
#include "tig/net.h"

#define TIG_NET_XFER_MAX_SLOTS 100

#define TIG_NET_XFER_ACTIVE 0x01
#define TIG_NET_XFER_RECEIVER 0x02

#define TIG_NET_XFER_INFO_TYPE_START 0
#define TIG_NET_XFER_INFO_TYPE_END 1
#define TIG_NET_XFER_INFO_TYPE_ACK 3

typedef struct TigNetXferDataPacket {
    int32_t xfer_id;
    int32_t size;
    uint8_t data[2048];
} TigNetXferDataPacket;

// See 0x534ED0.
static_assert(sizeof(TigNetXferDataPacket) == 0x808, "wrong size");

typedef struct TigNetXferInfoPacket {
    int32_t type;
    int32_t xfer_id;
    char path[TIG_MAX_PATH];
    int32_t size;
} TigNetXferInfoPacket;

// See 0x534ED0.
static_assert(sizeof(TigNetXferInfoPacket) == 0x110, "wrong size");

typedef struct TigNetXfer {
    int id;
    char* path;
    int client_id;
    unsigned int flags;
    TigFile* stream;
    int recieved_bytes;
    int total_bytes;
} TigNetXfer;

// See 0x535130.
static_assert(sizeof(TigNetXfer) == 0x1C, "wrong size");

// 0x630208
static TigNetXfer tig_net_xfers[TIG_NET_XFER_MAX_SLOTS];

// 0x630CF8
static uint16_t word_630CF8;

static void tig_net_xfer_reset(TigNetXfer* xfer);
static void tig_next_xfer_send_next_chunk(TigNetXfer* xfer);
static bool tig_net_xfer_get_free_slot(TigNetXfer** xfer_ptr);
static bool tig_net_xfer_find(int id, TigNetXfer** xfer_ptr);

// 0x5349A0
int tig_net_xfer_init()
{
    int index;

    tig_debug_printf("TIG: net_xfer: init %d MAX_SLOTS\n", TIG_NET_XFER_MAX_SLOTS);

    for (index = 0; index < TIG_NET_XFER_MAX_SLOTS; index++) {
        tig_net_xfer_reset(&(tig_net_xfers[index]));
    }

    tig_debug_printf("             : initialization complete\n");

    return TIG_OK;
}

// 0x5349E0
void tig_net_xfer_exit()
{
    int index;

    tig_debug_printf("TIG: net_xfer: exititing\n");

    for (index = 0; index < TIG_NET_XFER_MAX_SLOTS; index++) {
        if ((tig_net_xfers[index].flags & TIG_NET_XFER_ACTIVE) != 0) {
            tig_debug_printf("             : freeing slot %d\n", index);
            tig_file_fclose(tig_net_xfers[index].stream);
            FREE(tig_net_xfers[index].path);
        }
    }

    tig_debug_printf("             : exit_complete\n");
}

// 0x535130
void tig_net_xfer_reset(TigNetXfer* xfer)
{
    memset(xfer, 0, sizeof(*xfer));
}

// 0x534A40
void tig_net_xfer_ping()
{
}

// 0x534A50
int tig_net_xfer_send(const char* path, int client_id, int* xfer_id)
{
    return tig_net_xfer_send_as(path, path, client_id, xfer_id);
}

// 0x534A70
int tig_net_xfer_send_as(const char* path, const char* alias, int client_id, int* xfer_id)
{
    TigNetXfer* xfer;
    TigFileInfo file_info;
    TigNetXferInfoPacket info_pkt;

    if (strcmp(path + strlen(path) - 5, ".RECV") == 0) {
        return TIG_ERR_16;
    }

    if (!tig_net_xfer_get_free_slot(&xfer)) {
        return TIG_ERR_16;
    }

    if (!tig_file_exists(path, &file_info)) {
        return TIG_ERR_16;
    }

    xfer->stream = tig_file_fopen(path, "rb");
    if (xfer->stream == NULL) {
        return TIG_ERR_16;
    }

    xfer->flags = TIG_NET_XFER_ACTIVE;
    xfer->id = (word_630CF8 << 16) | sub_529520();
    word_630CF8++;

    if (xfer_id != NULL) {
        *xfer_id = xfer->id;
    }

    xfer->path = STRDUP(alias);
    xfer->client_id = client_id;
    xfer->flags = TIG_NET_XFER_ACTIVE;
    xfer->total_bytes = file_info.size;

    info_pkt.type = TIG_NET_XFER_INFO_TYPE_START;
    info_pkt.xfer_id = xfer->id;
    strncpy(info_pkt.path, alias, TIG_MAX_PATH);

    tig_debug_printf("TIG: net_xfer: sending ID=%08X, to=%d, file \"%s\" as \"%s\" size %d\n",
        xfer->id,
        xfer->client_id,
        path,
        alias,
        xfer->total_bytes);

    tig_net_send_generic(&info_pkt, sizeof(info_pkt), 11, client_id);

    return TIG_OK;
}

// 0x534C10
void tig_net_xfer_recv_data_pkt(void* pkt)
{
    TigNetXferDataPacket* data_pkt;
    TigNetXfer* xfer;
    TigNetXferInfoPacket info_pkt;

    data_pkt = (TigNetXferDataPacket*)pkt;
    if (!tig_net_xfer_find(data_pkt->xfer_id, &xfer)) {
        tig_debug_printf("TIG: net_xfer: could not find id 0x%08X.\n", data_pkt->xfer_id);
        return;
    }

    tig_file_fwrite(data_pkt->data, sizeof(*data_pkt->data), data_pkt->size, xfer->stream);
    xfer->recieved_bytes += data_pkt->size;

    info_pkt.type = TIG_NET_XFER_INFO_TYPE_ACK;
    info_pkt.xfer_id = data_pkt->xfer_id;
    tig_net_send_generic(&info_pkt, sizeof(info_pkt), 11, xfer->client_id);
}

// 0x534CB0
void tig_net_xfer_recv_info_pkt(void* pkt)
{
    char buffer[TIG_MAX_PATH];
    TigNetXferInfoPacket* info_pkt;
    TigNetXfer* xfer;

    info_pkt = (TigNetXferInfoPacket*)pkt;
    switch (info_pkt->type) {
    case TIG_NET_XFER_INFO_TYPE_START:
        if (tig_net_xfer_get_free_slot(&xfer)) {
            TigNetXferInfoPacket ack_pkt;
            char* directory_path;
            char* sep;

            xfer->id = info_pkt->xfer_id;
            xfer->path = STRDUP(info_pkt->path);

            directory_path = STRDUP(info_pkt->path);
            sep = strrchr(directory_path, '\\');
            if (sep != NULL) {
                *sep = '\0';
            }

            if (*directory_path != '\0') {
                tig_file_mkdir(directory_path);
            }

            FREE(directory_path);

            snprintf(buffer, sizeof(buffer), "%s.%s", xfer->path, ".RECV");

            xfer->stream = tig_file_fopen(buffer, "wb");
            xfer->flags = TIG_NET_XFER_ACTIVE | TIG_NET_XFER_RECEIVER;
            xfer->client_id = sub_52A530();
            xfer->total_bytes = info_pkt->size;

            tig_debug_printf("TIG: net_xfer: begin receiving file \"%s\" id 0x%08X size %d\n",
                xfer->path,
                xfer->id,
                xfer->total_bytes);

            ack_pkt.type = TIG_NET_XFER_INFO_TYPE_ACK;
            ack_pkt.xfer_id = info_pkt->xfer_id;
            tig_net_send_generic(&ack_pkt, sizeof(ack_pkt), 11, xfer->client_id);
        }
        break;
    case TIG_NET_XFER_INFO_TYPE_END:
        if (tig_net_xfer_find(info_pkt->xfer_id, &xfer)) {
            tig_debug_printf("TIG: net_xfer: ID=%08X, to=%d, file \"%s\" is complete\n",
                xfer->id,
                xfer->client_id,
                xfer->path);
            tig_file_fclose(xfer->stream);

            snprintf(buffer, sizeof(buffer), xfer->path, ".RECV");
            tig_file_rename(buffer, xfer->path);
            FREE(xfer->path);
            tig_net_xfer_reset(xfer);
        }
        break;
    case TIG_NET_XFER_INFO_TYPE_ACK:
        if (tig_net_xfer_find(info_pkt->xfer_id, &xfer)) {
            tig_next_xfer_send_next_chunk(xfer);
        }
        break;
    }
}

// 0x534ED0
void tig_next_xfer_send_next_chunk(TigNetXfer* xfer)
{
    TigNetXferInfoPacket info_pkt;
    TigNetXferDataPacket data_pkt;

    if ((xfer->flags & TIG_NET_XFER_ACTIVE) != 0 && (xfer->flags & TIG_NET_XFER_RECEIVER) == 0) {
        if (tig_file_feof(xfer->stream)) {
            info_pkt.type = TIG_NET_XFER_INFO_TYPE_END;
            info_pkt.xfer_id = xfer->id;
            tig_debug_printf("TIG: net_xfer: ID=%08X, to=%d, file \"%s\" has completed\n", xfer->id, xfer->client_id, xfer->path);
            tig_net_send_generic(&info_pkt, sizeof(info_pkt), 11, xfer->client_id);
            FREE(xfer->path);
            tig_file_fclose(xfer->stream);
            tig_net_xfer_reset(xfer);
        } else {
            memset(&data_pkt, -1, sizeof(data_pkt));
            data_pkt.size = tig_file_fread(data_pkt.data, sizeof(*data_pkt.data), sizeof(data_pkt.data), xfer->stream);
            if (data_pkt.size > 0) {
                data_pkt.xfer_id = xfer->id;
                tig_net_send_generic(&data_pkt, sizeof(data_pkt), 10, xfer->client_id);
            }
        }
    }
}

// 0x534FC0
int sub_534FC0(int id)
{
    TigNetXfer* xfer;
    TigNetXferInfoPacket info_pkt;

    if (!tig_net_xfer_find(id, &xfer)) {
        return TIG_ERR_16;
    }

    info_pkt.type = TIG_NET_XFER_INFO_TYPE_START;
    info_pkt.xfer_id = id;
    tig_net_send_generic(&info_pkt, sizeof(info_pkt), 11, xfer->client_id);
    tig_file_fclose(xfer->stream);
    FREE(xfer->path);
    tig_net_xfer_reset(xfer);

    return TIG_OK;
}

// 0x535050
int tig_net_xfer_count(int client_id)
{
    int index;
    int count = 0;

    for (index = 0; index < TIG_NET_XFER_MAX_SLOTS; index++) {
        if ((tig_net_xfers[index].flags & TIG_NET_XFER_ACTIVE) != 0) {
            if (tig_net_xfers[index].client_id == client_id) {
                count++;
            }
        }
    }

    return count;
}

// 0x535080
void tig_net_xfer_remove_all(int client_id)
{
    int index;

    for (index = 0; index < TIG_NET_XFER_MAX_SLOTS; index++) {
        if ((tig_net_xfers[index].flags & TIG_NET_XFER_ACTIVE) != 0) {
            if (tig_net_xfers[index].client_id == client_id) {
                if (tig_net_xfers[index].stream != NULL) {
                    tig_file_fclose(tig_net_xfers[index].stream);
                }

                if (tig_net_xfers[index].path != NULL) {
                    FREE(tig_net_xfers[index].path);
                }

                tig_net_xfer_reset(&(tig_net_xfers[index]));
            }
        }
    }
}

// 0x5350D0
int tig_net_xfer_status(int id, char* path, int* progress)
{
    TigNetXfer* xfer;

    if (!tig_net_xfer_find(id, &xfer)) {
        return TIG_ERR_16;
    }

    if (path != NULL) {
        strncpy(path, xfer->path, TIG_MAX_PATH);
    }

    if (progress != NULL) {
        *progress = 100 * xfer->recieved_bytes / xfer->total_bytes;
    }

    return TIG_OK;
}

// 0x535140
bool tig_net_xfer_get_free_slot(TigNetXfer** xfer_ptr)
{
    int index;

    for (index = 0; index < TIG_NET_XFER_MAX_SLOTS; index++) {
        if ((tig_net_xfers[index].flags & TIG_NET_XFER_ACTIVE) == 0) {
            *xfer_ptr = &(tig_net_xfers[index]);
            return true;
        }
    }

    return false;
}

// 0x535180
bool tig_net_xfer_find(int id, TigNetXfer** xfer_ptr)
{
    int index;

    for (index = 0; index < TIG_NET_XFER_MAX_SLOTS; index++) {
        if ((tig_net_xfers[index].flags & TIG_NET_XFER_ACTIVE) != 0) {
            if (tig_net_xfers[index].id == id) {
                *xfer_ptr = &(tig_net_xfers[index]);
                return true;
            }
        }
    }

    return false;
}
