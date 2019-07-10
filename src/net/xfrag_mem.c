
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "../sys/types.h"
#include "../kern/sys.h"
#include "xfrag_mem.h"



GLOBALSET SLIST_HEAD(buf_stack, xfrag_item) xfrag_stack;

void xfrag_pool_init(void)
{
    SLIST_INIT(&xfrag_stack);

}

void *xfrag_alloc(void)
{
    return NULL;
}

void xfrag_free(struct xfrag_item *xf)
{

}

int main() {


    xfrag_pool_init();

    printf("Success\n");
    return EXIT_SUCCESS;
}