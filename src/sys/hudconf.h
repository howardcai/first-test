/*
 * $F5Copyright_C:
 * Copyright (C) F5 Networks, Inc. 1996-2006, 2010-2013
 *
 * No part of the software may be reproduced or transmitted in any
 * form or by any means, electronic or mechanical, for any purpose,
 * without express written permission of F5 Networks, Inc. $
 */
#ifndef _HUDCONF_H
#define _HUDCONF_H

#include "types.h"

#define MAX_DEVICE      32
#define MAX_VLANGROUPS  10
#define MAX_VLANS       10

#ifndef MAX_NODES
#define MAX_NODES       4    /* make sure power of 2 */
#endif

#define NODE_MASK       (MAX_NODES - 1)
#define NODE_UNSPEC     (-MAX_NODES)
#define MAX_ZONES       MAX_NODES

#define DEFAULT_YIELD_PERCENT        10
#define MAX_YIELD_PERCENT            99
#define SINGLE_CPU_MIN_YIELD_PERCENT 3

#define DEFAULT_REBALANCE_CEILING    99
#define MAX_REBALANCE_CEILING        99

/*
 * MAX_SHARED_PHYS_SIZE: Max memory that will be shared between tmms.
 * This is used by device drivers, vCMP and other things.
 *
 * VA_RESERVE: Virtual address space set aside for shared memory
 * mapping etc.  This comes from pmap and estimates for SysV shared
 * memory, memory mapped devices, mapped files like tmctl, etc.
 *
 * MIN_TMM_HEAPSIZE: Min memory that the tmm will run with.  This is a
 * SWAG.
 */

/*
 * This was 24MB but increased to 32MB to avoid problems on Centaur/stirling.
 * This is taking 8MB away from normal TMM shared memory for all platforms.
 * XXX: If this setting were configurable we could optimize per platform.
*/
#define MAX_SHARED_PHYS_SIZE    32 /* MB */

#define VA_RESERVE              (64 + MAX_SHARED_PHYS_SIZE) /* MB */
#define MIN_TMM_HEAPSIZE        104 /* MB */
#define MAX_SHARED_PHYS_BYTES   (MAX_SHARED_PHYS_SIZE<<20)
#define MAX_SHARED_PHYS_ENTRIES (MAX_SHARED_PHYS_BYTES/HPAGE_SIZE)
#define MIN_TMM_MEMSIZE         (MIN_TMM_HEAPSIZE+MAX_SHARED_PHYS_SIZE)

enum shmx_mode {
    SHMX_NONE = 0,
    SHMX_MASTER,
    SHMX_DUT
};

#define STANDALONE_PACKET    (1 << 0)
#define STANDALONE_TRANSPORT (1 << 1)

struct hudconf {
    BOOL            ifvnic;
    int             realtime;
    BOOL            attachall;
    BOOL            syslog_stderr;
    char            *speed_test;
    UINT32          speed_test_delay; /* in ticks */
    char            *init_command;
    char            *command;
    char           *platform;
    UINT32          cluster_vlan_id;
    UINT32          numamaps;
    int             npus;
    UINT64          clusterid;
    int             nice;
    int             yield_percent;
    BOOL            busypoll;
    BOOL            rebalance;
    UINT32          rebalance_ceiling;
    BOOL            rebalance_backoff_min_sleep;
    int             testlen;
    int             testpkts;
    int             packet_drop;
    int             packet_reorder;
    SIZE            memsize;
    BYTE           *heap_base;
    SIZE            heap_size;
    SIZE            stacksize;
    BOOL            phys_addr_enabled;
    BOOL            devs_use_dev_index;
    UINT32          unit_mask;
    char            dag_parport[64];
    UINT32          dag_fpga_pci_addr;
    BOOL            vm_guest;
    BOOL            ignore_bigstart;
    BOOL            no_baseconfig;
    UINT16          dag_mod_id;
    UINT16          dag_egress_mod_id;
    BOOL            rate_limited;
    enum shmx_mode  shmx_mode;
    BOOL            cc_fips_mode;
    BOOL            cc_mode;
    BOOL            is_vadc; /* Virtual ADC */
    BOOL            shared_arl_enabled;
    BOOL            is_unit_test;
    BOOL            sadc;
    BOOL            wait_for_gdb;
    BOOL            use_loopback;
    UINT8           standalone;
    const char      *descsock_cfg_string;
    BOOL            is_madc; /* TMM in container */
};

struct hudthread {
    char            *clientciphers;
    char            *serverciphers;
    int             tmid;
    int             cpu;
    int             node;
    int             core;
    UINT32          npgs;
    UINT16          pgid;
    UINT16          virtual_id;
    BOOL            is_worker;
};

extern struct hudconf hudconf;
//extern RTTHREAD struct hudthread hudthread;

#endif /* OPTIONS_H */
