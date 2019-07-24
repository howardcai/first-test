
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>

#include "../sys/types.h"
#include "../kern/sys.h"
#include "xfrag_mem.h"


TAILQ_HEAD(rx_xfrag_pool_list, rx_xfrag_t) rx_xfrag_pool_head;
TAILQ_HEAD(tx_xfrag_pool_list, tx_xfrag_t) tx_xfrag_pool_head;

static UINT64 offset = 0;

static xfrag_ussage_stats_t xfrag_stats = {
    .xfrag_rx_used = 0,
    .xfrag_rx_avail = 0,
    .xfrag_tx_used = 0,
    .xfrag_rx_avail = 0,
};


/*
 * Rx xfrag for producer and return
 */
void rx_xfrag_pool_init(void *pool_base, UINT64 pool_len, int num_of_bufs)
{
    // XXX: VAlidate that we don't run over the pool_len
    TAILQ_INIT(&rx_xfrag_pool_head);
    int i;
    rx_xfrag_t *xf;

    for(i = 0; i < num_of_bufs; i++) {
        xf = malloc(sizeof(rx_xfrag_t));
        if(xf == NULL){
            printf("Failed to malloc rx_xfrag_t\n");
            return;
        }

        xf->base = pool_base + offset;
        xf->locked = FALSE;

        TAILQ_INSERT_TAIL(&rx_xfrag_pool_head, xf, xfrag_entry);
        offset += BUF_SIZE;
    }

    xfrag_stats.xfrag_rx_avail = num_of_bufs;
}

rx_xfrag_t* rx_xfrag_alloc(int mss)
{
    rx_xfrag_t *xf = TAILQ_FIRST(&rx_xfrag_pool_head);

    if(xf == NULL) {
        printf("no more rx_xfrags in pool\n");
        return NULL;
    }

    TAILQ_REMOVE(&rx_xfrag_pool_head, xf, xfrag_entry);

    xf->len = 0;
    xf->locked = TRUE;
    memset(xf->base, 0, BUF_SIZE);

    xfrag_stats.xfrag_rx_used++;
    xfrag_stats.xfrag_rx_avail--;

    return xf;
}

void rx_xfrag_free(rx_xfrag_t *xf)
{
    TAILQ_INSERT_TAIL(&rx_xfrag_pool_head, xf, xfrag_entry);
    xfrag_stats.xfrag_rx_used--;
    xfrag_stats.xfrag_rx_avail++;
}

/*
 * Rx xfrag for sending and completions
*/
void tx_xfrag_pool_init(void *poolbase, UINT64 pool_len, int num_of_bufs)
{
    // XXX: VAlidate that we don't run over the pool_len
    TAILQ_INIT(&tx_xfrag_pool_head);
    tx_xfrag_t *xf;
    int i;

    for(i = 0; i < num_of_bufs; i++) {
        xf = malloc(sizeof(tx_xfrag_t));
        if(xf == NULL) {
            printf("Failed to malloc tx_xfrag_t\n");
            return;
        }

        xf->base = poolbase + offset;
        xf->locked = FALSE;

        TAILQ_INSERT_TAIL(&tx_xfrag_pool_head, xf, xfrag_entry);
        offset += BUF_SIZE;
    }

    xfrag_stats.xfrag_tx_avail = num_of_bufs;
}

tx_xfrag_t* tx_xfrag_alloc(int mss)
{
    tx_xfrag_t *xf = TAILQ_FIRST(&tx_xfrag_pool_head);

    if(xf == NULL) {
        printf("no more tx_xfrags in pool\n");
        return NULL;
    }

    TAILQ_REMOVE(&tx_xfrag_pool_head, xf, xfrag_entry);

    xf->len = 0;
    xf->idx = 0;
    xf->locked = TRUE;
    memset(xf->base, 0, BUF_SIZE);

    xfrag_stats.xfrag_tx_used++;
    xfrag_stats.xfrag_tx_avail--;

    return xf;
}
void tx_xfrag_free(tx_xfrag_t *xf)
{
    TAILQ_INSERT_TAIL(&tx_xfrag_pool_head, xf, xfrag_entry);
    xfrag_stats.xfrag_tx_used--;
    xfrag_stats.xfrag_tx_avail++;
}