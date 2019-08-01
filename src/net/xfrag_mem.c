
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>
#include <assert.h>

#include "../sys/types.h"
#include "../kern/sys.h"
#include "xfrag_mem.h"


static TAILQ_HEAD(, xfrag) xfrag_pool_head;
static TAILQ_HEAD(, xfrag) xfrag_pool_headtx;
void *xf_base_addr;
//static UINT64 offset = 0;

xfrag_ussage_stats_t xfrag_stats = {
    .xfrag_rx_used = 0,
    .xfrag_rx_avail = 0,
    .xfrag_tx_used = 0,
    .xfrag_rx_avail = 0,
};

void xfrag_pool_init(void *pool_base, UINT64 pool_len, int num_of_bufs)
{
    int i;
    size_t sz = num_of_bufs * XFRAG_SIZE;
    void *rawbase = pool_base;
    int count = 0;
    UINT64 offset = 0;

    TAILQ_INIT(&xfrag_pool_head);
    TAILQ_INIT(&xfrag_pool_headtx);

    xf_base_addr = malloc(sizeof(struct xfrag ) * num_of_bufs);
    //printf("sizeof struct xf %ld\n", sizeof(struct xfrag));

    if(xf_base_addr == NULL) {
        printf("Failed to alloc xfrag base addr\n");
        exit(EXIT_FAILURE);
    }

    for(i = 0; i < num_of_bufs; i++) {

        struct xfrag *xf = (struct xfrag *)(xf_base_addr + offset);
        if(xf == NULL){
            printf("Failed to malloc rx_xfrag_t\n");
            exit(EXIT_FAILURE);
        }
        xf->idx = 0;
        xf->len = 0;
        //xf->next = NULL;
        xf->magic = 0;
        //memset(xf, 0, sizeof(struct xfrag));
        void *rawbytes = rawbase + (XFRAG_SIZE * i);
        assert((UINT64)rawbytes + XFRAG_SIZE <= (UINT64)rawbase + sz);
        xf->data = rawbytes;


        //xxx: add stats here
        if(count < (num_of_bufs / 2)) {
            TAILQ_INSERT_TAIL(&xfrag_pool_head, xf, next);
        }
        else {
            TAILQ_INSERT_TAIL(&xfrag_pool_headtx, xf, next);
        }

        //printf("xfrag in pool %lld with xdata-> %p %lld\n", (UINT64)xf, xf->data, (UINT64)xf->data);

        offset += sizeof(struct xfrag);
        count++;
        //offset += BUF_SIZE;
    }

}

struct xfrag * xfrag_alloc(bool rx)
{
    struct xfrag *xf;
    if(rx) {

        xf  = TAILQ_FIRST(&xfrag_pool_head);
        if(xf == NULL) {
            DESCSOCK_LOG("xfrag pool is empty\n");
            return NULL;
        }

        TAILQ_REMOVE(&xfrag_pool_head, xf, next);
        xf->len = 0;
        xf->idx = -1;
        //memset(xf->data, 0, BUF_SIZE);
    }
    else {
        xf = TAILQ_FIRST(&xfrag_pool_headtx);
        if(xf == NULL) {
            DESCSOCK_LOG("xfragtx pool is empty\n");
            return NULL;
        }
        TAILQ_REMOVE(&xfrag_pool_headtx, xf, next);
        xf->len = 0;
        xf->idx = -1;
        //memset(xf->data, 0, BUF_SIZE);
    }

    //printf("Allocated xfrag with base %p %lld\n", xf->data, (UINT64)xf->data);

    return xf;
}

void xfrag_free(struct xfrag *xf, bool rx)
{
    printf("freeing xfrag with base %p %lld\n", xf->data, (UINT64)xf->data);
    if(rx) {
        TAILQ_INSERT_TAIL(&xfrag_pool_head, xf, next);
    }
    else {
        TAILQ_INSERT_TAIL(&xfrag_pool_headtx, xf, next);
    }
}

void xfrag_pool_free(void)
{
    if(xf_base_addr != NULL) {
        free(xf_base_addr);
    }
}


void print_xfrag_pool(void)
{
    printf("xfrag pool dump\n");

    struct xfrag *xf;
    TAILQ_FOREACH(xf, &xfrag_pool_head, next) {
        printf("xfrag->len %d xfrag->data %p %lld\n", xf->len, xf->data, (UINT64)xf->data);
    }
}