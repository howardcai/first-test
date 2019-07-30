#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include "../sys/fixed_queue.h"
#include "../sys/types.h"
#include "packet.h"

#define RING_SIZE 256


static FIXEDQ(, struct client_rx_buf, RING_SIZE)    client_rx_bufstack;
static TAILQ_HEAD(, packet)                         pkt_pool_head;


void packet_init_pool(int num_of_pkts)
{
    int i;
    struct packet *pkt;

    FIXEDQ_INIT(client_rx_bufstack);
    TAILQ_INIT(&pkt_pool_head);

    for(i = 0; i < num_of_pkts; i++) {

        pkt = malloc(sizeof(struct packet));
        memset(pkt, 0, sizeof(struct packet));

        if(pkt == NULL) {
            printf("Failed to malloc packet\n");
            return;
        }

        TAILQ_INSERT_TAIL(&pkt_pool_head, pkt, next);
    }
}

void packet_pool_free()
{
    struct packet *pkt;
    struct xfrag *xf;

    while(TAILQ_EMPTY(&pkt_pool_head)) {
        pkt = TAILQ_FIRST(&pkt_pool_head);
        TAILQ_REMOVE(&pkt_pool_head, pkt, next);

        // free xfrags here
        xf = pkt->xf_first;
        xfrag_free(xf);

        free(pkt);
    }
}

BOOL packet_check(struct packet *pkt)
{
    return FALSE;
}

struct packet* packet_alloc()
{
    struct packet *pkt = TAILQ_FIRST(&pkt_pool_head);
    if(pkt == NULL) {
        printf("No packets in pool\n");
        exit(EXIT_FAILURE);
    }
    memset(pkt, 0, sizeof(struct packet));

    return pkt;
}
void packet_free(struct packet *pkt)
{
    /*
     * reset flags, put back into circulation
     */
    memset(pkt, 0, sizeof(struct packet));
    TAILQ_INSERT_TAIL(&pkt_pool_head, pkt, next);

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
