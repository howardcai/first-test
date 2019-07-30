
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>

#include "../sys/types.h"
#include "../kern/sys.h"
#include "xfrag_mem.h"


TAILQ_HEAD(rx_xfrag_pool_list, rx_xfrag_t) rx_xfrag_pool_head;
TAILQ_HEAD(tx_xfrag_pool_list, tx_xfrag_t) tx_xfrag_pool_head;

static TAILQ_HEAD(, xfrag) xfrag_pool_head;
static UINT64 offset = 0;

static xfrag_ussage_stats_t xfrag_stats = {
    .xfrag_rx_used = 0,
    .xfrag_rx_avail = 0,
    .xfrag_tx_used = 0,
    .xfrag_rx_avail = 0,
};

void xfrag_pool_init(void *pool_base, UINT64 pool_len, int num_of_bufs)
{
    int i;
    struct xfrag *xf;

    TAILQ_INIT(&xfrag_pool_head);

    for(i = 0; i < num_of_bufs; i++) {
        xf = malloc(sizeof(rx_xfrag_t));
        if(xf == NULL){
            printf("Failed to malloc rx_xfrag_t\n");
            exit(EXIT_FAILURE);
        }
        memset(xf, 0, sizeof(struct xfrag));
        xf->data = pool_base + offset;

        //xxx: add stats here
        TAILQ_INSERT_TAIL(&xfrag_pool_head, xf, next);

        offset += BUF_SIZE;
    }

}

struct xfrag * xfrag_alloc()
{
    struct xfrag *xf  = TAILQ_FIRST(&xfrag_pool_head);
    if(xf == NULL) {
        printf("xfrag pool is empty\n");
        return NULL;
    }

    TAILQ_REMOVE(&xfrag_pool_head, xf, next);
    xf->len = 0;
    memset(xf->data, 0, BUF_SIZE);

    return xf;
}

void xfrag_free(struct xfrag *xf)
{
    TAILQ_INSERT_TAIL(&xfrag_pool_head, xf, next);

}

void xfrag_pool_free(void)
{
    struct xfrag *xf;

    while(TAILQ_EMPTY(&xfrag_pool_head)) {
        xf = TAILQ_FIRST(&xfrag_pool_head);
        TAILQ_REMOVE(&xfrag_pool_head, xf, next);

        free(xf);
    }
}