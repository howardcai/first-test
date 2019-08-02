/*
 * $F5Copyright_C:
 * Copyright (C) F5 Networks, Inc. 2018
 *
 * No part of the software may be reproduced or transmitted in any
 * form or by any means, electronic or mechanical, for any purpose,
 * without express written permission of F5 Networks, Inc. $
 *
 * Descriptor Socket Network Interface Driver Framework
 *
 *
 */

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
#include <stdbool.h>

#include "descsock.h"
#include "sys.h"
#include "sys.h"
#include "xfrag_mem.h"

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

    ret = descsock_init(0, config.dma_shmem_path, config.master_socket_path, config.svc_id);
    if(ret == FAILED) {
        printf("Failed to init descsock framework\n");
        exit(EXIT_FAILURE);
    }

    return ret;
}

/* XXX: Implemented multi-thread */
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

int
descsock_client_poll(int event_mask)
{
    /* Check for received packets */
    bool ret = descsock_poll(event_mask);

    if(ret) {
        /* got a packet */
        return 1;
    }

    return 0;
}

/* Send a buf to the descsock framework */
ssize_t
descsock_client_send(void *buf, const uint64_t len, const int flags)
{
    /* call descsock_ifoutput sending buf to dma agent */
    int sent = descsock_send((void *)buf, len);

    return sent;
}

ssize_t
descsock_client_recv(void *buf, const uint64_t len, const int flags)
{
    return descsock_recv(buf, DESCSOCK_BUF_SIZE, 0);
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
