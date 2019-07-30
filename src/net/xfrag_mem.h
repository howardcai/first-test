#ifndef XBUF_H
#define XBUF_H
#include <sys/queue.h>

#include "../sys/types.h"

#define XFRAG_SIZE 2048

struct xfrag {
    UINT32 magic;
    void *data;
    UINT16 len;
    TAILQ_ENTRY(xfrag) next;
};

typedef struct {
    UINT64 xfrag_rx_used;
    UINT64 xfrag_rx_avail;

    UINT64 xfrag_tx_used;
    UINT64 xfrag_tx_avail;
} xfrag_ussage_stats_t;

typedef struct rx_xfrag {
    TAILQ_ENTRY(rx_xfrag) xfrag_entry;
    UINT16  len;
    void    *base;
    bool    locked;
    UINT64  idx;
}rx_xfrag_t;

typedef struct tx_xfrag {
    TAILQ_ENTRY(tx_xfrag) xfrag_entry;
    UINT16  len;
    void    *base;
    bool    locked;
    UINT64  idx;
} tx_xfrag_t;

void xfrag_pool_init(void *pool_base, UINT64 pool_len, int num_of_bufs);
void xfrag_pool_free(void);
struct xfrag * xfrag_alloc(void);
void xfrag_free(struct xfrag *xf);


#endif