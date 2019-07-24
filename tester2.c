#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <sys/epoll.h>

#include "descsock_client.h"

#define ETHER_HEADER_LEN    sizeof(struct ether_header)
#define IP_HEADER_LEN       sizeof(struct iphdr)

#define SVC_ID              1
#define MASTER_SOCKET_PATH  "/run/dmaa_doorbell.sock"
#define HUGEPAGES_PATH      "/var/hugepages"

uint16_t chksum(uint16_t *buf, int nwords);
void prep_dummypkt(void *buf_base);


int main(int argc, char *argv[]) {

    void *buf;
    int res;

    descsock_client_spec_t *client = malloc(sizeof(descsock_client_spec_t));

    snprintf(client->master_socket_path, DESCSOCK_PATHLEN, "%s", MASTER_SOCKET_PATH);
    snprintf(client->dma_shmem_path, DESCSOCK_PATHLEN, "%s", HUGEPAGES_PATH);
    client->svc_id = SVC_ID;


    res = descsock_client_open(client, 0);
    if(res == -1) {
        printf("Error client_open\n");
        exit(EXIT_FAILURE);
    }



    buf = malloc(DESCSOCK_BUF_SIZE);
    prep_dummypkt(buf);


    while(1) {

        descsock_client_send(buf, 2048, 0);
        sleep(2);

        descsock_client_poll(0);
        sleep(2);
    }


    printf("Compile success\n");
    free(client);

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