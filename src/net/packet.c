#include <stdio.h>
#include "../sys/fixed_queue.h"
#include "../sys/types.h"
#include "packet.h"

#define RING_SIZE 256


static FIXEDQ(, struct client_rx_buf, RING_SIZE)   client_rx_bufstack;


void packet_init_pool()
{
    FIXEDQ_INIT(client_rx_bufstack);
}

BOOL packet_check(struct packet *pkt)
{
    return FALSE;
}

void packet_free(struct client_rx_buf *buf)
{

}
BOOL packet_data_singlefrag(struct packet *pkt)
{
    return FALSE;
}

void * packet_data_firstfrag(struct packet *pkt)
{
    return NULL;
}

void packet_clear_flag(struct packet *pkt, UINT32 flag)
{

}

struct client_rx_buf* packet_alloc()
{
    struct client_rx_buf *buf = FIXEDQ_ALLOC(client_rx_bufstack);
    if(buf == NULL) {
        printf("empty client_rx_bufs\n");
        return NULL;
    }

    return buf;
}

// void packet_data_dma(struct packet *pkt, struct xfrag_item *xf, UINT32 len)
// {
//     printf("xxx: check valid dma\n");
// }
