/*
 * $F5Copyright_C:
 * Copyright (C) F5 Networks, Inc. 1996-2006, 2010, 2012-2015
 *
 * No part of the software may be reproduced or transmitted in any
 * form or by any means, electronic or mechanical, for any purpose,
 * without express written permission of F5 Networks, Inc. $
 */
#ifndef _TYPES_H
#define _TYPES_H

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
#ifndef _LP64
typedef unsigned long long      UINTMAX;
typedef long long               INTMAX;
#else
typedef unsigned long           UINTMAX;
typedef long                    INTMAX;
#endif
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
typedef enum { FALSE, TRUE }    BOOL;

typedef unsigned long           SIZE;

/*
 * Basic file I/O
 */

typedef int file_t;

/*
 * error codes
 *
 * Note: when updating this enum, be sure to also update the
 * text conversion array in stdio.c with the new entry.
 */
typedef enum {
    ERR_OK = 0,                 /* No error, everything OK. */
    ERR_MEM,                    /* Out of memory error.     */
    ERR_BUF,                    /* Buffer error.            */
    ERR_VAL,                    /* Illegal value.           */
    ERR_ARG,                    /* Illegal argument.        */
    ERR_BOUNDS,                 /* Out of bounds            */
    ERR_TOOBIG,                 /* Packet or data too big   */
    ERR_ABRT,                   /* Connection aborted.      */
    ERR_CONN,                   /* Not connected.           */
    ERR_VLAN,                   /* Vlan error.              */
    ERR_RTE,                    /* Routing problem.         */
    ERR_NEIGHBOR,               /* Neighbor problem.        */
    ERR_USE,                    /* Address in use.          */
    ERR_PORT,                   /* Out of ports.            */
    ERR_IO,                     /* Input/Output error.      */
    ERR_EXPIRED,                /* Expired data.            */
    ERR_REJECT,                 /* Traffic rejected.        */
    ERR_MORE_DATA,              /* More data required.      */
    ERR_NOT_FOUND,              /* Not found.               */
    ERR_HUD_ABORT,              /* Hudfilter abort.         */
    ERR_HUD_TEARDOWN,           /* Hudfilter teardown.      */
    ERR_INPROGRESS,             /* Operation now in progress      */
    ERR_MCP,                    /* MCP error or unsupported message */
    ERR_DB,                     /* Database error */
    ERR_TCL,                    /* TCL error */
    ERR_DL,                     /* Dynamic library error    */
    ERR_ISCONNECTED,            /* Already connected        */
    ERR_HA_UNIT_NOT_ACTIVE,     /* The HA unit is not active for this TMM */
    ERR_NOTINPROGRESS,          /* Prerequisite operation not in progress */
    ERR_TYPE,                   /* Type mismatch            */
    ERR_VERSION,                /* Improper version         */
    ERR_INIT,                   /* Initialization error     */
    ERR_UNKNOWN,                /* Unknown error.           */
    ERR_NOT_SUPPORTED,          /* Unsupported action.      */
    ERR_LICENSE,                /* Feature not licensed.    */
    ERR_ALIGN,                  /* Buffer alignment error   */
    ERR_TIMEOUT,                /* Operation timed out. */
    ERR_ENCAP_FAILED,           /* Encapsulation failed.    */
    ERR_VERBOTEN,               /* Operation not allowed.   */
    ERR_TUNNEL,                 /* Tunnel error.            */
    ERR_WOULDBLOCK,             /* Resource temporarily unavailable. */
    ERR_OPENSSLRNG,             /* OpenSSL RNG call failed. */
    ERR_RETRY,                  /* Retry the operation */
    ERR_DUPLICATE_ENTRY,        /* Duplicate entry, item already exists */
    ERR_LAST
} err_t;

struct tmm_memory {
    char name[256];
    void *base;
    UINT32 length;
};

struct xfrag {
    void *base;
    UINT32 len;
};
struct packet {
    void *addr;
    UINT32 len;
};

/*
 * Circular fixed-sized queue definitions.
 */
#define FIXEDQ(name, type, size)                                        \
struct name {                                                           \
    unsigned int qhead;                                                 \
    unsigned int qtail;                                                 \
    unsigned int qfree;                                                 \
    type q[size];                                                       \
}

#define FIXEDQ_TAIL(fixedq)                                             \
    ((fixedq).q[ ((fixedq).qhead + FIXEDQ_COUNT(fixedq) - 1) %          \
        (sizeof((fixedq).q)/sizeof((fixedq).q[0])) ])
#define FIXEDQ_HEAD(fixedq)     ((fixedq).q[(fixedq).qhead])
#define FIXEDQ_EMPTY(fixedq)                                            \
    (((fixedq).qhead == (fixedq).qtail) && ((fixedq).qfree != 0))
#define FIXEDQ_FULL(fixedq)     ((fixedq).qfree == 0)
#define FIXEDQ_COUNT(fixedq)                                            \
    ((sizeof((fixedq).q)/sizeof((fixedq).q[0])) - (fixedq).qfree)
#define FIXEDQ_FREE(fixedq)     (fixedq).qfree

#define FIXEDQ_INIT(fixedq) do {                                        \
    (fixedq).qhead = 0;                                                 \
    (fixedq).qtail = 0;                                                 \
    (fixedq).qfree = (sizeof((fixedq).q)/sizeof((fixedq).q[0]));        \
} while (0)

#define FIXEDQ_INITIALIZER(fixedq)                                      \
    { qhead: 0, qtail: 0, qfree: (sizeof((fixedq).q)/sizeof((fixedq).q[0])) }

#define FIXEDQ_INSERT(fixedq, elm) do {                                 \
    if (!FIXEDQ_FULL(fixedq)) {                                         \
        (fixedq).q[(fixedq).qtail] = elm;                               \
        (fixedq).qtail = ((fixedq).qtail + 1) %                         \
                (sizeof((fixedq).q)/sizeof((fixedq).q[0]));             \
        --(fixedq).qfree;                                               \
    }                                                                   \
} while (0)

#define FIXEDQ_INSERT_DIRECT(fixedq, elm) do {                                 \
    if (!FIXEDQ_FULL(fixedq)) {                                         \
        *elm = &((fixedq).q[(fixedq).qtail]);                               \
        (fixedq).qtail = ((fixedq).qtail + 1) %                         \
                (sizeof((fixedq).q)/sizeof((fixedq).q[0]));             \
        --(fixedq).qfree;                                               \
    }                                                                   \
} while (0)

/*
 * Alternate insert idiom: returns a pointer to the next free slot,
 * or NULL if the queue is full.  Use this to avoid the need to
 * copy into the queue -- especially useful if your entries contain
 * copy-unsafe structures like xbufs.
 */
#define FIXEDQ_ALLOC(fixedq) ( \
    FIXEDQ_FULL(fixedq) ? NULL : ( \
        ( (fixedq).qtail = ((fixedq).qtail + 1) % \
                (sizeof((fixedq).q)/sizeof((fixedq).q[0])) ),  \
        ( --(fixedq).qfree ), \
        ( &FIXEDQ_TAIL(fixedq) ) \
    ) \
)

#define FIXEDQ_REMOVE(fixedq) do {                                      \
    if (!FIXEDQ_EMPTY(fixedq)) {                                        \
        (fixedq).qhead = ((fixedq).qhead + 1) %                         \
                (sizeof((fixedq).q)/sizeof((fixedq).q[0]));             \
        ++(fixedq).qfree;                                               \
    }                                                                   \
} while (0)

#endif /* _TYPES_H */