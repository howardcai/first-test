#ifndef XBUF_H
#define XBUF_H
#include <sys/queue.h>

#include "../sys/types.h"

typedef struct {
    UINT64 xfrag_rx_used;
    UINT64 xfrag_rx_avail;

    UINT64 xfrag_tx_used;
    UINT64 xfrag_tx_avail;
} xfrag_ussage_stats_t;

struct xfrag_item {
    SLIST_ENTRY(xfrag_item) next;
    void    *base;
    UINT32  len;
    BOOL    locked;
};

typedef struct rx_xfrag {
    TAILQ_ENTRY(rx_xfrag) xfrag_entry;
    UINT16  len;
    void    *base;
    BOOL    locked;
}rx_xfrag_t;

typedef struct tx_xfrag {
    TAILQ_ENTRY(tx_xfrag) xfrag_entry;
    UINT16  len;
    void    *base;
    BOOL    locked;
    UINT64  idx;
} tx_xfrag_t;


/*
 * Rx xfrag for producer and return
 */
void rx_xfrag_pool_init(void *poolbase, UINT64 pool_len, int num_of_bufs);
rx_xfrag_t* rx_xfrag_alloc(int mss);
void rx_xfrag_free(rx_xfrag_t *xf);

/*
 * Rx xfrag for sending and completions
*/
void tx_xfrag_pool_init(void *poolbase, UINT64 pool_len, int num_of_bufs);
tx_xfrag_t* tx_xfrag_alloc(int mss);
void tx_xfrag_free(tx_xfrag_t *xf);


#endif