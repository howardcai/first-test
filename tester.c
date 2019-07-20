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

#define ETHER_HEADER_LEN    sizeof(struct ether_header)
#define IP_HEADER_LEN       sizeof(struct iphdr)


uint16_t chksum(uint16_t *buf, int nwords);
void prep_dummypkt(void *buf_base);

struct generic_softc {
    struct descsock_softc *sc;
    int epfd;
    struct epoll_event events[8];
};

int main(int argc, char *argv[]) {

    /*
     * Init generic softc
     */
    struct generic_softc *generic_dev = malloc(sizeof(struct generic_softc));
    if(generic_dev == NULL) {
        print("Error allocating generic softc\n");
        exit(EXIT_FAILURE);
    }
    /*
     * Init descosck framework
     */
    generic_dev->sc = descsock_init(argc, argv);
    if(generic_dev->sc == NULL) {
        printf("FAiled to init\n");
        exit(EXIT_FAILURE);
    }

    /* Alocate an empty buf from out internal descsock mem allocator */
    struct xfrag_item *buf = xfrag_alloc();
    if(buf == NULL) {
        printf("null exfrag\n");
        exit(EXIT_FAILURE);
    }

    prep_dummypkt(buf->base);

    descsock_send(generic_dev->sc, buf->base);



    xfrag_free(buf);

    descsock_teardown(generic_dev->sc);

    printf("test compile success\n");
    return EXIT_SUCCESS;
}

void prep_dummypkt(void *buf_base)
{
     /* Add ether header to buf */
    struct ether_header *eth = (struct ether_header *)buf_base;
    eth->ether_shost[0] = 0x00;
    eth->ether_shost[1] = 0x00;
    eth->ether_shost[2] = 0x00;
    eth->ether_shost[3] = 0x00;
    eth->ether_shost[4] = 0x00;
    eth->ether_shost[5] = 0xf5;

    /* add IP header to buf */
    struct iphdr *ip = (struct iphdr *) (buf_base + ETHER_HEADER_LEN);
    ip->ttl = 64;
    ip->tot_len = ETHER_HEADER_LEN + IP_HEADER_LEN;
    ip->protocol = 1;
    ip->check = chksum( (uint16_t*)buf_base,  ETHER_HEADER_LEN + IP_HEADER_LEN);

    //XXX: add ip source and dest

}

uint16_t chksum(uint16_t *buf, int nwords)
{
        uint64_t sum;

        for(sum = 0; nwords > 0; nwords--) {
             sum += *buf++;
        }

        sum = (sum >> 16) + (sum &0xffff);
        sum += (sum >> 16);

        return (uint16_t)(~sum);

}