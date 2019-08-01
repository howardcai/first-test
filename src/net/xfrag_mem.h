#ifndef XBUF_H
#define XBUF_H
#include <sys/queue.h>

#include "../sys/types.h"

#define XFRAG_SIZE 2048
// #if (XFRAG_SIZE > 65535)
// #error "XFRAG_SIZE too big"
// #endif

// #define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

// /* fence posts */
// #define XFRAG_MAGIC (*((UINT32 *)"FRAG"))
// #define XFRAG_FREE  (*((UINT32 *)"FREE"))
// #define XFRAG_POST 0x5555

// #define XFRAG_POST_SIZE (sizeof(UINT32))
// #define XFRAG_BUF_SIZE (XFRAG_SIZE - offsetof(struct xfrag, base) - XFRAG_POST_SIZE)

// /* Get the base of the data buffer. Account for the mss. */
// #define xfrag_base_mss(xf,mss) (((BYTE *)(xf)) + sizeof(struct xfrag) + XFRAG_BUF_SIZE - (mss))




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

void xfrag_pool_init(void *pool_base, UINT64 pool_len, int num_of_bufs);
void xfrag_pool_free(void);
struct xfrag * xfrag_alloc(bool rx);
void xfrag_free(struct xfrag *xf, bool rx);



#endif