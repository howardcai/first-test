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
#ifndef _DESCSOCK_H
#define _DESCSOCK_H

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
#define DESCSOCK_PATH_MAX               (1024)
#define DESCSOCK_MSG_MAX                (2048 + DESCSOCK_PATH_MAX)

/* DMA rings  metadata */
#define RING_SIZE                           (1024) /* XXX make dynamic? */
#define RING_MASK                           (RING_SIZE - 1)
#define RING_WRAP(x)                        ((x) & RING_MASK)
/*
 * DMA mem the library will allocate Rx, Tx DMA operations
 */
#define DESCSOCK_DMA_MEM_SIZE               (1ULL << 30) /* 1Gi */


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

/* Laden Rx Return descriptor flags. */
typedef union {
    struct {
        UINT8  l3csum_ok    : 1,
            l4csum_ok       : 1,
            fsu             : 1,
            directed        : 1,
            dlf             : 1,
            mcast           : 1,
                            : 2;
    };
    UINT8     u8;
} laden_rx_desc_flags_t;

/* Laden Tx Send descriptor flags. */
typedef union {
    struct {
        UINT8  repl_l3csum   : 1,
            repl_l4csum      : 1,
            snoop            : 1,
            directed         : 1,
            wait             : 1,
                             : 3;
    };
    UINT8     u8;
} laden_tx_desc_flags_t;

typedef union {
    laden_rx_desc_flags_t   rx;
    laden_tx_desc_flags_t   tx;
} laden_desc_flags_t;

typedef struct {
    UINT64 addr     : 48,
        len         : 16;   // 0x08

    UINT32 type     :  3,
                    :  1,
        host        :  1,
        sop         :  1,
        eop         :  1,
        err         :  1,
        flags       :  8,
        did         : 11,
                    :  5;   // 0x0C

    UINT32 sep      :  4,
        dm          :  2,
        svc         : 14,
        nti         : 12;   // 0x10

    UINT32            _r[3];   // 0x20
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

typedef enum {
    DESCSOCK_UP,
    DESSOCK_DOWN,
    DESCSOCK_FULL,
    DESCSOCK_FAILED,
    DESCSOCK_RESTART
}descsock_state_t;

/* Device instance structure. */
struct descsock_softc {
    int                         master_socket_fd;
    int                         sock_fd[DESCSOCK_MAX_QOS_TIERS * 2];
    int                         n_qos;
    int                         sep;
    descsock_state_t            state;
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
int descsock_init(int argc, char *dma_shmem_path, char *mastersocket, int svc_id, char *tenant_name);
int descsock_send(dsk_ifh_fields_t *ifh, void *buf, UINT32 len);
int descsock_recv(void *buf, UINT32 len, int flag);
BOOL descsock_state_full(void);
BOOL descsock_state_err(void);



#endif