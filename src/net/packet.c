#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include "../sys/fixed_queue.h"
#include "../sys/types.h"
#include "packet.h"



//static TAILQ_HEAD(, packet) pkt_pool_head;
//static FIXEDQ(, struct packet, PKT_QUEUE_SIZE) pkt_queue;
static SLIST_HEAD(, packet)     pkt_stack_head;
void *pkts_base_addr;


void packet_init_pool(int num_of_pkts)
{
    int i;
    UINT64 offset = 0;

    pkts_base_addr = malloc(sizeof(struct packet) * num_of_pkts);
    if(pkts_base_addr == NULL) {
        DESCSOCK_LOG("pkts base addre retunred NULL on malloc\n");
        exit(EXIT_FAILURE);
    }

    for(i = 0; i < num_of_pkts; i++) {
        struct packet *pkt = (struct packet *)(pkts_base_addr + offset);

        // XXX: validate pkt is not off bounds

        SLIST_INSERT_HEAD(&pkt_stack_head, pkt, next);

        offset += sizeof(struct packet);
    }

}

void packet_pool_free()
{
    if(pkts_base_addr != NULL) {
        free(pkts_base_addr);
    }
}

BOOL packet_check(struct packet *pkt)
{
    return FALSE;
}

struct packet* packet_alloc()
{
    struct packet *pkt = SLIST_FIRST(&pkt_stack_head);
    if(pkt == NULL) {
        DESCSOCK_LOG("No packets in pool\n");
        exit(EXIT_FAILURE);
    }
    //printf("allocated packet %p\n", pkt);

    SLIST_REMOVE_HEAD(&pkt_stack_head, next);

    return pkt;

}
void packet_free(struct packet *pkt)
{
    pkt->vlan_tag = 0;
    pkt->len = 0;
    pkt->magic = 0;
    pkt->flags = 0;
    pkt->xf_first = NULL;

    SLIST_INSERT_HEAD(&pkt_stack_head, pkt, next);
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


// void packet_data_dma(struct packet *pkt, struct xfrag_item *xf, UINT32 len)
// {
//     printf("xxx: check valid dma\n");
// }
