#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <sys/epoll.h>

#include "descsock_client.h"

#define SVC_ID              1
#define MASTER_SOCKET_PATH  "/run/dmaa_doorbell.sock"
#define HUGEPAGES_PATH      "/run/hugepages/"


int main(int argc, char *argv[]) {


    descsock_client_spec_t *client = malloc(sizeof(descsock_client_spec_t));


    descsock_client_open(client, 0);

    descsock_client_poll(0);


    printf("Compile success\n");




    free(client);

    return EXIT_SUCCESS;
}

