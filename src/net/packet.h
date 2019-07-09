#ifndef PACKET_H
#define PACKET_h

#include "sys/types.h"


#define PACKET_FLAG_LOCKED 1

struct packet {
    void *addr;
    UINT32 len;
};

#endif