/*
 * $F5Copyright_C:
 * Copyright (C) F5 Networks, Inc. 1996-2006, 2010, 2012-2015
 *
 * No part of the software may be reproduced or transmitted in any
 * form or by any means, electronic or mechanical, for any purpose,
 * without express written permission of F5 Networks, Inc. $
 */
#ifndef FIXED_QUEUE_H
#define FIXED_QUEUE_H
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


#endif /*_FIXED_QUEUE_H */