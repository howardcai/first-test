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

struct hudconf {
    UINT64          memsize;
    UINT32          queue_size;
    BOOL            physmem;
    BOOL            virtmem;
    BOOL            use_tap;
    char            *hugepages_path;
    char            *mastersocket;
    void            *dma_seg_base;
    UINT64          dma_seg_size;
    UINT32          num_seps;
    UINT32          svc_ids;
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
