/*
 * $F5Copyright_C:
 * Copyright (C) F5 Networks, Inc. 1996-2006, 2010, 2012-2015
 *
 * No part of the software may be reproduced or transmitted in any
 * form or by any means, electronic or mechanical, for any purpose,
 * without express written permission of F5 Networks, Inc. $
 */
#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

#define GLOBALSET
#define BUF_SIZE                2048
/*
 * TMM types
 */
typedef char                    INT8;
typedef unsigned char           UINT8;
typedef unsigned char           BYTE;
typedef short                   INT16;
typedef unsigned short          UINT16;
typedef int                     INT32;
typedef unsigned int            UINT32;
typedef long long               INT64;
typedef unsigned long long      UINT64;
#ifdef __x86_64__
typedef __uint128_t             UINT128;
#endif
typedef unsigned long           UINTPTR;
typedef long                    INTPTR;

typedef unsigned long           SIZE;

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
typedef enum { FALSE, TRUE }    BOOL;
typedef double                  DOUBLE;

typedef unsigned short vlan_t;

/*
 * Basic file I/O
 */

typedef int file_t;

struct tmm_memory {
    char name[256];
    void *base;
    UINT32 length;
};

typedef struct {
    char name[256];
} f5dev_t;

typedef struct {
    char name[256];
}f5device_t;

struct dev_poll_param {
    char name[256];
};

struct ifnet {
    char name[256];
};

struct client_rx_buf {
    void *base;
    UINT32 len;
    UINT64 idx;
};

typedef struct {
    void *base;
    UINT32 len;
    UINT64 idx;
} client_tx_buf_t;

#endif /* _TYPES_H */