/*
 * $F5Copyright_C:
 * Copyright (C) F5 Networks, Inc. 2018
 *
 * No part of the software may be reproduced or transmitted in any
 * form or by any means, electronic or mechanical, for any purpose,
 * without express written permission of F5 Networks, Inc. $
 *
 * Descriptor Socket Network Interface Driver
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "types.h"
#include "sys.h"
#include "xfrag_mem.h"
#include "descsock_client.h"

/*
 * Most of this logic comes from TMM's Xbuf API
 */


/*
 * Xfrag mem pool is serviced though a doubly linked linkedlist
 */

static TAILQ_HEAD(, xfrag) xfrag_pool_head;
static TAILQ_HEAD(, xfrag) xfrag_pool_headtx;
static void *xf_base_addr = NULL;


struct xfrag_ussage_stats xfrag_stats = {
    .xfrag_rx_count = 0,
    .xfrag_tx_count = 0,
    .xfrag_rx_free = 0,
    .xfrag_rx_free = 0,
};

err_t
xfrag_pool_init(void *pool_base, UINT64 pool_len, int num_of_bufs)
{
    int i;
    size_t sz = num_of_bufs * XFRAG_SIZE;
    void *rawbase = pool_base;
    int count = 0;
    UINT64 offset = 0;
    int total_bufs = num_of_bufs / 2;

    TAILQ_INIT(&xfrag_pool_head);
    TAILQ_INIT(&xfrag_pool_headtx);

    xf_base_addr = malloc(sizeof(struct xfrag ) * num_of_bufs);

    if(xf_base_addr == NULL) {
        DESCSOCK_LOG("Failed to alloc xfrag base addr\n");
        goto err_out;
    }

    for(i = 0; i < num_of_bufs; i++) {

        struct xfrag *xf = (struct xfrag *)(xf_base_addr + offset);
        if(xf == NULL){
            DESCSOCK_LOG("Failed to malloc rx_xfrag_t\n");
            goto err_out;
        }

        xf->idx = 0;
        xf->len = 0;
        //xf->next = NULL;
        xf->magic = 0;

        void *rawbytes = rawbase + (XFRAG_SIZE * i);
        assert((UINT64)rawbytes + XFRAG_SIZE <= (UINT64)rawbase + sz);
        xf->data = rawbytes;
        /*
         * Check for mod 4 alignment
         */
        is_4b_aligned((UINT64)xf->data);

        //xxx: add stats here
        /* Half of bufs are used as Rx DMA bufs and the other half for Tx DMA bufs */
        if(count < (num_of_bufs / 2)) {
            TAILQ_INSERT_TAIL(&xfrag_pool_head, xf, next);
        }
        else {
            TAILQ_INSERT_TAIL(&xfrag_pool_headtx, xf, next);
        }

        DESCSOCK_DEBUGF("xfrag in pool %lld with xdata-> %p %lld\n", (UINT64)xf, xf->data, (UINT64)xf->data);

        offset += sizeof(struct xfrag);
        count++;
    }

    xfrag_stats.xfrag_rx_count = total_bufs;
    xfrag_stats.xfrag_tx_count = total_bufs;

    xfrag_stats.xfrag_rx_free = total_bufs;
    xfrag_stats.xfrag_tx_free = total_bufs;

    return ERR_OK;

err_out:
    return ERR_MEM;

}

/*
 * Allocates a xfrag from the queue, if rx == true we need a xfrag for Rx packets
 * else we need an xfrag for Tx packet
 * Returns an xfrag object on success, NULL on failure
 */
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
        xfrag_stats.xfrag_rx_free--;
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
        xfrag_stats.xfrag_tx_free--;
    }

    DESCSOCK_DEBUGF("Allocated xfrag with base %p %lld\n", xf->data, (UINT64)xf->data);

    return xf;
}

/* Frees an xfrag object, gets put back into the rx or tx quees */
void xfrag_free(struct xfrag *xf, bool rx)
{
    DESCSOCK_DEBUGF("freeing xfrag with base %p %lld\n", xf->data, (UINT64)xf->data);

    if(rx) {
        TAILQ_INSERT_TAIL(&xfrag_pool_head, xf, next);
        xfrag_stats.xfrag_rx_free++;
    }
    else {
        TAILQ_INSERT_TAIL(&xfrag_pool_headtx, xf, next);
        xfrag_stats.xfrag_tx_free++;
    }
}

/* free all the blog of mem needed for the xfrag mem pools */
void xfrag_pool_free(void)
{
    if(xf_base_addr != NULL) {

        free(xf_base_addr);

        xf_base_addr = NULL;
    }
}


void print_xfrag_pool(void)
{
    DESCSOCK_LOG("xfrag pool dump\n");

    struct xfrag *xf;
    TAILQ_FOREACH(xf, &xfrag_pool_head, next) {
        DESCSOCK_LOG("xfrag->len %d xfrag->data %p %lld\n", xf->len, xf->data, (UINT64)xf->data);
    }
}

/* Will return the number of available free spots to insert data to */
int xfrag_pool_avail_count()
{

    return xfrag_stats.xfrag_tx_free;
}

/* Simple check for 4B alignment */
void
is_4b_aligned(UINT64 buf)
{
    uint8_t mod4aligned =  (buf & 3) == 0;
    /* assert(mod4aligned); */

    if(!mod4aligned) {
        descsock_client_stats.not4b++;
    }
}