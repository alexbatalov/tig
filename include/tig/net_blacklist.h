#ifndef TIG_NET_BLACKLIST_H_
#define TIG_NET_BLACKLIST_H_

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

bool tig_net_blacklist_init();
void tig_net_blacklist_exit();
bool tig_net_blacklist_add(const char* a1, const char* ip_address);
bool tig_net_blacklist_has(const char* a1, const char* ip_address);

#ifdef __cplusplus
}
#endif

#endif /* TIG_NET_BLACKLIST_H_ */
