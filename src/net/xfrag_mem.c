
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "../sys/types.h"
#include "../kern/sys.h"

struct xfrag_item {
    SLIST_ENTRY(xfrag_item) next;
    UINT64  addr;
    UINT32  len;
    BOOL    locked;
};

GLOBALSET SLIST_HEAD(buf_stack, xfrag_item) stack;

void xfrag_pool_init(void)
{
    SLIST_INIT(&stack);

}

int main() {


    return EXIT_SUCCESS;
}