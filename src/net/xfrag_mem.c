
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>

#include "../sys/types.h"
#include "../kern/sys.h"
#include "xfrag_mem.h"


static struct xfrag_item* xfrag_stack_pop(void);
static void xfrag_stack_push(struct xfrag_item *xf);
static err_t xfrag_stack_remove(struct xfrag_item *xf);

GLOBALSET SLIST_HEAD(buf_stack, xfrag_item) xfrag_stack;

void
xfrag_pool_init(void)
{
    int i;
    struct xfrag_item *xf;
    SLIST_INIT(&xfrag_stack);

    for(i = 0; i < 10; i++) {
        xf = malloc(sizeof(struct xfrag_item));
        xf->base = malloc(BUF_SIZE);
        xf->locked = FALSE;

        xfrag_stack_push(xf);
    }
}

void *
xfrag_alloc(void)
{
    struct xfrag_item *xf = xfrag_stack_pop();
    xf->locked = TRUE;

    return xf;
}

void xfrag_free(struct xfrag_item *xf)
{
    if(xf->locked) {
        xf->locked = FALSE;
        //free(xf->base);
        //xf->base = NULL;
        memset(xf->base, 0, BUF_SIZE);

        xfrag_stack_push(xf);
    }
}

static struct xfrag_item *
xfrag_stack_pop(void)
{
    // XXX: validate not empty
    struct xfrag_item *xf = SLIST_FIRST(&xfrag_stack);
    SLIST_REMOVE_HEAD(&xfrag_stack, next);

    return xf;
}

static void
xfrag_stack_push(struct xfrag_item *xf)
{
    SLIST_INSERT_HEAD(&xfrag_stack, xf, next);
}

static err_t
xfrag_stack_remove(struct xfrag_item *xf)
{
    // XXX: validate xf or xf->base is not null for double free error

    free(xf->base);
    xf->base = NULL;

    free(xf);

    return ERR_OK;
}
// int main() {


//     xfrag_pool_init();

//     printf("Success\n");
//     return EXIT_SUCCESS;
// }