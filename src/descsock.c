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
 * $Id: //depot/tmos/core-ce-platform/tmm/dev/descsock/if_descsock.c#4 $
 */
#include <stdint.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "sys/types.h"
#include "sys/fixed_queue.h"
#include "kern/sys.h"
#include "net/packet.h"

#include "descsock.h"



/*
 * Internal driver functions
 */
//static void descsock_periodic_task(struct timer *, void *);
err_t descsock_config_exchange(struct descsock_softc * sc, char * dmapath);
int descsock_establish_dmaa_conn(void);
void descsock_partition_sockets(int socks[], int rx[], int tx[]);

/*
 * TX, RX private helpers
 */

/*
 * Rx
 *
 * Building Rx slots, Producer descriptors, Reading bytes/descriptors from sockets, Building packets,
 * Passing packets up the TMM stack
 */
err_t descsock_build_rx_slot(struct descsock_softc *sep, UINT32 tier);
inline err_t descsock_refill_rx_slots(struct descsock_softc *sc, int tier, int max);
inline int refill_inbound_fifo_from_socket(struct descsock_softc *sep,  UINT32 tx,  UINT32 qos);
int descsock_count_pkts_from_fifo(laden_desc_fifo_t *fifo, UINT32 avail, UINT32 *pkt_count);
inline int rx_receive_advance_producer(struct descsock_softc *sc, UINT32 tier);
inline void rx_receive_advance_consumer(struct descsock_softc *sc, UINT32 tier);
inline int tx_receive_advance_producer(struct descsock_softc *sc, UINT32 tier);
inline int tx_receive_adanvace_consumer(struct descsock_softc *sc, UINT32 tier, int max);

/*
 * Tx
 *
 * Writing bytes/descriptors to sockets, Building send descriptors from egress packets
 * Saving packet references for cleaning up when completions arrive
 */
inline int flush_bytes_to_socket(struct descsock_softc *sep, UINT32 tx, int qos);
inline err_t tx_send_advance_producer(struct descsock_softc *sc, UINT32 tier);
inline int tx_send_advance_consumer(struct descsock_softc *sc, UINT32 tier);
err_t descsock_tx_single_desc_pkt(struct descsock_softc *sc, struct packet *pkt, UINT32 tier);
err_t descsock_tx_multi_desc_pkt(struct descsock_softc *sc, struct packet *pkt, UINT32 tier);
inline int rx_send_advance_producer(struct descsock_softc *sc, UINT16 tier, int max);
inline int rx_send_advance_consumer(struct descsock_softc *sc, int tier);
inline void clean_tx_completions(struct descsock_softc *sc, UINT16 tier);

/*
 * Initializing, debug, helper functions
 */
err_t descsock_init_tmmmadc(struct descsock_softc *sc);
err_t descsock_init_tmmpadc(struct descsock_softc *sc);
void descsock_print_buf(void * buf, int buf_len);
void descsock_print_pkt(struct packet *pkt);
void descsock_close_fds(struct descsock_softc *sc);
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
 * device configuration
 */
// GLOBALSET device_conf_t descsock_dev_conf = {
//     .name            = "descsock",
//     .probe           = descsock_probe,
//     .attach          = descsock_attach,
//     .attach_on_probe = TRUE,
// };
// DEVICE_DECLARE(pseudo, descsock_dev_conf);

// /*
//  * Used for TMM TCL script bring up
//  */
// static RTTHREAD struct descsock_config {
//     enum {
//         SETUP_NONE = 0,
//         SETUP_BY_BASE_FD,
//         SETUP_BY_BASE_PATH,
//         SETUP_BY_MASTER_FD,
//         SETUP_BY_MASTER_PATH,
//         SETUP_PARSE_FAILED = -1,
//     } setup_mode;

//     int         base_fd;
//     const char *base_path;

//     UINT32      flags;
//     int         n_qos;

//     const char *force_ifname;
//     const char *ifname_prefix;
//     UINT32      mtu;
//     UINT8       bus, slot, func;

// } descsock_config = {
//     .n_qos = 2,
//     .flags = DESCSOCK_FLAG_VIRT_ADDR,
//     .ifname_prefix = "descsock",
//     .mtu = DESCSOCK_MTU
// };

/*
 * Used for parsing descsock arguments from cli run command
 * --descsock="virt,base_path=/foo/bar/baz,fakepci=ff:1f.0"
 */
// static BOOL
// descsock_parse_config_clause(char *clause)
// {
//     DESCSOCK_LOG("descsock parse %s", clause);
//     char *key = clause;
//     char *val = NULL;
//     char *endptr = NULL;

//     if (clause == NULL ) {
//         return FALSE;
//     }

//     /* Check if there is an = char in the clause string */
//     val = strchr(clause, '=');

//     /* Allow for clauses with just a key rather than key-value pairs.*/
//     if (val == NULL) {
//         if (!strcmp(key, "virt")) {
//             descsock_config.flags |= DESCSOCK_FLAG_VIRT_ADDR;
//             descsock_config.flags &= ~DESCSOCK_FLAG_PHYS_ADDR;
//             return TRUE;
//         } else if (!strcmp(key, "phys")) {
//             descsock_config.flags |= DESCSOCK_FLAG_PHYS_ADDR;
//             descsock_config.flags &= ~DESCSOCK_FLAG_VIRT_ADDR;
//             return TRUE;
//         } else if (!strcmp(key, "debug")) {
//             descsock_config.flags |= DESCSOCK_FLAG_RUNTIME_DEBUG;
//             return TRUE;
//         } else {
//             return FALSE;
//         }
//     }

//     /*
//      * Catch clauses a zero-length string on either side of an '='.
//      */
//     if ((val == clause) || (val[1] == '\0')) {
//         return FALSE;
//     }

//     /* Replace the '=' with '\0' and advance the pointer by one. */
//     *(val++) = '\0';

//     /* XXX: refactor nested ifs */
//     if (!strcmp(key, "base_fd")) {
//         descsock_config.base_fd = strtol(val, &endptr, 0);
//         if ((descsock_config.base_fd <= 2) || (endptr == val) ||
//                 (endptr == NULL) || (*endptr != '\0')) {
//             return FALSE;
//         }
//         else {
//             descsock_config.setup_mode = SETUP_BY_BASE_FD;
//             return TRUE;
//         }
//     }
//     else if (!strcmp(key, "base_path")) {
//         descsock_config.base_path = umem_strdup(val, M_DEVBUF, 0);
//         if (descsock_config.base_path == NULL) {
//             return FALSE;
//         }
//         else {
//             descsock_config.setup_mode = SETUP_BY_BASE_PATH;
//             return TRUE;
//         }
//     }
//     else if (!strcmp(key, "master_fd")) {
//         descsock_config.base_fd = strtol(val, &endptr, 0);
//         if ((descsock_config.base_fd <= 2) || (endptr == val) ||
//                 (endptr == NULL) || (*endptr != '\0')) {
//             return FALSE;
//         }
//         else {
//             descsock_config.setup_mode = SETUP_BY_MASTER_FD;
//             return TRUE;
//         }
//     }
//     else if (!strcmp(key, "master_path")) {
//         descsock_config.base_path = umem_strdup(val, M_DEVBUF, 0);
//         if (descsock_config.base_path == NULL) {
//             return FALSE;
//         }
//         else {
//             descsock_config.setup_mode = SETUP_BY_MASTER_PATH;
//             return TRUE;
//         }
//     }
//     else if (!strcmp(key, "n_qos")) {
//         descsock_config.n_qos = strtol(val, &endptr, 0);
//         if ((descsock_config.n_qos < 1) ||
//                 (descsock_config.n_qos > DESCSOCK_MAX_QOS_TIERS) ||
//                 (endptr == val) || (endptr == NULL) || (*endptr == '\0')) {
//             return FALSE;
//         }
//         else {
//             return TRUE;
//         }
//     }
//     else if (!strcmp(key, "mtu")) {
//         descsock_config.mtu = strtol(val, &endptr, 0);
//         if ((descsock_config.mtu < ETHERMIN) ||
//                 (descsock_config.mtu > ETHER_MAX_LEN_JUMBO) ||
//                 (endptr == val) || (endptr == NULL) || (*endptr == '\0')) {
//             return FALSE;
//         }
//         else {
//             return TRUE;
//         }
//     }
//     else if (!strcmp(key, "ifname_prefix")) {
//         descsock_config.ifname_prefix = umem_strdup(val, M_DEVBUF, 0);
//         if (descsock_config.ifname_prefix == NULL) {
//             return FALSE;
//         }
//         else {
//             return TRUE;
//         }
//     }
//     else if (!strcmp(key, "ifname")) {
//         descsock_config.force_ifname = umem_strdup(val, M_DEVBUF, 0);
//         if (descsock_config.force_ifname == NULL) {
//             return FALSE;
//         }
//         else {
//             return TRUE;
//         }
//     }
//     else if (!strcmp(key, "fakepci")) {
//         descsock_config.bus = strtol(val, &endptr, 16);
//         if (*endptr != ':') {
//             return FALSE;
//         }
//         descsock_config.slot = strtol(endptr + 1, &endptr, 16);
//         if (*endptr != '.') {
//             return FALSE;
//         }
//         descsock_config.func = strtol(endptr + 1, &endptr, 16);
//         if (*endptr || (descsock_config.slot > 0x1F) || (descsock_config.func > 0x7)) {
//             return FALSE;
//         }
//         descsock_config.flags |= DESCSOCK_FLAG_FAKE_PCI;
//         return TRUE;
//     }

//     return FALSE;
// }

/*
 * Used for parsing descsock arguments from cli run command
 * --descsock="virt,base_path=/foo/bar/baz,fakepci=ff:1f.0"
 */
// static BOOL
// descsock_parse_config(void)
// {
//     DESCSOCK_DEBUGF("descsock_parse_config");
//     char *cfgtmp;
//     char *save = NULL;
//     char *trav;

//     if ((descsock_config.setup_mode != SETUP_NONE) ||
//             (hudconf.descsock_cfg_string == NULL)) {
//         goto out;
//     }

//     /* Get a pointer to the "virt,base_path=/foo/bar/baz,fakepci=ff:1f.0" for parsing */
//     cfgtmp = umem_strdup(hudconf.descsock_cfg_string, M_DEVBUF, 0);

//     /* Go through the string parsing for values to configure pci device */
//     for (trav = strtok_r(cfgtmp, ",", &save); trav != NULL;
//         trav = strtok_r(NULL, ",", &save)) {

//         if (!descsock_parse_config_clause(trav)) {
//             DESCSOCK_LOG("Invalid descsock config clause \"%s\".\n", trav);
//             descsock_config.setup_mode = SETUP_PARSE_FAILED;

//             break;
//         }
//     }
//     ufree(cfgtmp);

// out:
//     return (descsock_config.setup_mode != SETUP_NONE);
// }

/*
 * The device probe routine runs BEFORE anything from tmm_init.tcl or
 * --command <tcl cmd> is executed so we can't count on TCL to tell useful
 * whether or not the pseudo-device is configured so we must either return
 * true here unconditionally or use a command line flag like the shmx driver
 * does.
 */
BOOL
descsock_probe(f5dev_t dev)
{
    /*
     * XXX: --descsock="virt,base_path=/foo/bar/baz,fakepci=ff:1f.0"
     * parse pic card info here
     * hardcode bus,slot func values for now
     */

    return descsock_parse_config();
}

/* Bring up the driver */
// static err_t
// descsock_ifup(struct ifnet *ifp)
// {
//     /*
//      * Enable the interface
//      */
//     DESCSOCK_LOG("Setting interface state to up.");
//     ifp->ifflags |= IFF_UP;

//     ifp->if_link_state = LINK_STATE_UP;

//     ifmedia_link_update(ifp, IFM_ETHER | IFM_10000_TX | IFM_FDX);

//     return ERR_OK;
// }

/*
 * Attach to TMM
 */
// static f5device_t *
// descsock_attach(f5dev_t dev)
// {
//     int i;
//     struct descsock_softc *sc;
//     struct ifnet *ifp;
//     err_t err;
//     int tier;

//     if (descsock_config.setup_mode == SETUP_PARSE_FAILED) {
//         DESCSOCK_LOG("Cannot attach descsock device due to invalid config string.\n");
//         return NULL;
//     }

//     sc = (struct descsock_softc *)umalloc(sizeof(struct descsock_softc), M_DEVBUF, UM_ZERO);

//     if (dev.type ==  F5DEV_PSEUDO) {
//         DESCSOCK_LOG("Attempting to attach as pseudo device name = \"%s\"\n",
//                 dev.pseudo.name);
//     } else {
//         DESCSOCK_LOG("Attempting to attach as PCI device = %02x:%02x.%x\n",
//                 dev.pci.bus, dev.pci.slot, dev.pci.func);
//     }

//     if (sc == NULL) {
//         DESCSOCK_LOG("Cannot allocate descsock driver structure for sc %d\n", hudthread.tmid);
//         return NULL;
//     }

//     /* connect TMM ifnet to this driver */
//     ifp = &sc->ifnet;
//     ifp->dev.dv_name = descsock_dev_conf.name;
//     ifp->dev.dv_unit = hudthread.tmid;
//     ifp->dev.dv_class = DV_NET;
//     ifp->dev.poll = descsock_poll;
//     ifp->dev.detach = descsock_detach;
//     ifp->dev.dv_desc = "Descriptor Socket Virtual NIC";
//     /* device_set_period(&ifp->dev, DEVICE_MAX_PERIOD); */
//     device_set_period(&ifp->dev, DESCSOCK_POLL_USEC);
//     device_notify_sleep(&ifp->dev);

//     ifp->if_mtu = descsock_config.mtu;
//     ifp->if_baudrate = DESCSOCK_NOMINAL_BITRATE;
//     ifp->ifup = descsock_ifup;

//     ifp->ifdown = descsock_ifdown;
//     ifp->ifoutput = descsock_ifoutput;

//     if (descsock_config.flags & DESCSOCK_FLAG_FAKE_PCI) {
//         ifp->card_info.pci.bus = descsock_config.bus;
//         ifp->card_info.pci.slot = descsock_config.slot;
//         ifp->card_info.pci.func = descsock_config.func;
//     }

//     /*
//      * XXX:
//      * As support is added, enable IFF_RXCSUM, IFF_TXL4CSUM, LRO and TSO, etc.
//      */
//     ifnet_init_flag(ifp, IFF_VIRTUAL);
//     /* ifp->ifflags = 0; */

//     sc->n_qos = descsock_config.n_qos;
//     sc->sep = hudthread.tmid;

//     /*
//      * Check for L2 override behavior
//      */
//     char *l2_override = descsock_getenv("TMM_DESCSOCK_L2_OVERRIDE");
//     if(l2_override != NULL) {
//         DESCSOCK_LOG("L2_OVERRIDE IS SET\n");
//         sc->descsock_l2_override = TRUE;
//     }

//     /* Check for madc or padc */
//     if(strcmp(MADC_PLATFORM, hudconf.platform) == 0) {
//         DESCSOCK_LOG("\nAttach as MADC\n");
//         err = descsock_init_tmmmadc(sc);
//         sc->mode = MADC_MODE;

//         if(err != ERR_OK) {
//             DESCSOCK_LOG("Failed to init descsock tmm-madc");
//             goto fail_early;
//         }
//     }
//     else if(strcmp(PADC_PLATFORM, hudconf.platform) == 0) {
//         DESCSOCK_LOG("\nAttach as PADC\n");
//         err = descsock_init_tmmpadc(sc);
//         sc->mode = PADC_MODE;

//         if(err != ERR_OK) {
//              DESCSOCK_LOG("Failed to init descsock tmm-padc");
//             goto fail_early;
//         }
//     }
//     else {
//         goto fail_early;
//     }


//     DESCSOCK_LOG("Attach as platform %s\n", hudconf.platform);
//     if (descsock_config.force_ifname != NULL) {
//         snprintf(ifp->ifname, F5_IFNAMSIZ, "%s", descsock_config.force_ifname);
//     }
//     else {
//         snprintf(ifp->ifname, F5_IFNAMSIZ, "%s%d", descsock_config.ifname_prefix, sc->sep);
//     }

//     /* Register with tmm NIC driver framework. */
//     DESCSOCK_LOG("Registered with tmm framework\n");
//     ifnet_register(ifp);

//     /* We effectively have no phy to control so make something up and leave it */
//     ifmedia_init(ifp, &sc->ifmedia, IFM_IMASK, NULL, NULL);
//     ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_10000_TX | IFM_FDX, 0, NULL);
//     ifmedia_default(&sc->ifmedia, IFM_ETHER | IFM_10000_TX | IFM_FDX);

//     /*Update our interface stats a couple times a second. */
//     timer_add_periodic(&sc->stats_timer, descsock_periodic_task, sc, HZ / 10);

//     /* Init queues */
//     for(i = 0; i < NUM_TIERS; i++) {
//         FIXEDQ_INIT(sc->tx_queue.completions[i]);
//         FIXEDQ_INIT(sc->rx_queue.complete_pkt[i]);
//     }

//     /* XXX: from now we use tier 0 for everything */
//     tier = 0;

//     /*
//      * Produce RING_SIZE of empty buf producer descriptors and write them out to the DMA Agent
//      */

//     /* produce descriptors */
//     DESCSOCK_LOG("Producing xdatas\n");
//     i = rx_send_advance_producer(sc, tier, RING_SIZE - 1);
//     DESCSOCK_LOG("rx_send_advance producer %d\n", i);
//     if(i <= 0) {
//         DESCSOCK_LOG("Error pruducing xdatas\n");
//     }

//     /* Consume */
//     DESCSOCK_LOG("consuming xdatas\n");
//     i = rx_send_advance_consumer(sc, tier);
//     DESCSOCK_LOG("rx_send_advance consumer %d\n", i);
//     if(i <= 0) {
//         DESCSOCK_LOG("Error sending producer descriptors on init");
//     }

//     DESCSOCK_LOG("descsock: SEP%d attached as %s\n", sc->sep, ifp->ifname);

//     return &sc->ifnet.dev;

// fail_early:
//     for (i = 0; i < array_size(sc->sock_fd); i++) {
//         if (sc->sock_fd[i] >= 0) {
//             close_file(sc->sock_fd[i]);
//         }
//     }

//     DESCSOCK_LOG("Failed to attach descsock driver");
//     ufree(sc);
//     return NULL;
// }

/*
 * Init modular tmm,
 * send registration message containing this tmm's memory info and number of SEP requests
 */
err_t
descsock_init_tmmmadc(struct descsock_softc *sc)
{
    /* Get the information where tmm stores its memory */
    err_t err = ERR_OK;
    char msg[DESCSOCK_PATH_MAX];
    int i;

    sc->tmm_driver_mem = sys_get_tmm_mem();
    if(sc->tmm_driver_mem == NULL) {
        DESCSOCK_LOG("Could not get base address of tmm memory\n");
        err = ERR_CONN;
        goto out;
    }

    /* call the master socket here */
    sc->master_socket_fd = descsock_establish_dmaa_conn();
    if(sc->master_socket_fd < 0) {
        DESCSOCK_LOG("\ndescsock: Failed to connect to master socket\n");
        err = ERR_CONN;
        goto out;
    }
    DESCSOCK_LOG("recived master socket %d\n", sc->master_socket_fd);

    snprintf(msg, DESCSOCK_PATH_MAX, "path=%s\nbase=%llu\nlength=%d\nnum_sep=1\npid=1\nsvc_ids=1\n\n",
            sc->tmm_driver_mem->name, (UINT64)sc->tmm_driver_mem->base, sc->tmm_driver_mem->length);

    DESCSOCK_LOG("Sending msg to DMAA %s", msg);

    /* Send dma region path info to DMA AGENT */
    err = descsock_config_exchange(sc, msg);
    if (err != ERR_OK) {
        DESCSOCK_LOG("---- failed to write dma region to dmaa\n");
        err = ERR_CONN;
        goto out;
    }

    /* Set non-blocking on all TX, RX sockets */
    for(i = 0; i < NUM_TIERS; i++) {
        if (sys_set_non_blocking(sc->rx_queue.socket_fd[i], 1) != ERR_OK) {
            DESCSOCK_LOG("Error setting non_blocking on rx socket");
            err = ERR_MEM;
            goto out;
        }
        if (sys_set_non_blocking(sc->tx_queue.socket_fd[i], 1) != ERR_OK) {
            DESCSOCK_LOG("Error setting non_blocking on tx socket");
            err = ERR_MEM;
            goto out;
        }
    }

out:
    return err;
}

/*
 * Init tmm padc,
 * open virtio ports to crate Rx and Tx file descriptors.
 */
// err_t
// descsock_init_tmmpadc(struct descsock_softc *sc)
// {
//     //DESCSOCK_LOG("\nhudconf.npus %d", hudconf.npus);
//     int i, j;
//     int fd = -1;

//     for(i = 0; i < hudconf.npus; i++) {
//          for(j = 0; j < NUM_TIERS; j++) {

//             /*
//              * Rx, save the virtio port path, then open the device at that path
//              */
//             snprintf(sc->virtio_ports[i].rx_paths[j], (DESCSOCK_PATH_MAX -1),"/dev/virtio-ports/dma-sep-%d-rx-%d", i, j);
//             fd = open_file_read_write(sc->virtio_ports[i].rx_paths[j]);

//             if(fd < 0) {
//                 DESCSOCK_LOG("Failed to open virtio device %s", sc->virtio_ports[i].rx_paths[j]);
//                 return ERR_CONN;
//             }

//             sc->sep_sockets[i].rx_fd[j] = fd;
//             DESCSOCK_LOG("Opened virtio device %d at %s", sc->sep_sockets[i].rx_fd[j] = fd, sc->virtio_ports[i].rx_paths[j]);

//             /*
//              * Tx, save the virtio port path, then open the device at that path
//              */
//             snprintf(sc->virtio_ports[i].tx_paths[j], (DESCSOCK_PATH_MAX -1),"/dev/virtio-ports/dma-sep-%d-tx-%d", i, j);
//             fd = open_file_read_write(sc->virtio_ports[i].tx_paths[j]);
//             if(fd < 0) {
//                 DESCSOCK_LOG("Failed to open virtio device %s", sc->virtio_ports[i].tx_paths[j]);
//                 return ERR_CONN;
//             }

//             sc->sep_sockets[i].tx_fd[j] = fd;
//             DESCSOCK_LOG("Opened virtio device %d at %s", sc->sep_sockets[i].tx_fd[j] , sc->virtio_ports[i].tx_paths[j]);
//         }
//     }

//     /* Save sockets fds */
//     for(i = 0; i < NUM_TIERS; i++) {
//         sc->rx_queue.socket_fd[i] = sc->sep_sockets[0].rx_fd[i];
//         sc->tx_queue.socket_fd[i] = sc->sep_sockets[0].tx_fd[i];
//         DESCSOCK_LOG("sc->rx_queue.socket_fd[i] %d, sc->tx_queue.socket_fd[i] %d", sc->rx_queue.socket_fd[i], sc->tx_queue.socket_fd[i]);
//         //DESCSOCK_LOG("sc->rx_sockets[i] %d, sc->tx_sockets[i] %d", sc->sep_sockets[0].rx_fd[i], sc->sep_sockets[0].tx_fd[i]);
//     }

//     /* Set non-blocking on all TX, RX sockets */
//     for(i = 0; i < NUM_TIERS; i++) {
//         if (sys_set_non_blocking(sc->rx_queue.socket_fd[i], 1) != ERR_OK) {
//             DESCSOCK_LOG("Error setting non_blocking on rx socket");
//             sys_exit(-1);
//         }
//         if (sys_set_non_blocking(sc->tx_queue.socket_fd[i], 1) != ERR_OK) {
//             DESCSOCK_LOG("Error setting non_blocking on tx socket");
//             sys_exit(-1);
//         }
//     }
//     DESCSOCK_LOG("Opened virtio ports successfully\n");

//     return ERR_OK;
// }

/* Send our config to the DMA Agent */
err_t
descsock_config_exchange(struct descsock_softc * sc, char * dmapath)
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
        DESCSOCK_DEBUGF("Error writing to master socket config_exchange()%s");
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
    DESCSOCK_DEBUGF("number of events: %d", num_fds);

    /* receive sockets fds, and add them to the sc->sock_fd array */
    err = descsock_recv_socket_conns(events[0].data.fd, sc->sock_fd);
    if(err != ERR_OK) {
        DESCSOCK_LOG("Failed to receive Rx, Tx sockets");
        sys_exit(-1);
    }

    /* Partition received sockets to our RX, TX arrays */
   // descsock_partition_sockets(sc->sock_fd, sc->rx_queue.socket_fd, sc->tx_sockets);
    descsock_partition_sockets(sc->sock_fd, sc->rx_queue.socket_fd, sc->tx_queue.socket_fd);
    for (i = 0; i < NUM_TIERS; i++) {
        DESCSOCK_DEBUGF("RX: %d", sc->rx_queue.socket_fd[i]);
        DESCSOCK_DEBUGF("TX: %d", sc->tx_queue.socket_fd[i]);
    }

out:
    return err;
}

void
descsock_partition_sockets(int socks[], int rx[], int tx[]) {
    memcpy(rx, socks, NUM_TIERS * sizeof(int));
    memcpy(tx, socks + NUM_TIERS, NUM_TIERS * sizeof(int));
}

/* Connect to master socket at doorbell.sock */
int
descsock_establish_dmaa_conn(void)
{
    return descsock_get_unixsocket(MASTER_SOCKET);
}

/*
 * Bring down the driver
 */
// static err_t
// descsock_ifdown(struct ifnet *ifp)
// {
//     DESCSOCK_LOG("Setting interface state to down.");
//     ifp->ifflags &= ~IFF_UP;
//     ifp->if_link_state = LINK_STATE_DOWN;
//     ifmedia_link_update(ifp, 0);

//     return ERR_OK;
// }

void
descsock_periodic_task(struct timer *timer, void *param)
{
    struct descsock_softc *sc = (struct descsock_softc *)param;
    sc->timer_ticks++;
}

/*
 * Remove device from TMM
 * Print out stats
 */
// static void
// descsock_detach(f5device_t *devp)
// {
//     struct ifnet *ifp = (struct ifnet *)devp;
//     struct descsock_softc *sc = containerof(struct descsock_softc, ifnet, ifp);

//     DESCSOCK_LOG("detaching descsock device\n");
//     DESCSOCK_LOG("Tx packets\t\t%d", sc->stats.pkt_tx);
//     DESCSOCK_LOG("Rx packets\t\t%d", sc->stats.pkt_rx);
//     DESCSOCK_LOG("Packet count\t\t%d", (sc->stats.pkt_tx + sc->stats.pkt_rx));
//     DESCSOCK_LOG("Packet drop\t\t%d", sc->stats.pkt_drop);

//     descsock_close_fds(sc);

//     ifnet_unregister(ifp);
// }

/* Close master socket and all of unix sockets sent by the DMA Agent */
void
descsock_close_fds(struct descsock_softc *sc)
{
    int i;

    close_file(sc->master_socket_fd);

    for(i = 0; i < NUM_TIERS; i++) {
        if(sc->rx_queue.socket_fd[i] > 0) {
            close_file(sc->rx_queue.socket_fd[i]);
        }

        if(sc->tx_queue.socket_fd[i] > 0) {
            close_file(sc->tx_queue.socket_fd[i]);
        }
    }
}

/*
 * RX
 * TMM's polling function
 * This function will loop through a rx tier, looking for dma descriptors to
 * consume as Rx return full descriptors
 */
BOOL
descsock_poll(struct dev_poll_param *param, f5device_t *devp)
{
    UINT32 work = 0;
    err_t err;
    int tier, avail, flushed_bytes;
    struct packet *pkt;
    struct xfrag *xf;

    struct ifnet *ifp = (struct ifnet *)devp;
    //struct descsock_softc *sc = containerof(struct descsock_softc, ifnet, ifp);
    struct descsock_softc *sc = NULL;
    laden_buf_desc_t *desc;
    laden_desc_fifo_t *tx_out_fifo;

    rx_extra_fifo_t *extras_fifo;

    /*
     * Iterate through and poll all tiers
     */
    for(tier = 0; tier < NUM_TIERS; tier++) {

        /* Get fifo for current tier */
        tx_out_fifo = &sc->tx_queue.outbound_descriptors[tier];

        extras_fifo = &sc->rx_queue.pkt_extras[tier];


        /*
         * Clean all sent completions on tier
         */
        clean_tx_completions(sc, tier);

        /*
         * Flush/empty the send ring if it has anything
         */
        avail = laden_desc_fifo_avail(tx_out_fifo);
        if(avail >= LADEN_DESC_LEN) {
            /* XXX: if flushed_bytes is -1 then we encountered an error writing to socket */
            flushed_bytes = tx_send_advance_consumer(sc, tier);
            if(flushed_bytes == -1) {
                DESCSOCK_LOG("Failed to write to socket\n");
            }
        }

        /*
         * POLL rx return ring for tier
         */
        avail = rx_receive_advance_producer(sc, tier);

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

            pkt = packet_alloc(NULL);
            if(pkt == NULL) {
                DESCSOCK_LOG("packet_alloc returned NULL\n");
                goto out;
            }

            /* Build a packet from one or multipe return descriptors */
            desc = FIXEDQ_HEAD(sc->rx_queue.complete_pkt[tier]);
            if(desc->len == 0) {
                DESCSOCK_LOG("len is 0\n");
                packet_free(pkt);
                rx_receive_advance_consumer(sc, tier);
                break;
            }

            rx_extra_t *ex = (rx_extra_t *)&extras_fifo->extra[extras_fifo->cons_idx];

            xf = ex->frag;
            xf->base = ex->xdata;
            xf->len = desc->len;

            /* DMA buf data into packet */
            packet_data_dma(pkt, xf, desc->len);

            /* Adavance consumer index on return ring */
            rx_receive_advance_consumer(sc, tier);
            FIXEDQ_REMOVE(sc->rx_queue.complete_pkt[tier]);
            /* XXX: add support to Rx a multi-desc packet */
            if(!desc->eop) {
                DESCSOCK_LOG("Jumbo frame are not accepted yet");
                sys_exit(-1);
            }

            /* XXX: Remove later */
           // descsock_print_pkt(pkt);
            ifinput(ifp, pkt);

            work++;
            sc->stats.pkt_rx++;
            extras_fifo->cons_idx = RING_WRAP(extras_fifo->cons_idx + 1);
        }

        err = descsock_refill_rx_slots(sc, tier, avail);
        if(err != ERR_OK) {
            return FALSE;
        }
    }

out:
    sc->polls++;
    sc->idle += (work == 0)? 1 : 0;

    return (work > 0)? TRUE: FALSE;
}

/* Refill rx slots */
err_t
descsock_refill_rx_slots(struct descsock_softc *sc, int tier, int max)
{
    int ret;

    /* refill producer descriptor ring */
    rx_send_advance_producer(sc, tier, max);
    ret = rx_send_advance_consumer(sc, tier);
    if(ret == -1) {
        DESCSOCK_LOG("Failed to write to DMAA socket");
        return ERR_BUF;
    }

    return ERR_OK;
}

/* Recycle packets */
inline void
clean_tx_completions(struct descsock_softc * sc, UINT16 tier)
{
    int avail_to_clean, freed;

    /* Fill completions ring with descriptor bytes*/
    avail_to_clean = tx_receive_advance_producer(sc, tier);
    if(avail_to_clean == 0 || avail_to_clean == -1) {
        return;
    }

    /* advance consumer index by freeing sent packets */
    freed = tx_receive_adanvace_consumer(sc, tier, avail_to_clean);

    DESCSOCK_DEBUGF("Freed packets %d\n", freed);
}

/*
 * Produce empty buf descriptors on tx completions ring
 * Returns the number bytes read form socket
 */
inline int
tx_receive_advance_producer(struct descsock_softc *sc, UINT32 tier)
{
   int n = refill_inbound_fifo_from_socket(sc, 1, tier);
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
tx_receive_adanvace_consumer(struct descsock_softc * sc, UINT32 tier, int avail_to_clean)
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
            packet_clear_flag(tx_clean_ctx->pkt, PACKET_FLAG_LOCKED);
            packet_free(tx_clean_ctx->pkt);
            tx_clean_ctx->pkt = NULL;

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
rx_receive_advance_producer(struct descsock_softc * sc, UINT32 tier)
{
    int i, desc_count;
    UINT32 pkt_count = 0;
    laden_buf_desc_t *desc;
    int bytes_avail = refill_inbound_fifo_from_socket(sc, 0, tier);
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
int
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
inline void
rx_receive_advance_consumer(struct descsock_softc * sc, UINT32 tier)
{
    FIXEDQ_REMOVE(sc->rx_queue.complete_pkt[tier]);
}

/*
 * Send out empty buf descriptors aka producer descriptors, advancing the producer index
 * based on the number of bytes written to an RX socket.
 * Returns a non negative integer for success, -1 otherwise
 */
inline int
rx_send_advance_consumer(struct descsock_softc *sc, int tier)
{
    /* Advance consumer index writing bytes to socket */
    int sent_descs = flush_bytes_to_socket(sc, 0, tier);

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
inline int
rx_send_advance_producer(struct descsock_softc *sc, UINT16 tier, int max)
{
    int sent = 0;
    int i;
    err_t err;
    empty_desc_fifo_t *fifo = &sc->rx_queue.outbound_descriptors[tier];
    rx_extra_fifo_t *extra_fifo = &sc->rx_queue.pkt_extras[tier];

    for(i = 0; i < max; i++) {

        /* Allocate an xfrag/xdata */
        err = descsock_build_rx_slot(sc, tier);
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
err_t
descsock_build_rx_slot(struct descsock_softc * sc, UINT32 tier)
{
    void *xfrag;
    void *base;
    empty_buf_desc_t *producer_desc;
    empty_desc_fifo_t *fifo = &sc->rx_queue.outbound_descriptors[tier];

    /* This producer buf index is where we're gonna receive return full packets */
    UINT16 produce_buf_idx = (fifo->prod_idx / EMPTY_DESC_LEN);

    rx_extra_fifo_t *extra_fifo = &sc->rx_queue.pkt_extras[tier];
    rx_extra_t *ex;

    xfrag = packet_data_newfrag(&base);
    if(xfrag == NULL) {
        DESCSOCK_LOG("xfrag returned null\n");
        return ERR_BUF;
    }

    /* Get an empty buf descriptor */
    producer_desc = (empty_buf_desc_t *)&fifo->c[fifo->prod_idx];
    if(producer_desc == NULL) {
        DESCSOCK_LOG("NULL desc\n");
        sys_exit(-1);
    }
    ex = (rx_extra_t *)&extra_fifo->extra[produce_buf_idx];

    if(sc->mode == MADC_MODE) {
        /* Use virtual addresses for tmm madc */
        producer_desc->addr = (UINT64)base;
    }
    else {
        /* Use guest physical addresses for tmm padc*/
        producer_desc->addr = vtophys(base);
    }

    producer_desc->len = BUF_SIZE;
    ex->frag = xfrag;
    ex->xdata = base;
    ex->phys = producer_desc->addr;

    return ERR_OK;
}

/*
 * TX
 */
err_t
descsock_ifoutput(struct ifnet *ifp, struct packet *pkt)
{
    err_t err = ERR_OK;
    int bytes_avail;
    int tier;

    /* XXX: Get tier/qos for packet */

    /*
     * if(pkt->priority_set == TRUE) {
     *   tier = descsock_get_tier(pkt->priority);
     * }
     */

    /* for now set tier to 0 */
    tier = 0;
    //struct descsock_softc *sc = containerof(struct descsock_softc, ifnet, ifp);
    struct descsock_softc *sc = NULL;

    laden_desc_fifo_t *tx_out_fifo = &sc->tx_queue.outbound_descriptors[tier];

    if (!packet_check(pkt)) {
        packet_free(pkt);
        DESCSOCK_LOG("Bogus packet on send.\n");
        err = ERR_MEM;
        goto out;
    }

    /*
    if (!(ifp->ifflags & IFF_UP)) {
        DESCSOCK_LOG("Interface is down.\n");
        err = ERR_CONN;
        goto out;
    }

    if (ifp->if_link_state != LINK_STATE_UP) {
        DESCSOCK_LOG("Link is down.\n");
        err = ERR_CONN;
        goto out;
    }
    */

    /*
     * If send_ring is half full, flush it
     */
    bytes_avail = laden_desc_fifo_avail(tx_out_fifo);
    if(bytes_avail >= (DESCSOCK_MAX_PER_SEND * LADEN_DESC_LEN)) {
        err = ERR_BUF;
        goto out;
    }

    if(packet_data_singlefrag(pkt)) {
        /*
         * Handle single frag packet for Tx
         */
        err = descsock_tx_single_desc_pkt(sc, pkt, tier);

        sc->stats.pkt_tx += (err == ERR_OK)? 1 : 0;
    }
    else {
        /*
         * Handle multi frag packet for Tx
         */
        err = descsock_tx_multi_desc_pkt(sc, pkt, tier);

        sc->stats.pkt_tx += (err == ERR_OK)? 1 : 0;
        sc->stats.tx_jumbos += (err == ERR_OK)? 1 : 0;
    }

    if(err == ERR_OK) {
        DESCSOCK_DEBUGF("sendring cons:%d prod:%d", tx_out_fifo->cons_idx, tx_out_fifo->prod_idx);
        //packet_set_flag(pkt, PACKET_FLAG_LOCKED);
    }

out:
    return err;
}

/* send a single frag packet */
err_t
descsock_tx_single_desc_pkt(struct descsock_softc * sc, struct packet *pkt, UINT32 tier)
{

    void *xdata;
    UINT16 xdata_len = 0;
    UINT16 vlan_tag = 0;
    err_t err;
    tx_completions_ctx_t *clean_ctx;
    laden_desc_fifo_t *tx_out_fifo = &sc->tx_queue.outbound_descriptors[tier];
    laden_buf_desc_t *send_desc = (laden_buf_desc_t *)&tx_out_fifo->c[tx_out_fifo->prod_idx];

    /* Get buf data from packet */
    xdata = xfrag_getptrlen(packet_data_firstfrag(pkt), &xdata_len);

    /* tmm-padc or tmm-madc */
    if(sc->mode == MADC_MODE) {
        send_desc->addr = (UINT64)xdata;
    }
    else if(sc->mode == PADC_MODE) {
        send_desc->addr = vtophys(xdata);
    }

    send_desc->len = xdata_len;
    send_desc->type = TX_BUF;
    send_desc->sop = 1;
    send_desc->eop = 1;

    /* Check for vlan tag */
    /* Hack get a DID from a vlan tag */
    err = descsock_get_vlantag(pkt, &vlan_tag);
    if(err == ERR_OK) {
        if(sc->descsock_l2_override) {
            send_desc->did = DESCSOCK_SET_DID(vlan_tag);
            /* Set the DIR flag in descriptor */
            send_desc->flags |= (1 << 8);
            vlan_insert_tag(pkt, vlan_tag, TRUE);
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

    /* Advance producer index on send ring */
    tx_send_advance_producer(sc, tier);

    return ERR_OK;
}

/* XXX: support multi frag packets */
/* Send a multi frag packet */
// static err_t
// descsock_tx_multi_desc_pkt(struct descsock_softc * sc, struct packet *pkt, UINT32 tier)
// {
//     err_t err = ERR_OK;
//     struct xfrag *xf;
//     void *xdata;
//     UINT16 xdata_len = 0;
//     int xfrag_count = 0;
//     laden_buf_desc_t *send_desc = NULL;
//     tx_completions_ctx_t *clean_ctx = NULL;
//     laden_desc_fifo_t *tx_out_fifo = &sc->tx_queue.outbound_descriptors[tier];

//     if (packet_data_fragcount(pkt) > DESCSOCK_MAX_TX_XFRAGS_PER_PACKET) {
//         /*
//          * This is pathological case. We should try to compact it
//          * and reduce # frags. This is expensive operation which
//          * involves data movement.
//          */
//         packet_data_compact(pkt);
//         if (packet_data_fragcount(pkt) > DESCSOCK_MAX_TX_XFRAGS_PER_PACKET) {
//             /* Frag count is still to big, drop it */
//             sc->stats.pkt_drop++;
//             err = ERR_TOOBIG;
//             goto out;
//         }
//     }

//     /* Slice packet for multiple send descriptors */
//     packet_data_xfrag_foreach(xf, pkt) {
//         xdata = xfrag_getptrlen(xf, &xdata_len);

//         send_desc = (laden_buf_desc_t *)&tx_out_fifo->c[tx_out_fifo->prod_idx];
//         /* tmm-padc or tmm-madc */
//         if(sc->mode == MADC_MODE) {
//             send_desc->addr = (UINT64)xdata;
//         }
//         else if(sc->mode == PADC_MODE) {
//             send_desc->addr = vtophys(xdata);
//         }

//         send_desc->len = xdata_len;
//         send_desc->type = TX_BUF;

//         /* Handle vlan tag */
//         if (packet_test_flag(pkt, PACKET_FLAG_ETHERTYPE_VLAN)) {
//         }
//         else {
//         }

//         /* If we have the first xfrag, set the start-of-packet flag */
//         if(xfrag_count == 0) {
//             send_desc->sop = 1;
//         }
//         /*
//          * Alloc clean context from queue
//          * Save a reference to this packet to clean up later when a tx completion arrives
//          */
//         clean_ctx = FIXEDQ_ALLOC(sc->tx_queue.completions[tier]);
//         if(clean_ctx == NULL) {
//             /* sc->sent_completions_ring is full */
//             DESCSOCK_DEBUGF("clean ctx fifo is full\n");
//             err = ERR_BUF;
//             goto out;
//         }
//         clean_ctx->desc = send_desc;
//         clean_ctx->pkt = NULL;

//         xfrag_count++;

//         /* Get another fresh send descriptor */
//         tx_send_advance_producer(sc, tier);
//     }
//     send_desc->eop = 1;
//     clean_ctx->pkt = pkt;

// out:
//     return err;
// }

/* Advance producer index on send ring */
inline err_t
tx_send_advance_producer(struct descsock_softc * sc, UINT32 tier)
{
    laden_desc_fifo_t *tx_out_fifo = &sc->tx_queue.outbound_descriptors[tier];

    tx_out_fifo->prod_idx = LADEN_DESC_RING_WRAP(tx_out_fifo->prod_idx + LADEN_DESC_LEN);

    return ERR_OK;
}

/*
 * Adavances the consumer index after writing descriptors/bytes to socket
 * returns the number of free bytes on ring
 */
inline int
tx_send_advance_consumer(struct descsock_softc * sc, UINT32 tier)
{
    int n = flush_bytes_to_socket(sc, 1, tier);

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
inline int
flush_bytes_to_socket(struct descsock_softc * sc, UINT32 tx, int qos)
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
    DESCSOCK_DEBUGF("Writen bytes %d\n", advance);

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
inline int
refill_inbound_fifo_from_socket(struct descsock_softc *sc, UINT32 tx, UINT32 qos)
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
    DESCSOCK_DEBUGF("read bytes %d\n", advance);
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

/* Get vlan tag from buf */
// err_t
// descsock_get_vlantag(struct packet *pkt, UINT16 *tag)
// {
//     char *l2hdr;
//     struct ether_vlan_header *evh;
//     err_t err = ERR_OK;

//     /* is VLAN set in struct packet. */
//     if (packet_test_flag(pkt, PACKET_FLAG_ETHERTYPE_VLAN)) {
//         *tag = pkt->vlan_tag;
//         goto out;
//     }

//     err = packet_data_pullup_head(pkt, sizeof(*evh), (void **)&l2hdr);
//     if (err != ERR_OK) {
//         goto out;
//     }

//     /* Check the ethertype in the header and if it indicates a VLAN header, use it. */
//     evh = (struct ether_vlan_header *)l2hdr;
//     if (ntohs(evh->evl_encap_proto) == ETHERTYPE_VLAN) {
//         *tag = ntohs(evh->evl_tag);
//         goto out;
//     }

// out:
//     return err;
// }

/* Get ether type from buf */
// UINT16
// descsock_get_ethertype(struct packet *pkt)
// {
//     UINT16 ethertype = 0;
//     char *l2hdr;
//     struct ether_vlan_header *evh;
//     struct ether_header *eh;

//     if (packet_data_pullup_head(pkt, sizeof(*evh), (void **)&l2hdr) !=
//             ERR_OK) {
//         goto out;
//     }

//     evh = (struct ether_vlan_header *)l2hdr;
//     if (ntohs(evh->evl_encap_proto) == ETHERTYPE_VLAN) {
//         ethertype = ntohs(evh->evl_proto);
//         pkt->l3hdr = sizeof(*evh);
//         goto out;
//     }

//     eh = (struct ether_header *)l2hdr;
//     ethertype = ntohs(eh->ether_type);
//     pkt->l3hdr = sizeof(*eh);

// out:
//     return ethertype;
// }

// void
// descsock_print_buf(void * buf, int buf_len)
// {
//     unsigned char *p = (char* )buf;
//     int i;

//     if (buf == NULL || buf_len < 0) {
//         DESCSOCK_LOG("NULL buf\n");
// 		return;
//     }

//     printf("Buf: %p Length: %ld\n", buf, buf_len);

//     for (i = 0; i < buf_len; i++) {
//         if (i % 32 == 0) {
//             printf("\n%06d ", i);
//         }
//         printf("%02x ", p[i]);
//     }
//     printf("\n\n");
// }

// void
// descsock_print_pkt(struct packet *pkt)
// {
//     unsigned char *p;
//     pktcursor_t xcur;
//     err_t err;
//     int i;

//     if (pkt == NULL) {
//         goto done;
//     }

//     xcur = packet_data_begin(pkt);
//     err = packet_data_pullup(pkt, xcur, packet_data_len(pkt), (void **)&p);

//     if (err != ERR_OK) {
//         panic("packet_data_pullup() failed - err: %x\n", err);
//         goto done;
//     }

//     printf("Pkt: %p Length: %ld Buffer: %p\n", pkt, packet_data_len(pkt), p);

//     for (i = 0; i < packet_data_len(pkt); i++) {
//         if (i % 32 == 0) {
//             printf("\n%06d ", i);
//         }
//         printf("%02x ", p[i]);
//     }
//     printf("\n\n");

// done:
//     return;
// }

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