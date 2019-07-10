#include <stdio.h>
#include <stdlib.h>

#include "src/descsock.h"
#include "src/kern/sys.h"
#include "src/kern/sys.h"

int main(int argc, char * argv[]) {

    printf("test compile success\n");

    struct descsock_softc *sc = descsock_init();



    return EXIT_SUCCESS;
}