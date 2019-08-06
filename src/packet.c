#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "common.h"
#include "fixed_queue.h"
#include "types.h"
#include "packet.h"

/*
 * Most of this logic comes from TMM's Packet API
 */

/*
 * Packet mem pool is serviced though a simple linkedlist stack
 */
static SLIST_HEAD(, packet)     pkt_stack_head;
static void *pkts_base_addr = NULL;


/* Allocate a block of memory and build packet objects, and add to pkt stack */
err_t
packet_init_pool(int num_of_pkts)
{
    int i;
    SLIST_INIT(&pkt_stack_head);
    UINT64 offset = 0;

    pkts_base_addr = malloc(sizeof(struct packet) * num_of_pkts);
    if(pkts_base_addr == NULL) {
        DESCSOCK_LOG("pkts base address returned NULL on malloc\n");
        goto err_out;
    }

    for(i = 0; i < num_of_pkts; i++) {
        struct packet *pkt = (struct packet *)(pkts_base_addr + offset);

        // XXX: validate pkt is not off bounds
        SLIST_INSERT_HEAD(&pkt_stack_head, pkt, next);

        offset += sizeof(struct packet);
    }

    return ERR_OK;

err_out:
    return ERR_MEM;
}

/* Free the previously allocated blob of mem */
void packet_pool_free()
{
    if(pkts_base_addr != NULL) {
        free(pkts_base_addr);
        pkts_base_addr = NULL;
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
        return NULL;
    }
    /* Pop stack */
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