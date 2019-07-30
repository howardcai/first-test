#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <sys/epoll.h>

#include "descsock_client.h"

#define ETHER_HEADER_LEN    sizeof(struct ether_header)
#define IP_HEADER_LEN       sizeof(struct iphdr)

/*
 * Hard coded values used for this client config
 */
#define SVC_ID              1
#define MASTER_SOCKET_PATH  "/run/dmaa_doorbell.sock"
#define HUGEPAGES_PATH      "/var/hugepages"


uint16_t chksum(uint16_t *buf, int nwords);
void prep_dummypkt(void *buf_base);


int main(int argc, char *argv[]) {

    descsock_client_tx_buf_t *txbuf;
    void *rxbuf;
    int ret = 0;;

    descsock_client_spec_t *client = malloc(sizeof(descsock_client_spec_t));

    /* set configs for this client */
    snprintf(client->master_socket_path, DESCSOCK_PATHLEN, "%s", MASTER_SOCKET_PATH);
    snprintf(client->dma_shmem_path, DESCSOCK_PATHLEN, "%s", HUGEPAGES_PATH);
    client->svc_id = SVC_ID;


    /* Initialize descsock library */
    ret = descsock_client_open(client, 0);
    if(ret == -1) {
        printf("Error client_open\n");
        exit(EXIT_FAILURE);
    }

    /* Allocate a send buf from descsock library */
    txbuf = descsock_client_alloc_buf();

    /* Allocate a receive buf to revc data into */
    rxbuf = malloc(DESCSOCK_BUF_SIZE);

    if(txbuf == NULL || rxbuf == NULL) {
        printf("buf is null\n");
        exit(EXIT_FAILURE);
    }

    /* dummy packet */
    prep_dummypkt(txbuf->base);
    txbuf->len = ETHER_HEADER_LEN + IP_HEADER_LEN + sizeof(descsock_client_tx_buf_t);

    printf("Sending buf %p %lu\n", txbuf->base, (uint64_t)txbuf->base);
    ret = descsock_client_send(txbuf, txbuf->len, 0);
    if(!ret) {
        printf("Failed to send\n");
        exit(EXIT_FAILURE);
    }

    sleep(2);

    while(1) {

        /* Poll descsock lib */
        ret = descsock_client_poll(0);
        if(ret) {

            /* You got mail! */
            printf("Packets ready to be consumed\n");
            descsock_client_recv(rxbuf, DESCSOCK_BUF_SIZE, 0);
           // break;
        }
        else {
            printf("no packets\n");
        }

        txbuf = descsock_client_alloc_buf();
        printf("Sending buf %p %lu\n", txbuf->base, (uint64_t)txbuf->base);
        descsock_client_send(txbuf, txbuf->len, 0);

        sleep(2);
    }


    descsock_client_free_buf(txbuf);

    free(rxbuf);
    free(client);

    descsock_client_close();

    printf("Compile success\n");
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