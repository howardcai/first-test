#include <stdio.h>
#include <stdlib.h>

#include "src/descsock.h"
#include "src/kern/sys.h"
#include "src/kern/sys.h"
#include "src/net/xfrag_mem.h"

int main(int argc, char *argv[]) {


    struct descsock_softc *sc = descsock_init(argc, argv);


    struct xfrag_item *buf = xfrag_alloc();



    xfrag_free(buf);



    descsock_teardown(sc);

    printf("test compile success\n");
    return EXIT_SUCCESS;
}