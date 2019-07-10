#ifndef XBUF_H
#define XBUF_H
#include <sys/queue.h>

#include "../sys/types.h"

struct xfrag_item {
    SLIST_ENTRY(xfrag_item) next;
    void    *base;
    UINT32  len;
    BOOL    locked;
};

void xfrag_pool_init(void);

void *xfrag_alloc(void);

void xfrag_free(struct xfrag_item *xf);


#endif