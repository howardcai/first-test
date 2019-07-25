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

#define FAILED -1

static struct client_config {
    char *dma_shmem_path;
    char *master_socket_path;
    int svc_id;
} config = {
    .svc_id = 0,
};

static int init_descosck_lib(void);

static int
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


    return ret;
}

int
descsock_client_open(descsock_client_spec_t * const spec, const int flags)
{


    //pthread_t thread_id;
    int ret = 1;

    config.dma_shmem_path = strdup(spec->dma_shmem_path);
    config.master_socket_path = strdup(spec->master_socket_path);
    config.svc_id = spec->svc_id;


    printf("Initializing descsock library\n");
    //ret = pthread_create(&thread_id, NULL, &init_descosck_lib, NULL);
    ret = init_descosck_lib();
    if(ret == FAILED) {
        perror("starting thread ");
        exit(EXIT_FAILURE);
    }

    //pthread_join(thread_id, NULL);
    sleep(3);


    return ret;
}

descsock_client_tx_buf_t* descsock_client_alloc_buf()
{
    return (descsock_client_tx_buf_t*) descsock_alloc_tx_xfrag();
}

void descsock_client_free_buf(descsock_client_tx_buf_t *buf)
{
    descsock_free_tx_xfrag((void* )buf);
}

int
descsock_client_poll(int event_mask)
{
    // call descsock_poll to read rx sockets for any descriptor
    //descsock_poll()
    return 0;
}

ssize_t
descsock_client_send(descsock_client_tx_buf_t *buf, const uint64_t len, const int flags)
{
    // call descsock_ifoutput sending buf to dma agent
    int sent = descsock_send((void *)buf, len);

    return sent;
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
    free(config.dma_shmem_path);
    free(config.master_socket_path);

    descsock_teardown();

    return 0;
}
