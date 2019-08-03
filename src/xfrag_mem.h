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

typedef struct {
    UINT64 xfrag_rx_used;
    UINT64 xfrag_rx_avail;

    UINT64 xfrag_tx_used;
    UINT64 xfrag_tx_avail;
} xfrag_ussage_stats_t;

err_t xfrag_pool_init(void *pool_base, UINT64 pool_len, int num_of_bufs);

void xfrag_pool_free(void);

struct xfrag * xfrag_alloc(bool rx);

void xfrag_free(struct xfrag *xf, bool rx);



#endif