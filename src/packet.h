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

#ifndef PACKET_H
#define PACKET_H


#include <sys/queue.h>
#include <stdint.h>

#include "types.h"
#include "xfrag_mem.h"
#include "err.h"

struct ifh_header {
    uint32_t    did;
    uint32_t    sep;
    uint32_t    svc;
    uint32_t    qos_tier;
    uint32_t    nti;
    uint32_t    dm;
};
struct packet {
    SLIST_ENTRY(packet) next;
    UINT16              magic;
    UINT32              len;
    struct xfrag        *xf_first;
    vlan_t              vlan_tag;
    UINT64              flags;
    /* Add more fields as we go  */
    struct ifh_header   ifh;
};

err_t packet_init_pool(int num_of_pkts);

void packet_pool_free();

BOOL packet_check(struct packet *pkt);

void packet_free(struct packet *pkt);

BOOL packet_data_singlefrag(struct packet *pkt);

void * packet_data_firstfrag(struct packet *pkt);

void packet_clear_flag(struct packet *pkt, UINT32 flag);

struct packet* packet_alloc(void);



//void packet_data_dma(struct packet *pkt, struct xfrag_item *xf, UINT32 len);

#endif