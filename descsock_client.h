/*
 * Copyright (C) F5 Networks, Inc. 2019
 *
 * No part of the software may be reproduced or transmitted in any form or by
 * any means, electronic or mechanical, for any purpose, without express
 * written permission of F5 Networks, Inc.
 */
#ifndef _DESCSOCK_CLIENT_H
#define _DESCSOCK_CLIENT_H

/*
 * ======================= DEPENDENCIES ========================
 *
 * Requires <errno.h>, <stdint.h>, XXX - probably other headers, document
 * all prerequisite includes for including this header here.
 */
#include <stdint.h>
#include <errno.h>


/*
 * Maximum length of a file path parameter or field used with this library.
 */
#define DESCSOCK_PATHLEN    512
#define DESCSOCK_BUF_SIZE   2048

/*
 * ======================= GENERAL USAGE ========================
 *
 *  1)  Call descsock_client_open() with an descsock_client_spec_t defining
 *      client requirements, as well as any flags. The client library will
 *      create a thread in the caller's process which manages the DMA Agent
 *      SEP opened based on the client spec, and buffers frames received
 *      from the DMA Agent over the SEP and sends frames from the client
 *      calls to descksock_send().
 *  2)  Use descsock_poll() or descsock_client_send()/_recv(), depending on
 *      the blocking / nonblocking requirement of your program's IO
 *      model, to check for, send, and receive Ethernet frames,
 *      respectively.
 *  3)  Call descsock_client_close() to terminate the client connection
 *      with the DMA Agent.
 */

/*
 * This structure defines public statistics exported by the client
 * library through an extern variable (see descsock_stats extern below).
 */
typedef struct {
    /*
     * Rx packets received from DMA Agent into library thread.
     */
    uint64_t    rx_packets_in;
    /*
     * Rx packets output from library thread into client (after
     * calling descsock_client_recv()). The difference between rx_packets_in
     * and rx_packet_out is the number of packets currently buffered
     * by the library worker thread.
     */
    uint64_t    rx_packets_out;
    /*
     * Rx bytes received from DMA Agent into library worker thread.
     */
    uint64_t    rx_bytes_in;
    /*
     * Rx bytes output from library worker thread into client (as a result of
     * calling descsock_client_recv()).
     */
    uint64_t    rx_bytes_out;
    /*
     * Flow control (fc) drops on Rx. The client library may drop packets if
     * there is not enough buffer space to store the frame received from the DMA
     * Agent because the client is not calling descsoc_recv() in a lively manner.
     */
    uint64_t    rx_fc_drops;
    /*
     * Tx packets accepted from client into library worker thread
     * (as a result of calling descsoc_send()).
     */
    uint64_t    tx_packets_in;
    /*
     * Tx packets output from library worker thread into DMA Agent.
     */
    uint64_t    tx_packets_out;
    /*
     * Tx bytes accepted from client into library worker thread.
     */
    uint64_t    tx_bytes_in;
    /*
     * Tx bytes output from client library worker thread into DMA Agent
     * (as a result of calling descsoc_send()).
     */
    uint64_t    tx_bytes_out;
} descsock_client_stats_t;


/*
 * This extern pointer is set and managed by the descsock client library internally, for client
 * consumption, and the pointed-to data updated in lively manner by the library worker thread.
 */
extern volatile const descsock_client_stats_t *descsock_stats;

typedef struct {
    void *base;
    uint32_t idx;
    uint32_t len;
} descsock_client_tx_buf_t;

typedef struct {
    /*
     * This specifies the path to the shared memory file to be used to buffer
     * DMA operations - the descsock client library will create this file and
     * manage its geometry. If the file exists, it will be zeroed and
     * truncated or extended to meet the requirements of the library.
     * Preferably, this path resides under a 2MB-pagesize hugetlbfs mount, but
     * must have at least (XXX - TBD) MB of free space.
     */
    char    dma_shmem_path[DESCSOCK_PATHLEN];
    /*
     * This is the path to the DMA Agent or CPProxy master domain socket.
     */
    char    master_socket_path[DESCSOCK_PATHLEN];
    /*
     * This specifies the Service ID the library will attempt to register
     * this client with.
     */
    int     svc_id;
} descsock_client_spec_t;


/*
 * Flag bits for open / send / recv @flags argument.
 */
#define DESCOCK_NONBLOCK    (1 << 0)


/*
 * Poll event bits requested in @event_mask of calls to descsock_poll(), and
 * returned by descsock_poll() if requested condition(s) are met.
 */
#define DESCSOCK_POLLIN     (1 <<  0)   // There is data to receive.
#define DESCSOCK_POLLOUT    (1 <<  1)   // Sending now would not block.
#define DESCSOCK_POLLERR    (1 << 31)   // Error condition (return only).


/*
 * Opens a new tenant datapath connection into the DMA agent, as specified
 * by @spec (see definition of descock_client_spec_t above), and starts
 * the client library worker thread. The @flags argument can be used
 * to set default flags applied to all subsequent descsock_client_send() and
 * descosck_recv() calls.
 *
 * Only a single SEP is opened, and only Rx/Tx QoS 0 on that SEP will be
 * operated by the client library. As a result of calling this function, the
 * client library creates a new thread in the caller's context which manages
 * the client's SEP, providing empty buffer descriptors to the DMA Agent and
 * buffering returned laden descriptors until the user calls descsock_client_recv(),
 * as well as sending buffers supplied in descsock_client_send().
 *
 * At present, this library does not support tenants in VMs using VirtIO
 * serial sockets.
 *
 * Returns 0 on success, -1 on failure and errno will be set appropriately.
 */
int descsock_client_open(descsock_client_spec_t * const spec, const int flags);


/*
 * Takes an @event_mask of DESCSOCK_POLL* events and checks if the current
 * descsock state meets the condition of the requested events.
 *
 * Returns a mask of event conditions which are satisfied and in the @event_mask,
 * or DESCSOCK_POLLERR (-1) if an error is encountered.
 */
int descsock_client_poll(int event_mask);


/*
 * Takes a @buf and a corresponding @len and queues it for sending by the
 * library worker thread over the DMA Agent SEP associated with this process.
 * @flags specifies any flags which override the defaults specified in
 * descsock_client_open().
 *
 * Note that @buf must point to a complete Ethernet frame, including 4 trailing
 * empty bytes for the CRC. This is required by hardware. XXX - working to have
 * hardware team rectify this behavior.
 *
 * Returns 0 on success, -1 on failure and errno will be set appropriately.
 *
 * If errno is EWOULDBLOCK, and the DESCSOCK_NONBLOCK flag is set, then there is
 * back-pressure on the transmit descriptor socket (no room to send).
 */
ssize_t descsock_client_send(descsock_client_tx_buf_t* buf, const uint64_t len, const int flags);


/*
 * Takes a @buf and a corresponding @len and copies into it the next ready frame
 * buffered by the library worker thread from the DMA Agent SEP associated
 * with this process. @flags specifies any flags which override the defaults
 * specified in descsock_client_open().
 *
 * Note that if data is received, @buf will point to a complete Ethernet
 * frame, including 4 trailing empty bytes for the CRC. This is required by
 * hardware. XXX - working to have hardware team rectify this behavior.
 *
 * Returns 0 on success, -1 on failure and errno will be set appropriately.
 *
 * If errno is EWOULDBLOCK, and the DESCSOCK_NONBLOCK flag is set, then no
 * complete packets have been buffered internally by the client library
 * worke thread (no data to recv).
 */
ssize_t descsock_client_recv(void * const buf, const uint64_t len, const int flags);


/*
 * This is to provide a conceptual analogue to fnctl on a socket. Currently no
 * commands (@cmd) are implemented, but this is defined for future uses like
 * setting advanced options, etc, as a general way to achieve mostly
 * backwards-compatible / non-breaking extensions to the API.
 *
 * Returns 0 on success, -1 on failure and errno will be set appropriately.
 */
int descsock_client_ctrl(const int cmd, ...);

descsock_client_tx_buf_t* descsock_client_alloc_buf();
void descsock_client_free_buf(descsock_client_tx_buf_t *buf);


/*
 * This cleanly closes the connection to the DMA Agent associated with this
 * process / library worker thread, unmaps any shared memory, and frees any
 * internal resources.
 *
 * Returns 0 on success, -1 on failure and errno will be set appropriately.
 */
int descsock_client_close(void);

#endif
