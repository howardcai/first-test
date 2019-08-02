/*
 * $F5Copyright_C:
 * Copyright (C) F5 Networks, Inc. 1996-2006
 *
 * No part of the software may be reproduced or transmitted in any
 * form or by any means, electronic or mechanical, for any purpose,
 * without express written permission of F5 Networks, Inc. $
 */

#ifndef ERR_H
#define ERR_H

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

#endif