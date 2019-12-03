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
    char tenant_name[DESCSOCK_CLIENT_PATHLEN];
} config = {
    .svc_id = 0,
};

//descsock_client_stats_t  *descsock_client_stats;

static int init_descosck_lib(void);

/*
 * Start library
 * Returns 1 on success, -1 on failure
 */
static int
init_descosck_lib() {
    int ret;

    /* Initialize descsock library */
    ret = descsock_init(0, config.dma_shmem_path, config.master_socket_path,
            config.svc_id, config.tenant_name);

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
    strcpy(config.tenant_name, spec->tenant_name);
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
        /* Set flag indicating data is available */
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
    int sent = descsock_send(NULL, buf, len);
    if(sent != 0) {
        descsock_client_stats.tx_bytes_in += len;
        descsock_client_stats.tx_packets_in++;
    }

    return sent;
}

/*
 * Receive a packet into buf
 * Returns number of bytes read, -1 on failure
 */
ssize_t
descsock_client_recv(void *buf, const uint64_t len, const int flags)
{
    int read_bytes = descsock_recv(buf, DESCSOCK_CLIENT_BUF_SIZE, 0);
    if(read_bytes != 0) {
        descsock_client_stats.rx_bytes_out += read_bytes;
        descsock_client_stats.rx_packets_out++;
    }
    return read_bytes;
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

/*
 * Set IFH fields in send descriptor
 */
ssize_t
descsock_client_send_extended(dsk_ifh_fields_t *ifh, void *buf, const uint64_t len, const int flags)
{
    int written = descsock_send(ifh, buf, len);
    if(written != 0) {
        descsock_client_stats.tx_bytes_in += len;
        descsock_client_stats.tx_packets_in++;
    }

    return written;
}