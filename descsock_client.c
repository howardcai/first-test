#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "src/descsock.h"
#include "src/kern/sys.h"
#include "src/kern/sys.h"
#include "src/net/xfrag_mem.h"

#include "descsock_client.h"

#define FAILED 1

static struct client_config {
    char *dma_shmem_path;
    char *master_socket_path;
    int svc_id;
} config = {
    .svc_id = 0,
};

void* init_descsock_lib(void);

void *
init_descosck_lib() {
    int ret;
    //char argv[2][DESCSOCK_PATHLEN];
    //snprintf(argv[0], DESCSOCK_PATHLEN, "--hugepages_path %s", spec->dma_shmem_path);
    //snprintf(argv[1], DESCSOCK_PATHLEN, "--mastersocket %s", spec->master_socket_path);

    //printf("a: %s b: %s\n", a, b);

    ret = descsock_init(0, config.dma_shmem_path, config.master_socket_path, config.svc_id);
    if(ret == FAILED) {
        printf("Failed to init descsock framework\n");
        exit(EXIT_FAILURE);
    }


    return NULL;
}

int
descsock_client_open(descsock_client_spec_t * const spec, const int flags)
{


    pthread_t thread_id;
    int ret;

    config.dma_shmem_path = strdup(spec->dma_shmem_path);
    config.master_socket_path = strdup(spec->master_socket_path);
    config.svc_id = spec->svc_id;


    ret = pthread_create(&thread_id, NULL, &init_descosck_lib, NULL);
    if(ret != 0) {
        perror("starting thread ");
        exit(EXIT_FAILURE);
    }

    printf("Initializing descsock library\n");

    sleep(3);




    return 1;
}

int
descsock_client_poll(int event_mask)
{
    sys_usage();
    return 0;
}

ssize_t
descsock_client_send(const void * const buf, const uint64_t len, const int flags)
{
    return 0;
}

ssize_t
descsock_client_recv(void * const buf, const uint64_t len, const int flags)
{
    return 0;
}

int
descsock_client_cntl(const int cmd, ...)
{
    return 0;
}

int
descsock_client_close(void)
{
    return 0;
}
