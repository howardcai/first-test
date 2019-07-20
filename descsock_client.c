#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <sys/epoll.h>

#include "src/descsock.h"
#include "src/kern/sys.h"
#include "src/kern/sys.h"
#include "src/net/xfrag_mem.h"

#include "descsock_client.h"


int
descsock_client_open(descsock_client_spec_t * const spec, const int flags)
{
    descsock_getpid();
    return 0;
}

int
descsock_poll2(int event_mask)
{
    sys_usage();
    return 0;
}

int
descsock_client_send(const void * const buf, const uint64_t len, const int flags)
{
    return 0;
}

int
descsock_client_recv(void * const buf, const uint64_t len, const int flags)
{
    return 0;
}

int
descsock_client_cntl(const int cmd, ...)
{
    return 0;
}

int
descsock_client_close(void)
{
    return 0;
}
