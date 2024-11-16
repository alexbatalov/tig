#ifndef TIG_NET_H_
#define TIG_NET_H_

#include <winsock2.h>
#include <ws2tcpip.h>

#include "tig/timer.h"
#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIG_NET_CLIENT_NAME_LENGTH 24
#define TIG_NET_DESCRIPTION_SIZE 23
#define TIG_NET_PASSWORD_NAME_SIZE 24
#define TIG_NET_MAX_PLAYERS 8

#define TIG_NET_LISTEN_ASYNC_SELECT (WM_USER + 1)
#define TIG_NET_BCAST_ASYNC_SELECT (WM_USER + 2)

#define TIG_NET_CONNECTED 0x01
#define TIG_NET_HOST 0x02
#define TIG_NET_WAITING 0x04
#define TIG_NET_FLAG_0x08 0x08
#define TIG_NET_AUTO_JOIN 0x10
#define TIG_NET_NO_BROADCAST 0x20
#define TIG_NET_RESET 0x40

#define TIG_NET_DISCONNECT_REASON_LEAVE 0
#define TIG_NET_DISCONNECT_REASON_WRONG_PASSWORD 1
#define TIG_NET_DISCONNECT_REASON_BANNED 2
#define TIG_NET_DISCONNECT_REASON_INCOMPATIBLE_PROTOCOL 3
#define TIG_NET_DISCONNECT_REASON_DISALLOWED 4
#define TIG_NET_DISCONNECT_REASON_APP_SPECIFIC 5

typedef enum TigNetServerType {
    TIG_NET_SERVER_TYPE_FREE_FOR_ALL = 0,
    TIG_NET_SERVER_TYPE_COOPERATIVE = 1,
    TIG_NET_SERVER_TYPE_ROLEPLAY = 2,
} TigNetServerType;

typedef enum TigNetServerOptions {
    TIG_NET_SERVER_PLAYER_KILLING = 0x0001,
    TIG_NET_SERVER_FRIENDLY_FIRE = 0x0002,
    TIG_NET_SERVER_PRIVATE_CHAT = 0x0004,
    TIG_NET_SERVER_PASSWORD_PROTECTED = 0x0008,
    TIG_NET_SERVER_BOOKMARKED = 0x0010,
    TIG_NET_SERVER_AUTO_EQUIP = 0x0020,
    TIG_NET_SERVER_KEY_SHARING = 0x0040,
} TigNetServerOptions;

typedef enum TigNetServerFilters {
    TIG_NET_SERVER_FILTER_FREE_FOR_ALL = 0x0001,
    TIG_NET_SERVER_FILTER_COOPERATIVE = 0x0002,
    TIG_NET_SERVER_FILTER_ROLEPLAY = 0x0004,
    TIG_NET_SERVER_FILTER_BOOKMARKED = 0x0008,
    TIG_NET_SERVER_FILTER_INVERSE = 0x0080,
    TIG_NET_SERVER_FILTER_ALL = 0x007F,
} TigNetServerFilters;

typedef struct TigNetPlayer {
    char name[TIG_NET_CLIENT_NAME_LENGTH];
    int field_18;
} TigNetPlayer;

typedef struct TigNetServer {
    char name[TIG_NET_CLIENT_NAME_LENGTH - 1];
    char description[TIG_NET_DESCRIPTION_SIZE];
    TigNetServerType type;
    int min_level;
    int max_level;
    int max_players;
    int num_players;
    struct sockaddr_in addr;
    int field_54;
    int field_58;
    TigNetServerOptions options;
    tig_timestamp_t time;
    TigNetPlayer players[TIG_NET_MAX_PLAYERS];
} TigNetServer;

// See 0x527B90.
static_assert(sizeof(TigNetServer) == 0x144, "wrong size");

typedef bool(TigNetOnNetworkEvent)(int type, int client_id, void* data, int size);
typedef void(TigNetOnMessage)(void* msg);
typedef void(TigNetOnJoinResponse)(bool allowed);
typedef void(TigNetOnServersChange)(TigNetServer* servers, int count);
typedef bool(TigNetOnMessageValidation)(void* msg);
typedef void(TigNetOnServerBcast)(TigNetServer* server);

extern unsigned int tig_net_flags;

bool tig_net_client_is_active(int client_id);
bool tig_net_client_is_local(int client_id);
bool sub_526B20(int client_id);
const char* tig_net_client_info_get_name(int client_id);
bool tig_net_client_info_set_name(int client_id, const char* name);
int tig_net_client_info_get_bytes_sent(int client_id);
int tig_net_client_info_get_bytes_received(int client_id);
int sub_526CF0(int client_id);
bool sub_526D20(int client_id);
int sub_526D60(int client_id);
void sub_526E00();
int tig_net_inet_string(unsigned long addr, char* dest);
bool tig_net_inet_addr(const char* cp, unsigned long* addr);
int tig_net_init(TigInitInfo* init_info);
int tig_net_reset();
void tig_net_exit();
int tig_net_start_client();
bool tig_net_start_server();
bool sub_5280F0();
bool tig_net_connect_to_server(int server_id, void* buf, int size);
bool tig_net_connect_directly(const char* cp, void* buf, int size);
bool tig_net_reset_connection();
bool sub_5286E0();
void tig_net_on_network_event(TigNetOnNetworkEvent* func);
void tig_net_on_message(TigNetOnMessage* func);
void tig_net_on_join_response(TigNetOnJoinResponse* func);
void tig_net_on_servers_change(TigNetOnServersChange* func);
void tig_net_on_message_validation(TigNetOnMessageValidation* func);
void tig_net_auto_join_enable();
void tig_net_auto_join_disable();
bool tig_net_auto_join_is_enabled();
void sub_528790();
void sub_5287A0();
void tig_net_ping();
void tig_net_bcast_async_select(HWND hWnd, WPARAM wParam, LPARAM lParam);
void tig_net_listen_async_select(HWND hWnd, WPARAM wParam, LPARAM lParam);
bool tig_net_local_server_set_description(const char* description);
bool tig_net_local_server_set_max_players(int max_players);
int tig_net_local_server_get_max_players();
bool tig_net_local_client_set_name(const char* name);
const char* tig_net_local_client_get_name();
void tig_net_local_server_set_name(const char* name);
void tig_net_local_server_get_name(char* dest, size_t size);
void tig_net_local_server_set_password(const char* password);
size_t tig_net_local_server_get_password_length();
void tig_net_local_server_clear_password();
void tig_net_local_server_get_level_range(int* min_level_ptr, int* max_level_ptr);
void tig_net_local_server_set_level_range(int min_level, int max_level);
void tig_net_local_server_type_get(int* type_ptr);
void tig_net_local_bookmark_set_type(int type);
int tig_net_local_server_get_options();
void tig_net_local_server_enable_option(int flag);
void tig_net_local_server_disable_option(int flag);
int sub_529520();
void tig_net_send_generic(void* data, int size, uint8_t type, int client_id);
void tig_net_send_app_all(void* data, int size);
void tig_net_send_app_except(void* data, int size, int client_id);
void tig_net_send_app(void* data, int size, int client_id);
int sub_52A530();
void tig_net_enable_server_filter(int filter);
void tig_net_disable_server_filter(int filter);
int tig_net_get_server_filter();
bool tig_net_get_servers(TigNetServer** servers_ptr, int* num_servers_ptr);
bool sub_52A900();
void sub_52A940();
void sub_52A950();
void tig_net_client_set_loading(int client_id);
void tig_net_client_unset_loading(int client_id);
void sub_52A9E0(int client_id);
bool tig_net_client_is_waiting(int client_id);
void tig_net_shutdown(int client_id);
bool tig_net_client_is_lagging(int client_id);
bool sub_52ACE0(int client_id, char* dst, size_t size);
bool tig_net_ban(int client_id);
void tig_net_no_broadcast_enable();
void tig_net_no_broadcast_disable();
void tig_net_bookmark_add(unsigned long addr);
void tig_net_bookmark_remove(unsigned long addr);
void sub_52B0C0(struct sockaddr_in* addr);
void sub_52B0F0(struct sockaddr_in* addr);
void tig_net_on_server_bcast(TigNetOnServerBcast* func);
unsigned long tig_net_ntohl(unsigned long addr);
void sub_52B210();
void sub_52B220(int client_id);
void sub_52B270(int client_id);
void sub_52B2E0(int a1);

// NOTE: Seen in `anim_mp_reap_run_index`.
static inline bool tig_net_is_active() {
    return (tig_net_flags & TIG_NET_CONNECTED) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* TIG_NET_H_ */
