#ifndef PACKET_H
#define PACKET_h

#include <sys/queue.h>

#include "../sys/types.h"
#include "xfrag_mem.h"

#define PACKET_FLAG_LOCKED 1

struct packet {
    LIST_ENTRY(packet) next;
    void *buf;
    UINT32 len;
    BOOL locked;
};

BOOL packet_check(struct packet *pkt);

void packet_free(struct client_rx_buf *buf);

BOOL packet_data_singlefrag(struct packet *pkt);

void * packet_data_firstfrag(struct packet *pkt);

void packet_clear_flag(struct packet *pkt, UINT32 flag);

struct client_rx_buf* packet_alloc(void);

//void packet_data_dma(struct packet *pkt, struct xfrag_item *xf, UINT32 len);

struct packet;

#endif