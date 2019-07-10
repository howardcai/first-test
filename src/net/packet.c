#include <stdio.h>
#include "../sys/types.h"
#include "packet.h"

BOOL packet_check(struct packet *pkt)
{
    return FALSE;
}
void packet_free(struct packet *pkt)
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

struct packet * packet_alloc(void *buf)
{
    return NULL;
}

void packet_data_dma(struct packet *pkt, struct xfrag_item *xf, UINT32 len)
{

}
