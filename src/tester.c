#include <stdio.h>
#include <stdlib.h>

#include "descsock.h"
#include "kern/sys.h"
#include "kern/sys.h"

int main(int argc, char * argv[]) {

    printf("test compile success\n");

    struct descsock_softc *sc = descsock_init();


    return EXIT_SUCCESS;
}