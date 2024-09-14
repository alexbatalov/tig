#ifndef TIG_NET_XFER_H_
#define TIG_NET_XFER_H_

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int tig_net_xfer_init();
void tig_net_xfer_exit();
void tig_net_xfer_ping();
int tig_net_xfer_send(const char* path, int client_id, int* xfer_id);
int tig_net_xfer_send_as(const char* path, const char* alias, int client_id, int* xfer_id);
void tig_net_xfer_recv_data_pkt(void* data_pkt);
void tig_net_xfer_recv_info_pkt(void* info_pkt);
int tig_net_xfer_count(int client_id);
void tig_net_xfer_remove_all(int client_id);
int tig_net_xfer_status(int id, char* path, int* progress);

#ifdef __cplusplus
}
#endif

#endif /* TIG_NET_XFER_H_ */
