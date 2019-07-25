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
 * $Id: //depot/tmos/core-ce-platform/tmm/dev/descsock/if_descsock.h#4 $
 */
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>

#include "sys/types.h"
#include "sys/fixed_queue.h"
#include "kern/err.h"
#include "net/xfrag_mem.h"

#define DESCSOCK_POLL_USEC          (50)
#define DESCSOCK_NOMINAL_BITRATE    (120000000000ULL)
#define DESCSOCK_MAX_QOS_TIERS      (4)
#define DESCSOCK_MTU                (1 << 13) /* 8K */

#define ETHER_CRC_LEN               4


/*
 * QoS selection TBD
 */
#define DESCSOCK_QSEL(_tx, _qos)    (((_tx)?1:0) | (((_qos) & (DESCSOCK_MAX_QOS_TIERS - 1)) << 1))
#define DESCSOCK_TX_FROM_QSEL(_qsel)    ((_qsel) & 1)
#define DESCSOCK_QOS_FROM_QSEL(_qsel)   ((_qsel) >> 1)
#define DESCSOCK_PATH_MAX           (256)

#define DESCSOCK_DEBUG_PRINT 0
#define DESCSOCK_DEBUGF(fmt, rest...) ({if(DESCSOCK_DEBUG_PRINT) { printf("%s():%d " fmt "\n", __FUNCTION__, __LINE__, ##rest); }})
#define DESCSOCK_LOG(fmt, rest...) printf("descsock: " fmt "\n", ##rest);

/* TMM dma driver queue metadata */
#define RING_SIZE                           (256) /* XXX make dynamic? */
#define RING_MASK                           (RING_SIZE - 1)
#define RING_WRAP(x)                        ((x) & RING_MASK)
#define DESCSOCK_MAX_TX_XFRAGS_PER_PACKET   10
#define NUM_TIERS                           4


#define DESCSOCK_MAX_PER_POLL               32
#define DESCSOCK_MAX_PER_SEND               (64)

/* Add more flags for ePVA, sPVA, DDoS, etc. as needed. */
#define DESCSOCK_FLAG_VIRT_ADDR     (1)
#define DESCSOCK_FLAG_PHYS_ADDR     (2)
#define DESCSOCK_FLAG_RUNTIME_DEBUG (4)
#define DESCSOCK_FLAG_FAKE_PCI      (8)

// #define EPOLLIN 0x001
// /* Valid opcodes ( "op" parameter ) to issue to epoll_ctl().  */
// #define EPOLL_CTL_ADD 1	/* Add a file descriptor to the interface.  */
// #define EPOLL_CTL_DEL 2	/* Remove a file descriptor from the interface.  */
// #define EPOLL_CTL_MOD 3	/* Change file descriptor epoll_event structure.  */

/* To store bufs when reading and writting to sockets*/
// struct iovec {
//     void *iov_base;	/* Pointer to data.  */
//     UINT64 iov_len;	/* Length of data.  */
// };

// typedef union epoll_data {
//   void *ptr;
//   int fd;
//   uint32_t u32;
//   uint64_t u64;
// } epoll_data_t;

// struct epoll_event {
//   uint32_t events;	/* Epoll events */
//   epoll_data_t data;	/* User data variable */
// };

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
#define MADC_PLATFORM       "Z102"
#define PADC_PLATFORM       "Z101"
#define DESCSOCK_SET_DID(x) (x & ((1 << 9) -1))


typedef struct  {
    void *frag;
    void *xdata;
    UINT64 phys;
    rx_xfrag_t *xf;

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
    void *base;
    UINT32 len;
    UINT64 idx;
} client_tx_buf_t;

typedef struct {
    laden_buf_desc_t    *desc;
    client_tx_buf_t      *pkt;
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
 * XXX: Since TMM runs per thread/cpu core, which means each SEP will get a
 * TMM thread lets put a temp number of max cpus here, we'll need to dynamically
 * obtain this later with hudconf.ncpu
 */
#define DESCSOCK_MAX_CPU        2

#define VIRTIO_DMA_MASTER "dma-master"

struct tx_context {
    SLIST_ENTRY(tx_context)     next;
    tx_xfrag_t                  *tx_xfrag;
    client_tx_buf_t             *client_buf;
    laden_buf_desc_t            *desc;
    UINT64                      idx;
};
struct tx_entry {
    SLIST_ENTRY(tx_entry) entry;
    tx_xfrag_t *xf;
    client_tx_buf_t *buf;
    UINT64 idx;
};

typedef enum {
    PADC_MODE,
    MADC_MODE,
} descsock_mode_t;

typedef struct {
    int rx_fd[NUM_TIERS];
    int tx_fd[NUM_TIERS];
} descsock_sep_t;


struct descsock_rx {
    laden_desc_fifo_t                           inbound_descriptors[NUM_TIERS];
    empty_desc_fifo_t                           outbound_descriptors[NUM_TIERS];
    rx_extra_fifo_t                             pkt_extras[NUM_TIERS];
    FIXEDQ(, laden_buf_desc_t *, RING_SIZE)     complete_pkt[NUM_TIERS];
    int                                         socket_fd[NUM_TIERS];
};

struct descsock_tx {
    /* Sockets for sending send descriptors */
    int                                         socket_fd[NUM_TIERS];
    /* socket facing byte fifos */
    empty_desc_fifo_t                           inbound_descriptors[NUM_TIERS];
    laden_desc_fifo_t                           outbound_descriptors[NUM_TIERS];

    FIXEDQ(, client_tx_buf_t, RING_SIZE)        client_buf_stack;
    FIXEDQ(, tx_completions_ctx_t, RING_SIZE)   completions[NUM_TIERS];
    SLIST_HEAD(tx_ctx, tx_context)              tx_ctx_listhead;

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

    BOOL                                        descsock_l2_override;

    dma_region_t               dma_region;
    descsock_mode_t            mode;

  };


/*
 * device interface functions
 */
BOOL descsock_probe(f5dev_t dev);
f5device_t *descsock_attach(f5dev_t dev);
void descsock_detach(f5device_t *devp);
BOOL descsock_poll(struct dev_poll_param *param, f5device_t *devp);

/*
 * ifnet interface functions
 */
// static err_t descsock_ifup(struct ifnet *ifp);
// static err_t descsock_ifdown(struct ifnet *ifp);
err_t descsock_ifoutput(struct descsock_softc *sc, client_tx_buf_t *buf);

int descsock_init(int argc, char *dma_shmem_path, char *mastersocket, int svc_id);
err_t descsock_setup(struct descsock_softc *sc);
err_t descsock_teardown();


/*
 * library client API
 */
int descsock_send(void *handle, UINT64 len);
int descsock_recv(struct descsock_softc *sc);

client_tx_buf_t* descsock_alloc_tx_xfrag();
void descsock_free_tx_xfrag(void *handle);



/* Forward declaration.  Actual struct only used in if_descsock.c */
struct descsock_softc;