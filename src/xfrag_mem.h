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

#ifndef XBUF_H
#define XBUF_H
#include <sys/queue.h>

#include "types.h"
#include "err.h"

#define XFRAG_SIZE 2048

struct xfrag {
    UINT32 magic;
    void *data;
    UINT16 len;
    INT64 idx;
    TAILQ_ENTRY(xfrag) next;
};

struct xfrag_ussage_stats {
    UINT32 xfrag_rx_count;
    UINT64 xfrag_rx_free;

    UINT32 xfrag_tx_count;
    UINT64 xfrag_tx_free;
};

err_t xfrag_pool_init(void *pool_base, UINT64 pool_len, int num_of_bufs);

void xfrag_pool_free(void);

struct xfrag * xfrag_alloc(bool rx);

void xfrag_free(struct xfrag *xf, bool rx);

int xfrag_pool_avail_count(void);

void is_mod4_aligned(UINT64 buf);

#endif