#ifndef PACKET_H
#define PACKET_h

#include <sys/queue.h>

#include "../sys/types.h"
#include "xfrag_mem.h"

#define PACKET_FLAG_LOCKED 1

struct packet {
    UINT16              magic;
    UINT32              len;
    struct xfrag        *xf_first;
    vlan_t              vlan_tag;
    UINT64              flags;
    TAILQ_ENTRY(packet) next;
    /* Add more fields as we go  */
};

void packet_init_pool(int num_of_pkts);
void packet_pool_free();
BOOL packet_check(struct packet *pkt);

void packet_free(struct packet *pkt);

BOOL packet_data_singlefrag(struct packet *pkt);

void * packet_data_firstfrag(struct packet *pkt);

void packet_clear_flag(struct packet *pkt, UINT32 flag);

struct packet* packet_alloc(void);

//void packet_data_dma(struct packet *pkt, struct xfrag_item *xf, UINT32 len);

struct packet;

#endif