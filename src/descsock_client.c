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

#include "descsock_client.h"
#include "descsock.h"


static struct client_config {
    char dma_shmem_path[DESCSOCK_CLIENT_PATHLEN];
    char master_socket_path[DESCSOCK_CLIENT_PATHLEN];
    int svc_id;
} config = {
    .svc_id = 0,
};

static int init_descosck_lib(void);

/*
 * Start library
 * Returns 1 on success, -1 on failure
 */
static int
init_descosck_lib() {
    int ret;

    /* Initialize descsock library */
    ret = descsock_init(0, config.dma_shmem_path, config.master_socket_path, config.svc_id);
    if(ret == DESCSOCK_CLIENT_FAILED) {
        printf("Failed to init descsock framework\n");
        return DESCSOCK_CLIENT_FAILED;
    }

    return ret;
}

/*
 * XXX: Implemented multi-thread
 *
 * Open descsock library with a client_spec config
 * Returns 1 on success, -1 on failure
 */

int
descsock_client_open(descsock_client_spec_t * const spec, const int flags)
{
    //pthread_t thread_id;
    int ret = 1;

    strcpy(config.dma_shmem_path, spec->dma_shmem_path);
    strcpy(config.master_socket_path, spec->master_socket_path);
    config.svc_id = spec->svc_id;


    printf("Initializing descsock library\n");
    //ret = pthread_create(&thread_id, NULL, &init_descosck_lib, NULL);
    ret = init_descosck_lib();
    if(ret == DESCSOCK_CLIENT_FAILED) {
        printf("Failed to initialize descsock library\n");
        return DESCSOCK_CLIENT_FAILED;
    }

    //pthread_join(thread_id, NULL);

    return ret;
}

/*
 * Poll Rx unix domain sockets for outstanding return descriptors
 * Returns 1 if Rx packets are available, 0 if none
 */
int
descsock_client_poll(int event_mask)
{
    int flags = 0;

    /* Check if descsock is in err state */
    bool err = descsock_state_err();
    if(err) {
        return DESCSOCK_POLLERR;
    }

    /* Check if descsock state has space for at least 9KB of outgoing packet data */
    bool full = descsock_state_full();
    if((!full) && (event_mask & DESCSOCK_POLLOUT)) {
        flags |= DESCSOCK_POLLOUT;
    }

    /* Check for received packets */

    bool readable = descsock_poll(event_mask);

    if((readable) && (event_mask & DESCSOCK_POLLIN)) {
        // There is data to be read
        flags |= DESCSOCK_POLLIN;
    }

    return flags;
}

/*
 * Send a buf
 * Returns the number of bytes writen, -1 on failure
 */
ssize_t
descsock_client_send(void *buf, const uint64_t len, const int flags)
{
    /* call descsock_ifoutput sending buf to dma agent */
    int sent = descsock_send(buf, len);

    return sent;
}

/*
 * Receive a packet into buf
 * Returns number of bytes read, -1 on failure
 */
ssize_t
descsock_client_recv(void *buf, const uint64_t len, const int flags)
{
    return descsock_recv(buf, DESCSOCK_CLIENT_BUF_SIZE, 0);
}

/* Future implementation */
int
descsock_client_cntl(const int cmd, ...)
{
    return 0;
}

/* Close descsock framework, free memory */
int
descsock_client_close(void)
{
    descsock_teardown();

    return 0;
}
