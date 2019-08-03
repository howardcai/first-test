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
#include <stdint.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "common.h"
#include "types.h"
#include "hudconf.h"
#include "fixed_queue.h"
#include "sys.h"
#include "packet.h"
#include "xfrag_mem.h"
#include "descsock_client.h"
#include "descsock.h"


/*
 * Internal driver functions
 */
static err_t descsock_ifoutput(struct packet *pkt);
static err_t descsock_config_exchange(char * dmapath);
static int descsock_establish_dmaa_conn(void);
static void descsock_partition_sockets(int socks[], int rx[], int tx[]);

/*
 * Rx
 *
 * Building Rx slots, Producer descriptors, Reading bytes/descriptors from sockets, Building packets,
 * Passing packets up the TMM stack
 */
static err_t descsock_build_rx_slot(UINT32 tier);
static inline err_t descsock_refill_rx_slots(int tier, int max);
static inline int refill_inbound_fifo_from_socket(UINT32 tx,  UINT32 qos);
static int descsock_count_pkts_from_fifo(laden_desc_fifo_t *fifo, UINT32 avail, UINT32 *pkt_count);
static inline int rx_receive_advance_producer(UINT32 tier);
static inline void rx_receive_advance_consumer(UINT32 tier);
static inline int tx_receive_advance_producer(UINT32 tier);
static inline int tx_receive_adanvace_consumer(UINT32 tier, int max);

/*
 * Tx
 *
 * Writing bytes/descriptors to sockets, Building send descriptors from egress packets
 * Saving packet references for cleaning up when completions arrive
 */
static inline int flush_bytes_to_socket(UINT32 tx, int qos);
static inline err_t tx_send_advance_producer(UINT32 tier);
static inline int tx_send_advance_consumer(UINT32 tier);
static err_t descsock_tx_single_desc_pkt(struct packet *pkt, UINT32 tier);
//err_t descsock_tx_multi_desc_pkt(struct packet *pkt, UINT32 tier);
static inline int rx_send_advance_producer(UINT16 tier, int max);
static inline int rx_send_advance_consumer(int tier);
static inline void clean_tx_completions(UINT16 tier);

/*
 * Initializing, debug, helper functions
 */
static err_t descsock_init_conn();
void descsock_print_buf(void * buf, int buf_len);
void descsock_close_fds();
UINT16 descsock_get_ethertype(struct packet *pkt);
err_t descsock_get_vlantag(struct packet *pkt, UINT16 *vlan);

/*
 * Laden/full and empty descriptors are different size so we have to have different logic for
 * computing size, when full or empty etc...
 * laden_desc_size = 32 bytes, empty_desc_size = 8 bytes
 */
BOOL laden_desc_fifo_full(laden_desc_fifo_t *fifo);
BOOL laden_desc_fifo_empty(laden_desc_fifo_t *fifo);
int laden_desc_fifo_avail(laden_desc_fifo_t *fifo);
BOOL empty_desc_fifo_full(empty_desc_fifo_t *fifo);
BOOL empty_desc_fifo_empty(empty_desc_fifo_t *fifo);
int  empty_desc_fifo_avail(empty_desc_fifo_t *fifo);

/*
 * Main structure used as core of the framework
 */
struct descsock_softc *sc;

/*
 * Start descsock framewor, Allocate DMA memory, send config message to DMAA
 * Returns 1 on success, -1 on failuer
 */
int
descsock_init(int argc, char *dma_shmem_path, char *mastersocket, int svc_id)
{
    int i;
    int ret = SUCCESS;
    err_t err = ERR_OK;
    int tier = 0;

    hudconf.hugepages_path = strdup(dma_shmem_path);
    hudconf.mastersocket = strdup(mastersocket);
    hudconf.svc_ids = svc_id;
    hudconf.memsize = DESCSOCK_DMA_MEM_SIZE;
    hudconf.num_seps = 1;
    /* size is in mb so we multiple to get mb */
    hudconf.dma_seg_size = (hudconf.memsize * 1024 * 1024);
    /* align for unix page size */
    DESCSOCK_LOG("Total mem allocated for descsock framework %lld\n", hudconf.dma_seg_size);
    hudconf.dma_seg_size = ((hudconf.dma_seg_size + PAGE_MASK) & ~PAGE_SIZE);

    sc = malloc(sizeof(struct descsock_softc));
    if(sc == NULL) {
        DESCSOCK_LOG("Error allocating sc\n");
        ret = FAILED;
        goto err_out;
    }

    /* Init xfrag buf pool and device */
    err = descsock_init_conn(sc);
    if(err != ERR_OK) {
        DESCSOCK_LOG("Error initializing connection\n");
        ret = FAILED;
        goto err_out;
    }

    /* Init queues for all tiers
     * Only using tier 0 for for now
     */
    for(i = 0; i < NUM_TIERS; i++) {
        FIXEDQ_INIT(sc->tx_queue.completions[i]);
        FIXEDQ_INIT(sc->rx_queue.complete_pkt[i]);
    }

    FIXEDQ_INIT(sc->rx_queue.rx_pkt_queue);

    /* build producer descriptors */
    DESCSOCK_LOG("Producing xdatas\n");
    i = rx_send_advance_producer(tier, RING_SIZE - 1);
    DESCSOCK_LOG("rx_send_advance producer %d\n", i);
    if(i <= 0) {
        DESCSOCK_LOG("Error pruducing xdatas\n");
    }

    /* Send empty producer descriptors */
    DESCSOCK_LOG("consuming xdatas\n");
    i = rx_send_advance_consumer(tier);
    DESCSOCK_LOG("rx_send_advance consumer %d\n", i);
    if(i <= 0) {
        DESCSOCK_LOG("Error sending producer descriptors on init");
    }

    return ret;

err_out:
    if(sc != NULL) {
        free(hudconf.hugepages_path);
        free( hudconf.mastersocket);
        free(sc);
    }

    return FAILED;
}

/*
 * Init descsock framework
 * send registration message containing this driver memory info and number SEP info
 */
err_t
descsock_init_conn()
{
    err_t err = ERR_OK;
    char msg[DESCSOCK_PATH_MAX];
    int i;

    /* Concatenate descsock.000 to path string to create hugepages path mount */
    snprintf(msg, DESCSOCK_PATH_MAX, "%s/descsock.000", hudconf.hugepages_path);

    /* Save hugepages mount info  */
    snprintf(sc->dma_region.path, DESCSOCK_PATH_MAX, "%s", msg);
    sc->dma_region.len = hudconf.dma_seg_size;

    /* try mmaping the passed in hugepages path */
    sc->dma_region.base = descsock_map_dmaregion(sc->dma_region.path, sc->dma_region.len);
    if(sc->dma_region.base == NULL) {
        DESCSOCK_LOG("Failed to map hugepages\n");
        err = ERR_CONN;
        goto err_out;
    }

    /* Create message string to send to dmaa */
    snprintf(msg, DESCSOCK_PATH_MAX, "path=%s\nbase=%llu\nlength=%llu\nnum_sep=1\npid=1\nsvc_ids=1\n\n",
             sc->dma_region.path, (UINT64)sc->dma_region.base, sc->dma_region.len);

    DESCSOCK_LOG("%s\n", msg);

    /* call the master socket here */
    sc->master_socket_fd = descsock_establish_dmaa_conn();
    if(sc->master_socket_fd < 0) {
        DESCSOCK_LOG("Failed to connect to master socket at %s", MASTER_SOCKET);
        err = ERR_CONN;
        goto err_out;
    }

    /*
     * Initialize xfrag and packet memory pool,
     */
    err = xfrag_pool_init(sc->dma_region.base, sc->dma_region.len, RING_SIZE * 2);
    if(err != ERR_OK) {
        err = ERR_MEM;
        goto err_out;
    }

    err = packet_init_pool(RING_SIZE * 2);
    if(err != ERR_OK) {
        err = ERR_MEM;
        goto err_out;
    }

    DESCSOCK_LOG("received master socket %d\n", sc->master_socket_fd);

    DESCSOCK_LOG("Sending msg to DMAA %s", msg);

    /* Send dma region path info to DMA AGENT */
    err = descsock_config_exchange(msg);
    if (err != ERR_OK) {
        DESCSOCK_LOG("Failed to write dma region to dmaa\n");
        err = ERR_CONN;
        goto err_out;
    }

    /* Set non-blocking on all TX, RX sockets */
    for(i = 0; i < NUM_TIERS; i++) {
        if (sys_set_non_blocking(sc->rx_queue.socket_fd[i], 1) != ERR_OK) {
            DESCSOCK_LOG("Error setting non_blocking on rx socket");
            err = ERR_IO;
            goto err_out;
        }
        if (sys_set_non_blocking(sc->tx_queue.socket_fd[i], 1) != ERR_OK) {
            DESCSOCK_LOG("Error setting non_blocking on tx socket");
            err = ERR_IO;
            goto err_out;
        }
    }

    return err;

err_out:
    /* Free DMA mem */
    xfrag_pool_free();

    packet_pool_free();

    return err;
}

/* Send a buf owned by descsock client */
// XXX: Return the number of bytes writen
int descsock_send(void *buf, UINT32 len)
{
    err_t err = ERR_OK;
    struct xfrag *xf;
    struct packet *pkt;
    bool rx = false;

    //XXX: Validate buf from client

    pkt = packet_alloc();
    if(pkt == NULL) {
        DESCSOCK_LOG("Failed to alloc packet\n");
        err = ERR_MEM;
        goto err_out;
    }

    xf = xfrag_alloc(rx);
    if(xf == NULL) {
        DESCSOCK_LOG("Error xfrag_allo() retunred null\n");
        err = ERR_MEM;
        goto err_out;
    }

    /* Copy packet data from user to a send buf to send in descriptor */
    memcpy(xf->data, buf, len);

    /* Save xfrag reference in packet  */
    xf->len = len;
    pkt->xf_first = xf;
    pkt->len = len;

    err = descsock_ifoutput(pkt);

    return (err == ERR_OK)? SUCCESS : FAILED;

err_out:
    return err;
}
/*
 * XXX: Return number of bytes writen
 */
int
descsock_recv(void *buf, UINT32 len, int flag)
{
    struct packet *pkt;
    bool rx = true;

    while(!FIXEDQ_EMPTY(sc->rx_queue.rx_pkt_queue)) {
        pkt = FIXEDQ_HEAD(sc->rx_queue.rx_pkt_queue);

        DESCSOCK_DEBUGF("received pkt %p with buf %p %lld len %d\n",
            pkt, pkt->xf_first->data, (UINT64)pkt->xf_first->data, pkt->len);

        memcpy(buf, pkt->xf_first->data, pkt->len);

        /* Recycle DMA bufs  */
        xfrag_free(pkt->xf_first, rx);

        packet_free(pkt);

        FIXEDQ_REMOVE(sc->rx_queue.rx_pkt_queue);
    }

    return SUCCESS;
}

err_t
descsock_teardown()
{
    free(hudconf.hugepages_path);
    free(hudconf.mastersocket);

    xfrag_pool_free();
    packet_pool_free();

    free(sc);

    /*
     * Free all allocated memory in queues
     */

    return ERR_OK;
}

BOOL
descsock_poll(int mask) {

    UINT32 work = 0;
    err_t err;
    int tier, avail, flushed_descriptors;
    struct packet *pkt;
    struct xfrag *xf;
    laden_buf_desc_t *desc;
    laden_desc_fifo_t *tx_out_fifo;

    rx_extra_fifo_t *extras_fifo;

    /*
     * Iterate through and poll all tiers
     */
    for(tier = 0; tier < DESCSOCK_MAX_QOS_TIERS; tier++) {

        /* Get fifo for current tier */
        tx_out_fifo = &sc->tx_queue.outbound_descriptors[tier];

        extras_fifo = &sc->rx_queue.pkt_extras[tier];


        /*
         * Clean all sent completions on tier
         */
        clean_tx_completions(tier);

        /*
         * Flush/empty the send ring if it has anything
         */
        avail = laden_desc_fifo_avail(tx_out_fifo);
        if(avail >= LADEN_DESC_LEN) {
            /* XXX: if flushed_bytes is -1 then we encountered an error writing to socket */

            flushed_descriptors = tx_send_advance_consumer(tier);
             DESCSOCK_DEBUGF("flushed bytes %d\n", flushed_descriptors);
            if(flushed_descriptors == -1) {
                DESCSOCK_LOG("Failed to write to socket\n");
            }
        }

        /*
         * POLL rx return ring for tier
         */
        avail = rx_receive_advance_producer(tier);


        /* if not packets have arrived on this tier, poll the next one */
        if(avail == 0) {
            continue;
        }

        /* Got an error while trying to read from socket */
        if(avail == -1) {
            DESCSOCK_LOG("Error reading from socket on tier %d\n", tier);
            goto out;
        }

        /* Go through all received descriptors */
        while(!FIXEDQ_EMPTY(sc->rx_queue.complete_pkt[tier])) {

            pkt = packet_alloc();
            if(pkt == NULL) {
                DESCSOCK_LOG("packet_alloc returned NULL\n");
                goto out;
            }

            /* Build a packet from one or multipe return descriptors */
            desc = FIXEDQ_HEAD(sc->rx_queue.complete_pkt[tier]);
            if(desc->len == 0) {
                DESCSOCK_LOG("len is 0\n");
                packet_free(pkt);
                rx_receive_advance_consumer(tier);
                break;
            }

            rx_extra_t *pkt_extras = (rx_extra_t *)&extras_fifo->extra[extras_fifo->cons_idx];

            xf = pkt_extras->xf;
            xf->len = desc->len;

            pkt->xf_first = xf;

            /* XXX: figure out the correct packet len */
            pkt->len = xf->len;

            /* DMA buf data into packet */
            //packet_data_dma(pkt, xf, desc->len);

            /* Adavance consumer index on return ring */
            rx_receive_advance_consumer(tier);
           // FIXEDQ_REMOVE(sc->rx_queue.complete_pkt[tier]);
            /* XXX: add support to Rx a multi-desc packet */
            if(!desc->eop) {
                DESCSOCK_LOG("Jumbo frame are not accepted yet");
                exit(-1);
            }

            /* XXX: Remove later */
           // descsock_print_buf(pkt->xf_first->data, pkt->len);

            /* add pkt to rx ready to consume queueu */
            if(FIXEDQ_FULL(sc->rx_queue.rx_pkt_queue)) {
                DESCSOCK_LOG("Rx queue is full dropping rx packet\n");
                sc->stats.pkt_drop++;
                goto out;
            }

            DESCSOCK_DEBUGF("Enqueueing Rx packet %p with buf %p %lld len %d\n",
                 pkt, pkt->xf_first->data, (UINT64)pkt->xf_first->data, pkt->len);

            /*
             * Insert receved packets into queue
             */
            FIXEDQ_INSERT(sc->rx_queue.rx_pkt_queue, pkt);

            work++;
            sc->stats.pkt_rx++;
            extras_fifo->cons_idx = RING_WRAP(extras_fifo->cons_idx + 1);
        }

        err = descsock_refill_rx_slots(tier, avail);
        if(err != ERR_OK) {
            return FALSE;
        }
    }

out:
    sc->polls++;
    sc->idle += (work == 0)? 1 : 0;

    return (work > 0)? TRUE: FALSE;
}

/*
 * TX
 */
err_t
descsock_ifoutput(struct packet *pkt)
{
    err_t err = ERR_OK;
    int bytes_avail;
    int tier;

    /* for now set tier to 0 */
    tier = 0;
    //struct descsock_softc *sc = containerof(struct descsock_softc, ifnet, ifp);

    laden_desc_fifo_t *tx_out_fifo = &sc->tx_queue.outbound_descriptors[tier];

    /*
     * If send_ring is half full, flush it
     */
    bytes_avail = laden_desc_fifo_avail(tx_out_fifo);
    if(bytes_avail >= (DESCSOCK_MAX_PER_SEND * LADEN_DESC_LEN)) {
        err = ERR_BUF;
        goto out;
    }

    err = descsock_tx_single_desc_pkt(pkt, tier);

    sc->stats.pkt_tx += (err == ERR_OK)? 1 : 0;

    if(err == ERR_OK) {
        DESCSOCK_DEBUGF("sendring cons:%d prod:%d", tx_out_fifo->cons_idx, tx_out_fifo->prod_idx);
        //packet_set_flag(pkt, PACKET_FLAG_LOCKED);
    }

out:
    return err;
}

/* send a single frag packet */
err_t
descsock_tx_single_desc_pkt(struct packet *pkt, UINT32 tier)
{

    UINT16 vlan_tag = 0;
    err_t err = ERR_OK;
    tx_completions_ctx_t *clean_ctx;
    laden_desc_fifo_t *tx_out_fifo = &sc->tx_queue.outbound_descriptors[tier];
    laden_buf_desc_t *send_desc = (laden_buf_desc_t *)&tx_out_fifo->c[tx_out_fifo->prod_idx];

    /* Get buf data from packet */
    send_desc->addr = (UINT64) pkt->xf_first->data;
    send_desc->len = pkt->len + ETHER_CRC_LEN;
    send_desc->type = TX_BUF;
    send_desc->sop = 1;
    send_desc->eop = 1;

    /* Check for vlan tag */
    /* Hack get a DID from a vlan tag */
    //err = descsock_get_vlantag(pkt, &vlan_tag);
    if(err == ERR_OK) {
        if(sc->descsock_l2_override) {
            send_desc->did = DESCSOCK_SET_DID(vlan_tag);
            /* Set the DIR flag in descriptor */
            send_desc->flags |= (1 << 8);
            //vlan_insert_tag(pkt, vlan_tag, TRUE);
            /* descsock_print_pkt(pkt); */
        }
    }

    /*
     * Alloc clean context from queue
     * Save a reference to this packet to clean up later when a tx completion arrives
     */
    clean_ctx = FIXEDQ_ALLOC(sc->tx_queue.completions[tier]);
    if(clean_ctx == NULL) {
        /* tx completions queue is full */
        DESCSOCK_DEBUGF("clean ctx fifo is full\n");
        return ERR_BUF;
    }

    clean_ctx->desc = send_desc;
    clean_ctx->pkt = pkt;
    clean_ctx->xf = pkt->xf_first;

    /* Advance producer index on send ring */
    tx_send_advance_producer(tier);

    /* Send packets here */
    tx_send_advance_consumer(0);

    return ERR_OK;
}


/* Send our config to the DMA Agent */
static err_t
descsock_config_exchange(char * dmapath)
{
    err_t err = ERR_OK;
    int n, res, i, epfd;
    int num_fds = 0;
    struct epoll_event ev;
    struct epoll_event events[2];

    DESCSOCK_DEBUGF("Sending message to dmaa %s", dmapath);

    n = descsock_socket_write(sc->master_socket_fd, dmapath, 256);
    DESCSOCK_DEBUGF("bytes written %d\n", n);
    if(n < 0 ) {
        //DESCSOCK_DEBUGF("Error writing to master socket config_exchange()%s");
        err = ERR_BUF;
        goto out;
    }

    epfd = descsock_epoll_create(1);
    if(epfd < 0) {
        DESCSOCK_DEBUGF("Error epfd: descsock_config_exchange()");
        err = ERR_BUF;
        goto out;
    }
    /* Single event to register for and listen on doorbell socket */
    ev.data.fd = sc->master_socket_fd;
    ev.events = EPOLLIN;

    res = descsock_epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if(res < 0){
        DESCSOCK_DEBUGF("Error descsock_epoll_ctl");
        err = ERR_BUF;
        goto out;
    }

    while (num_fds == 0) {
        DESCSOCK_LOG("driver waiting for FDs");
        /* Wait 10 seconds */
        num_fds = descsock_epoll_wait(epfd, events, 1, (10 * 1000));
        if (num_fds < 0) {
            DESCSOCK_LOG("error on epoll_wait");
            err = ERR_BUF;
            goto out;
        }
    }
    DESCSOCK_LOG("number of events: %d", num_fds);

    /* receive sockets fds, and add them to the sc->sock_fd array */
    err = descsock_recv_socket_conns(events[0].data.fd, sc->sock_fd);
    if(err != ERR_OK) {
        DESCSOCK_LOG("Failed to receive Rx, Tx sockets");
        exit(-1);
    }

    /* Partition received sockets to our RX, TX arrays */
   // descsock_partition_sockets(sc->sock_fd, sc->rx_queue.socket_fd, sc->tx_sockets);
    descsock_partition_sockets(sc->sock_fd, sc->rx_queue.socket_fd, sc->tx_queue.socket_fd);
    for (i = 0; i < NUM_TIERS; i++) {
        DESCSOCK_LOG("RX: %d", sc->rx_queue.socket_fd[i]);
        DESCSOCK_LOG("TX: %d", sc->tx_queue.socket_fd[i]);
    }

out:
    return err;
}

static void
descsock_partition_sockets(int socks[], int rx[], int tx[]) {
    memcpy(rx, socks, NUM_TIERS * sizeof(int));
    memcpy(tx, socks + NUM_TIERS, NUM_TIERS * sizeof(int));
}

/* Connect to master socket at doorbell.sock */
static int
descsock_establish_dmaa_conn(void)
{
    return descsock_get_unixsocket(MASTER_SOCKET);
}

/* Close master socket and all of unix sockets sent by the DMA Agent */
void
descsock_close_fds()
{
    int i;

    close(sc->master_socket_fd);

    for(i = 0; i < NUM_TIERS; i++) {
        if(sc->rx_queue.socket_fd[i] > 0) {
            close(sc->rx_queue.socket_fd[i]);
        }

        if(sc->tx_queue.socket_fd[i] > 0) {
            close(sc->tx_queue.socket_fd[i]);
        }
    }
}

/*
 * RX
 * TMM's polling function
 * This function will loop through a rx tier, looking for dma descriptors to
 * consume as Rx return full descriptors
 */
/* Refill rx slots */
static err_t
descsock_refill_rx_slots(int tier, int max)
{
    int ret;

    /* refill producer descriptor ring */
    rx_send_advance_producer(tier, max);
    ret = rx_send_advance_consumer(tier);
    if(ret == -1) {
        DESCSOCK_LOG("Failed to write to DMAA socket");
        return ERR_BUF;
    }

    return ERR_OK;
}

/* Recycle packets */
static inline void
clean_tx_completions( UINT16 tier)
{
    int avail_to_clean, freed;

    /* Fill completions ring with descriptor bytes*/
    avail_to_clean = tx_receive_advance_producer(tier);
    if(avail_to_clean == 0 || avail_to_clean == -1) {
        return;
    }

    /* advance consumer index by freeing sent packets */
    freed = tx_receive_adanvace_consumer(tier, avail_to_clean);

    DESCSOCK_DEBUGF("Freed packets %d\n", freed);
}

/*
 * Produce empty buf descriptors on tx completions ring
 * Returns the number bytes read form socket
 */
inline int
tx_receive_advance_producer(UINT32 tier)
{
   int n = refill_inbound_fifo_from_socket(1, tier);
   if(n == -1) {
       DESCSOCK_LOG("Error reading from DMAA socket\n");
       return -1;
   }
   return n;
}

/*
 * Consume and clean all packets/descriptors from completions ring
 * Returns the number of recycled packets
 */
inline int
tx_receive_adanvace_consumer(UINT32 tier, int avail_to_clean)
{
    int i = 0;
    tx_completions_ctx_t *tx_clean_ctx;
    empty_desc_fifo_t *tx_in_fifo = &sc->tx_queue.inbound_descriptors[tier];
    int max_clean = DESCSOCK_MAX_PER_POLL * 2;

    /* clean until we hit max count */
    while(avail_to_clean >= EMPTY_DESC_LEN && (i < max_clean)) {

        /* Get a clean context from queue */
        tx_clean_ctx = &FIXEDQ_HEAD(sc->tx_queue.completions[tier]);
        FIXEDQ_REMOVE(sc->tx_queue.completions[tier]);

        if(tx_clean_ctx->pkt != NULL) {
            DESCSOCK_DEBUGF("Cleaning tx send buf %p\n", tx_clean_ctx->xf->data);

            xfrag_free(tx_clean_ctx->xf, false);

            packet_free(tx_clean_ctx->pkt);

            sc->stats.pkt_cleaned++;
        }

        i++;

        /* Advance consumer index on completions ring */
        tx_in_fifo->cons_idx = EMPTY_DESC_RING_WRAP(tx_in_fifo->cons_idx + EMPTY_DESC_LEN) ;

        avail_to_clean -= EMPTY_DESC_LEN;
    }

    return i;
}

/*
 * Polls return ring/rx_socket for new packets, adding descriptors to a queue
 * Returns the number of polled descriptors
 * Returns -1 if we encountered a socket read error
 */
inline int
rx_receive_advance_producer(UINT32 tier)
{
    int i, desc_count;
    UINT32 pkt_count = 0;
    laden_buf_desc_t *desc;
    int bytes_avail = refill_inbound_fifo_from_socket(0, tier);
    laden_desc_fifo_t *rx_in_fifo = &sc->rx_queue.inbound_descriptors[tier];

    if(bytes_avail == 0) {
        return 0;
    }
    else if(bytes_avail == -1) {
        DESCSOCK_LOG("Error reading from socket on tier %d", tier);
        return -1;
    }

    desc_count = descsock_count_pkts_from_fifo(rx_in_fifo, bytes_avail, &pkt_count);

    for(i = 0; i < desc_count; i++) {
        if(FIXEDQ_FULL(sc->rx_queue.complete_pkt[tier])) {
            DESCSOCK_DEBUGF("full return desc fifo\n");
            break;
        }

        desc = (laden_buf_desc_t *)&rx_in_fifo->c[rx_in_fifo->cons_idx];
        FIXEDQ_INSERT(sc->rx_queue.complete_pkt[tier], desc);

        rx_in_fifo->cons_idx = LADEN_DESC_RING_WRAP(rx_in_fifo->cons_idx + LADEN_DESC_LEN);
    }

    return i;
}

/*
 * Will traverse an array of bytes looking for end-of-packet markers so we know the number of
 * packets that we can consume. Save the packet count to pkt_count to pre-allocate packets later
 */
static int
descsock_count_pkts_from_fifo(laden_desc_fifo_t *fifo, UINT32 avail, UINT32 *pkt_count)
{
    int j = 0;
    int open = 0;
    UINT32 pkts = 0;
    int desc_count = 0;
    UINT16 temp_cons_idx = fifo->cons_idx;

    /* Run until we have not descriptor of we have reached max per poll */
    while(avail >= LADEN_DESC_LEN) {
        laden_buf_desc_t *desc = (laden_buf_desc_t *)&fifo->c[temp_cons_idx];

        if(desc->sop && desc->eop) {
            desc_count++;
            pkts++;

            if(pkts == DESCSOCK_MAX_PER_POLL) {
                break;
            }
        }
        else if(desc->sop) {
            open = 1;
            j++;

        }
        else if(!desc->sop && !desc->eop && open) {
            j++;
        }
        else if(desc->eop && open) {
            /* Count this descriptor as last descriptor for packet */
            desc_count++;
            pkts++;
            /* Add the other descriptors to this count */
            desc_count += j;

            /* Reset counters for next full packet search */
            j = 0;
            open = 0;
        }

        temp_cons_idx = LADEN_DESC_RING_WRAP(temp_cons_idx + LADEN_DESC_LEN);
        avail -= LADEN_DESC_LEN;
    }

    *pkt_count = pkts;

    return desc_count;
}

/* Consume a descriptors from queue */
static inline void
rx_receive_advance_consumer(UINT32 tier)
{
    FIXEDQ_REMOVE(sc->rx_queue.complete_pkt[tier]);
}

/*
 * Send out empty buf descriptors aka producer descriptors, advancing the producer index
 * based on the number of bytes written to an RX socket.
 * Returns a non negative integer for success, -1 otherwise
 */
static inline int
rx_send_advance_consumer(int tier)
{
    /* Advance consumer index writing bytes to socket */
    int sent_descs = flush_bytes_to_socket(0, tier);

    if(sent_descs == -1) {
        DESCSOCK_LOG("Error writing to DMAA socket on tier %d", tier);
        return -1;
    }

    return sent_descs;
}


/*
 * Produces rx empty buf slots on the rx_send / producer ring
 * @max is the number of max slots we would like to allocate at any given time
 * Returns the number of allocated rx slots
 */
static inline int
rx_send_advance_producer(UINT16 tier, int max)
{
    int sent = 0;
    int i;
    err_t err;
    empty_desc_fifo_t *fifo = &sc->rx_queue.outbound_descriptors[tier];
    rx_extra_fifo_t *extra_fifo = &sc->rx_queue.pkt_extras[tier];

    for(i = 0; i < max; i++) {

        /* Allocate an xfrag/xdata */
        err = descsock_build_rx_slot(tier);
        if(err != ERR_OK) {
            DESCSOCK_DEBUGF("Error allocating rx_slot\n");
            break;
        }
        sc->stats.rx_descs++;

        /* Advance producer index */
        fifo->prod_idx = EMPTY_DESC_RING_WRAP(fifo->prod_idx + EMPTY_DESC_LEN);
        extra_fifo->prod_idx = RING_WRAP((fifo->prod_idx / EMPTY_DESC_LEN) + 1);
        sent++;
    }

    return sent;
}

/*
 * Allocates a 2K return buf aka xfrag/xdata for packet data to be writen to.
 */
static err_t
descsock_build_rx_slot(UINT32 tier)
{
    struct xfrag *xfrag;
    bool rx = true;
    //void *base = NULL;
    empty_buf_desc_t *producer_desc;
    empty_desc_fifo_t *fifo = &sc->rx_queue.outbound_descriptors[tier];

    /* This producer buf index is where we're gonna receive return full packets */
    UINT16 produce_buf_idx = (fifo->prod_idx / EMPTY_DESC_LEN);

    rx_extra_fifo_t *extra_fifo = &sc->rx_queue.pkt_extras[tier];
    rx_extra_t *pkt_extras;

    //xfrag = packet_data_newfrag(&base);
    xfrag = xfrag_alloc(rx);
    if(xfrag == NULL) {
        DESCSOCK_LOG("xfrag returned null\n");
        return ERR_BUF;
    }

    /* Get an empty buf descriptor */
    producer_desc = (empty_buf_desc_t *)&fifo->c[fifo->prod_idx];
    if(producer_desc == NULL) {
        DESCSOCK_LOG("NULL desc\n");
        exit(-1);
    }
    pkt_extras = (rx_extra_t *)&extra_fifo->extra[produce_buf_idx];

    producer_desc->addr = (UINT64)xfrag->data;
    producer_desc->len = BUF_SIZE;
    //ex->frag = xfrag;
    pkt_extras->xf = xfrag;
    pkt_extras->phys = producer_desc->addr;

    return ERR_OK;
}

/* Advance producer index on send ring */
static inline err_t
tx_send_advance_producer(UINT32 tier)
{
    laden_desc_fifo_t *tx_out_fifo = &sc->tx_queue.outbound_descriptors[tier];

    tx_out_fifo->prod_idx = LADEN_DESC_RING_WRAP(tx_out_fifo->prod_idx + LADEN_DESC_LEN);

    return ERR_OK;
}

/*
 * Adavances the consumer index after writing descriptors/bytes to socket
 * returns the number flushed descriptors
 */
static inline int
tx_send_advance_consumer(UINT32 tier)
{
    int n = flush_bytes_to_socket(1, tier);

    if(n == -1) {
        DESCSOCK_LOG("Error writing to socket");
        return -1;
    }

    return n;
}

/*
 * Send ring and producer ring socket writev operations, this function writes bytes from a descriptor
 * fifo to a socket
 */
static inline int
flush_bytes_to_socket(UINT32 tx, int qos)
{
    int fd, advance, ret;
    char *buf;
    /* Define an iovec struct that describes one or both destination ranges */
    struct iovec iov[2];
    UINT32 avail_to_send, free_bytes, ring_size, ring_mask, ring_wrap, len0;
    UINT16 prod_idx, cons_idx;

    ring_size = (tx)? LADEN_DESC_RING_SIZE : EMPTY_DESC_RING_SIZE;
    ring_mask = (tx)? LADEN_DESC_RING_MASK : EMPTY_DESC_RING_MASK;
    prod_idx = (tx)? sc->tx_queue.outbound_descriptors[qos].prod_idx : sc->rx_queue.outbound_descriptors[qos].prod_idx;
    cons_idx = (tx)? sc->tx_queue.outbound_descriptors[qos].cons_idx : sc->rx_queue.outbound_descriptors[qos].cons_idx;
    /* Get a pointer to the consumer index to update later when we write to socket */
    UINT16 *ptr_cons = (tx)? &sc->tx_queue.outbound_descriptors[qos].cons_idx : &sc->rx_queue.outbound_descriptors[qos].cons_idx;

    /* Select a file descriptor from the array by qos tier */
    fd = (tx)? sc->tx_queue.socket_fd[qos] : sc->rx_queue.socket_fd[qos];
    ret = 0;

    /* Calculate bytes available to send */
    avail_to_send = (prod_idx - cons_idx) & ring_mask;
    free_bytes = ring_size - avail_to_send;

    /* If nothing to send then return the amount of bytes available in ring */
    if(avail_to_send == 0) {
        /* XXX: need to return 0 here */
        return free_bytes;
    }

    /* Determine if the available data we're trying to flush spans the ring wrap point. */
    ring_wrap = (cons_idx + avail_to_send) >= ring_size;
    /* Determine how many bytes in the first block. */
    len0 = ((ring_wrap)? ring_size : prod_idx) - cons_idx;

    /* Get a pointer to the buf of the send ring or producer ring */
    buf = (tx)? sc->tx_queue.outbound_descriptors[qos].c : sc->rx_queue.outbound_descriptors[qos].c;

    /* first block to write bytes to */
    iov[0].iov_base = buf + cons_idx;
    iov[0].iov_len = len0;

    /*
     * The second range always starts at the base and its length (possibly zero) is the free space after
     * the wrap
     */
    iov[1].iov_base = buf;
    iov[1].iov_len = avail_to_send - len0;

    /* Make the socket I/O call */
    advance = descsock_writev_file(fd, iov, (1 + ring_wrap));
    //DESCSOCK_DEBUGF("Writen bytes %d\n", advance);

    if (advance > 0) {
        /* We wrote data out to the socket, update consumer idx and return number of bytes writen */
        *ptr_cons = (cons_idx + advance) & ring_mask;
        ret = advance / (tx? LADEN_DESC_LEN : EMPTY_DESC_LEN);
    }
    else if (advance == 0){
        /*
         * We received a EAGAIN or EWOULDBLOCK orderly zero byte write. Return the
         * number of previously available bytes
         */
        ret = free_bytes;
    }
    else if(advance == -1){
        /* received an error while writing to socket */
        ret = -1;
    }

    return ret;
}

/*
 * Tx completions ring or Rx return ring
 * This function reads bytes from a socket to fill a descriptor fifo
 */
static inline int
refill_inbound_fifo_from_socket(UINT32 tx, UINT32 qos)
{
    int ret, advance, fd;
    char *buf;
    /* Define an iovec struct that describes one or two sources */
    struct iovec iov[2];
    UINT32 current_avail, free_n, ring_size, ring_mask, ring_wrap, len0;
    UINT16 prod_idx, cons_idx;
    UINT16 *ptr_prod;

    ring_size = (tx)? EMPTY_DESC_RING_SIZE : LADEN_DESC_RING_SIZE;
    ring_mask = (tx)? EMPTY_DESC_RING_MASK : LADEN_DESC_RING_MASK;
    prod_idx = (tx)? sc->tx_queue.inbound_descriptors[qos].prod_idx : sc->rx_queue.inbound_descriptors[qos].prod_idx;
    cons_idx = (tx)? sc->tx_queue.inbound_descriptors[qos].cons_idx : sc->rx_queue.inbound_descriptors[qos].cons_idx;
    /* Get a pointer to the producer index to update later when we write to socket */
    ptr_prod = (tx) ? &sc->tx_queue.inbound_descriptors[qos].prod_idx : &sc->rx_queue.inbound_descriptors[qos].prod_idx;
    ret = 0;

    /* Calculate the current amount of bytes in the ring */
    current_avail = (prod_idx - cons_idx) & ring_mask;
    free_n = ring_size - current_avail;

    /* If there are not free bytes in the ring which means is full, return the current amount */
    if(free_n == 0) {
        // XXX: Don't have to return 0 here, the ring is empty
        return current_avail;
    }

    /* Determine if the futre available data we're trying to read will span the ring wrap point. */
    ring_wrap = (prod_idx + free_n) >= ring_size;
    /* Determine how many bytes in the first block. */
    len0 = (ring_wrap? ring_size : cons_idx) - prod_idx;
    /* Get a buf pointer the ring we'll use for reading into */
    buf = tx ? sc->tx_queue.inbound_descriptors[qos].c : sc->rx_queue.inbound_descriptors[qos].c;

    /* first block to write bytes to */
    iov[0].iov_base = buf + prod_idx;
    iov[0].iov_len = len0;

    /* second block if we have to ring wrap. the length will be 0 if no need to wrap */
    iov[1].iov_base = buf;
    iov[1].iov_len = free_n - len0;

    /* Select a file descriptor from the array by qos tier */
    fd = tx ? sc->tx_queue.socket_fd[qos] : sc->rx_queue.socket_fd[qos];

    /* Make the socket I/O call */
    advance = descsock_readv_file(fd, iov, (1 + ring_wrap));
    //DESCSOCK_DEBUGF("read bytes %d\n", advance);
    if (advance > 0) {
        /* We read bytes from socket, advance the producer idx */
        *ptr_prod = (prod_idx + advance) & ring_mask;
        /* return the new total bytes available in ring */
        ret =  current_avail + advance;
    }
    else if (advance == 0) {
        /*
         * We received a EAGAIN or EWOULDBLOCK orderly zero byte read. Return the
         * number of previously available bytes
         */
        ret = current_avail;
    }
    else if (advance == -1) {
        /* received an error while reading from socket */
        ret =  -1;
    }

    return ret;
}

void
descsock_print_buf(void * buf, int buf_len)
{
    char *p = (char *)buf;
    int i;

    if (buf == NULL || buf_len < 0) {
        DESCSOCK_LOG("NULL buf\n");
		return;
    }

    printf("Buf: %p Length: %d\n", buf, buf_len);

    for (i = 0; i < buf_len; i++) {
        if (i % 32 == 0) {
            printf("\n%06d ", i);
        }
        printf("%02x ", p[i]);
    }
    printf("\n\n");
}


/*
 * FIFO's Api
 * XXX: All of this can be static inline functions
 */
int
laden_desc_fifo_avail(laden_desc_fifo_t *fifo)
{
    int avail = (fifo->prod_idx - fifo->cons_idx) & LADEN_DESC_RING_MASK;
    return avail;
}

BOOL
laden_desc_fifo_empty(laden_desc_fifo_t *fifo)
{
    UINT16 prod = fifo->prod_idx;
    UINT16 cons = fifo->cons_idx;

    return (prod == cons);
}

BOOL
laden_desc_fifo_full(laden_desc_fifo_t *fifo)
{
    int avail = laden_desc_fifo_avail(fifo);
    return (avail == 0);
}

int
empty_desc_fifo_avail(empty_desc_fifo_t *fifo)
{
    int avail = (fifo->prod_idx - fifo->cons_idx) & EMPTY_DESC_RING_MASK;
    return avail;
}

BOOL
empty_desc_fifo_empty(empty_desc_fifo_t *fifo)
{
    UINT16 prod = fifo->prod_idx;
    UINT16 cons = fifo->cons_idx;

    return (prod == cons);
}

BOOL
empty_desc_fifo_full(empty_desc_fifo_t *fifo)
{
    int avail = empty_desc_fifo_avail(fifo);
    return (avail == 0);
}