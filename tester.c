#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <net/ethernet.h>

#include "src/descsock.h"
#include "src/kern/sys.h"
#include "src/kern/sys.h"
#include "src/net/xfrag_mem.h"

#define ETHER_HEADER_LEN    sizeof(struct ether_header)
#define IP_HEADER_LEN       sizeof(struct iphdr)


uint16_t chksum(uint16_t *buf, int nwords);


int main(int argc, char *argv[]) {


    /* Init descosck framework */
    struct descsock_softc *sc = descsock_init(argc, argv);
    if(sc == NULL) {
        printf("FAiled to init\n");
        exit(EXIT_FAILURE);
    }

    /* Alocate a empty buf from out internal descsock mem allocator */
    struct xfrag_item *buf = xfrag_alloc();

    /* Add ether header to buf */
    struct ether_header *eth = (struct ether_header *)buf->base;
    eth->ether_shost[0] = 0x00;
    eth->ether_shost[1] = 0x00;
    eth->ether_shost[2] = 0x00;
    eth->ether_shost[3] = 0x00;
    eth->ether_shost[4] = 0x00;
    eth->ether_shost[5] = 0xf5;

    /* add IP header to buf */
    struct iphdr *ip = (struct iphdr *) (buf->base + ETHER_HEADER_LEN);
    ip->ttl = 64;
    ip->tot_len = ETHER_HEADER_LEN + IP_HEADER_LEN;
    ip->protocol = 1;
    ip->check = chksum( (uint16_t*)buf->base,  ETHER_HEADER_LEN + IP_HEADER_LEN);



    xfrag_free(buf);



    descsock_teardown(sc);

    printf("test compile success\n");
    return EXIT_SUCCESS;
}

uint16_t chksum(uint16_t *buf, int nwords)
{
        unsigned long sum;

        for(sum=0; nwords>0; nwords--) {
             sum += *buf++;
        }

        sum = (sum >> 16) + (sum &0xffff);
        sum += (sum >> 16);

        return (uint16_t)(~sum);

}