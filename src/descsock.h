/*
 * $F5Copyright_C:
 * Copyright (C) F5 Networks, Inc. 2018
 *
 * No part of the software may be reproduced or transmitted in any
 * form or by any means, electronic or mechanical, for any purpose,
 * without express written permission of F5 Networks, Inc. $
 *
 * Descriptor Socket Network Interface Driver
 *
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>

#include "sys/types.h"
#include "fixed_queue.h"
#include "err.h"
#include "xfrag_mem.h"

#define DESCSOCK_POLL_USEC          (50)
#define DESCSOCK_NOMINAL_BITRATE    (120000000000ULL)
#define DESCSOCK_MAX_QOS_TIERS      (4)
#define DESCSOCK_MTU                (1 << 13) /* 8K */

#define ETHER_CRC_LEN               4

/*
 * QoS selection TBD
 */
#define DESCSOCK_QSEL(_tx, _qos)        (((_tx)?1:0) | (((_qos) & (DESCSOCK_MAX_QOS_TIERS - 1)) << 1))
#define DESCSOCK_TX_FROM_QSEL(_qsel)    ((_qsel) & 1)
#define DESCSOCK_QOS_FROM_QSEL(_qsel)   ((_qsel) >> 1)
#define DESCSOCK_PATH_MAX               256

/* DMA rings  metadata */
#define RING_SIZE                           (256) /* XXX make dynamic? */
#define RING_MASK                           (RING_SIZE - 1)
#define RING_WRAP(x)                        ((x) & RING_MASK)
/*
 * DMA mem the library will allocate Rx, Tx DMA operations
 */
#define DESCSOCK_DMA_MEM_SIZE   64


#define DESCSOCK_MAX_TX_XFRAGS_PER_PACKET   5
#define NUM_TIERS                           4
#define DESCSOCK_MAX_PER_POLL               32
#define DESCSOCK_MAX_PER_SEND               64

typedef enum {
    RX_RET    = 1,
    TX_BUF    = 2,
} desc_type_t;

typedef struct {
    /* buffer info */
    UINT64  addr     : 48,
            len      : 16;
} empty_buf_desc_t;

typedef struct {
    /* 0x00 */
    UINT64 addr     : 48,
        len         : 16;

    UINT32 type     : 3,
        eop         : 1,
        sop         : 1,
        flags       : 16,
        did         : 11;

    UINT32 sep      : 4,
        dm          : 2,
        svc         : 14,
        nti         : 12;

    /* 0x10 */
    UINT32 tc       : 8,
        wl_hits     : 5,
                    : 7,
        dos_vec     : 8,
        dos_act     : 3,
        dos_wl      : 1;

    UINT32 epva_hash    : 24,
        spva_res        : 8;

    UINT32 lro_hints    : 20,
                        : 12;

    UINT32 resv3;
} laden_buf_desc_t;

/* Ring metadata */
#define EMPTY_DESC_LEN              sizeof(empty_buf_desc_t)
#define LADEN_DESC_LEN              sizeof(laden_buf_desc_t)

#define EMPTY_DESC_RING_SIZE        (EMPTY_DESC_LEN * RING_SIZE)
#define EMPTY_DESC_RING_MASK        (EMPTY_DESC_RING_SIZE - 1)
#define EMPTY_DESC_RING_WRAP(x)     ((x) & EMPTY_DESC_RING_MASK)

#define LADEN_DESC_RING_SIZE        (LADEN_DESC_LEN * RING_SIZE)
#define LADEN_DESC_RING_MASK        (LADEN_DESC_RING_SIZE - 1)
#define LADEN_DESC_RING_WRAP(x)     ((x) & LADEN_DESC_RING_MASK)

#define MASTER_SOCKET     "/config/dmaa_doorbell.sock"

#define DESCSOCK_SET_DID(x) (x & ((1 << 9) -1))


typedef struct  {
    struct xfrag *xf;
    UINT64 phys;
} rx_extra_t;

/* Store extra info about packet data here, for cleaning packets later on Rx */
typedef struct  {
    rx_extra_t extra[RING_SIZE];
    UINT16 prod_idx;
    UINT16 cons_idx;
    /* Indecies used for keeping track of return packet virtual local virt addresses */
    UINT16 rx_cons_idx;
    UINT16 rx_prod_idx;
}rx_extra_fifo_t;

typedef struct {
    laden_buf_desc_t    *desc;
    struct packet       *pkt;
    struct xfrag        *xf;
} tx_completions_ctx_t;

typedef struct {
    char    c[LADEN_DESC_RING_SIZE];
    UINT16  cons_idx;
    UINT16  prod_idx;
} laden_desc_fifo_t;

typedef struct {
    char    c[EMPTY_DESC_RING_SIZE];
    UINT16  cons_idx;
    UINT16  prod_idx;
} empty_desc_fifo_t;

typedef struct {
    UINT64  pkt_tx;
    UINT64  pkt_rx;
    UINT64  pkt_drop;
    UINT64  pkt_cleaned;
    UINT64  pkt_count;
    UINT64  free_desc_count;
    UINT64  rx_descs;
    UINT64  tx_descs;
    UINT64  tx_jumbos;
    UINT64  rx_jumbos;
} descsock_stats_t;

/*
 * RX queue
 */
struct descsock_rx {
    /* Unix domain socket file descriptors  for Rx */
    int                                         socket_fd[NUM_TIERS];

    /* Socket facing byte fifos */
    laden_desc_fifo_t                           inbound_descriptors[NUM_TIERS];
    empty_desc_fifo_t                           outbound_descriptors[NUM_TIERS];

    /* Rx packet queues */
    rx_extra_fifo_t                             pkt_extras[NUM_TIERS];
    FIXEDQ(, laden_buf_desc_t *, RING_SIZE)     complete_pkt[NUM_TIERS];
    FIXEDQ(, struct packet *, RING_SIZE)        rx_pkt_queue;
};

/*
 * Tx Queue
 */
struct descsock_tx {
    /* Unix domain socket file descriptors  for Tx */
    int                                         socket_fd[NUM_TIERS];

    /* Socket facing byte fifos */
    empty_desc_fifo_t                           inbound_descriptors[NUM_TIERS];
    laden_desc_fifo_t                           outbound_descriptors[NUM_TIERS];

    /* This queue is for keeping track of sent packet for future reclying upon sent completion */
    FIXEDQ(, tx_completions_ctx_t, RING_SIZE)   completions[NUM_TIERS];
};

typedef struct {
    char    path[DESCSOCK_PATH_MAX];
    void    *base;
    UINT64  len;
} dma_region_t;

/* Device instance structure. */
struct descsock_softc {

    int                         master_socket_fd;
    int                         sock_fd[DESCSOCK_MAX_QOS_TIERS * 2];
    int                         n_qos;
    int                         sep;
    UINT32                      timer_ticks;
    UINT64                      polls, idle;
    descsock_stats_t            stats;

    struct descsock_rx          rx_queue;
    struct descsock_tx          tx_queue;

    BOOL                        descsock_l2_override;
    dma_region_t                dma_region;
  };

/*
 * ifnet interface functions
 */
err_t descsock_setup(void);
err_t descsock_teardown(void);

/*
 * library client API
 */
BOOL descsock_poll(int event_mask);
int descsock_init(int argc, char *dma_shmem_path, char *mastersocket, int svc_id);
int descsock_send(void *handle, UINT32 len);
int descsock_recv(void *buf, UINT32 len, int flag);


/* Forward declaration.  Actual struct only used in if_descsock.c */
struct descsock_softc;