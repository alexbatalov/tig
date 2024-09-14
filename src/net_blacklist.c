// NET BLACKLIST
// ---
//
// Contains utility functions to manage list of banned ip addresses.
//
// NOTES
//
// - The blacklist is volatile (i.e. not saved in some file).
//
// - There is no API to unban certain ip address. The `tig_net_blacklist_clear`
// which is used to reset entire blacklist is never called by the game (aside
// from `tig_net_blacklist_exit`). This means there is no in-game UI to unban
// clients.

#include "tig/net_blacklist.h"

#include <string.h>

#include "tig/memory.h"

typedef struct TigNetBlacklistNode {
    /* 0000 */ char* field_0;
    /* 0004 */ char* ip_address;
    /* 0008 */ struct TigNetBlacklistNode* next;
} TigNetBlacklistNode;

static_assert(sizeof(TigNetBlacklistNode) == 0xC, "wrong size");

static bool tig_net_blacklist_clear();

// 0x638BB0
static TigNetBlacklistNode* tig_net_blacklist_head;

// 0x638BB4
static int tig_net_blacklist_size;

// 0x53AB00
bool tig_net_blacklist_init()
{
    tig_net_blacklist_head = NULL;
    tig_net_blacklist_size = 0;

    return true;
}

// 0x53AB20
void tig_net_blacklist_exit()
{
    tig_net_blacklist_clear();
}

// 0x53AB30
bool tig_net_blacklist_clear()
{
    TigNetBlacklistNode* node;

    node = tig_net_blacklist_head;
    while (node != NULL) {
        tig_net_blacklist_head = node->next;
        if (node->field_0 != NULL) {
            FREE(node->field_0);
        }
        if (node->ip_address != NULL) {
            FREE(node->ip_address);
        }
        FREE(node);
        node = tig_net_blacklist_head;
        tig_net_blacklist_size--;
    }

    return true;
}

// 0x53AB90
bool tig_net_blacklist_add(const char* a1, const char* ip_address)
{
    TigNetBlacklistNode* node;

    if (a1 == NULL) {
        a1 = "*";
    }

    if (ip_address == NULL) {
        ip_address = "*";
    }

    node = (TigNetBlacklistNode*)MALLOC(sizeof(*node));
    node->field_0 = STRDUP(a1);
    node->ip_address = STRDUP(ip_address);
    node->next = tig_net_blacklist_head;
    tig_net_blacklist_size++;

    return true;
}

// 0x53ABF0
bool tig_net_blacklist_has(const char* a1, const char* ip_address)
{
    TigNetBlacklistNode* node;

    (void)a1;

    node = tig_net_blacklist_head;
    while (node != NULL) {
        if (strcmpi(node->ip_address, ip_address) == 0) {
            return true;
        }
        node = node->next;
    }
    return false;
}
