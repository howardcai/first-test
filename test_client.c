#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <sys/epoll.h>

#include <descsock_client.h>

#define ETHER_HEADER_LEN    sizeof(struct ether_header)
#define IP_HEADER_LEN       sizeof(struct iphdr)

/* structure buf used for Tx or Rx  */
struct client_buf {
    void *base;
    uint32_t len;
};


/*
 * Hard coded values used for this client config
 */
#define SVC_ID              1
#define MASTER_SOCKET_PATH  "/run/dmaa_doorbell.sock"
#define HUGEPAGES_PATH      "/var/hugepages"


uint16_t chksum(uint16_t *buf, int nwords);
void prep_dummypkt(void *buf_base);
void send_packets(int count);
void descsock_client_print_buf(void * buf, int buf_len);


int main(int argc, char *argv[]) {

    void *rxbuf;
    int ret = 0;;
    int recv_count = 0;

    descsock_client_spec_t *client = malloc(sizeof(descsock_client_spec_t));

    /* set configs for this client */
    snprintf(client->master_socket_path, DESCSOCK_CLIENT_PATHLEN, "%s", MASTER_SOCKET_PATH);
    snprintf(client->dma_shmem_path, DESCSOCK_CLIENT_PATHLEN, "%s", HUGEPAGES_PATH);
    client->svc_id = SVC_ID;


    /* Initialize descsock library */
    ret = descsock_client_open(client, 0);
    if(ret == -1) {
        printf("Error client_open\n");
        exit(EXIT_FAILURE);
    }

    /* To receive data into */
    rxbuf = malloc(DESCSOCK_CLIENT_BUF_SIZE);
    if(rxbuf == NULL) {
        printf("rxbuf returned null on malloc\n");
        exit(EXIT_FAILURE);
    }
    int read = 0;
    while(1) {

        /* Poll descsock lib */
        ret = descsock_client_poll(DESCSOCK_POLLIN | DESCSOCK_POLLOUT);

        if(ret & DESCSOCK_POLLERR) {
            printf("descsock library in ERR state\n");
            break;
        }

        /*
         * Is descsock readable
         */
        if(ret & DESCSOCK_POLLIN) {

            /*
             * There is data to be read from descsock
             * Read until we have not more Rx packets
             */
            while((read = descsock_client_recv(rxbuf, DESCSOCK_CLIENT_BUF_SIZE, 0))) {

                /*
                 * consume packets in rxbuf
                 */
                descsock_client_print_buf(rxbuf, read);
                recv_count++;
            }

            printf("Recieved %d packets\n", recv_count);
            recv_count = 0;
        }

        /*
         * Can we write to descsock
         */
        if(ret & DESCSOCK_POLLOUT) {

            /*
             * Send some packets
             */
            printf("Sending 10 packet\n");
            send_packets(1);
        }

        sleep(1);
    }


    //descsock_client_free_buf(txbuf);

    free(rxbuf);
    free(client);

    descsock_client_close();

    printf("Compile success\n");
    return EXIT_SUCCESS;
}

void
descsock_client_print_buf(void * buf, int buf_len)
{
    char *p = (char *)buf;
    int i;

    if (buf == NULL || buf_len < 0) {
        printf("NULL buf\n");
		return;
    }

    printf("Buf: %p Length: %d\n", buf, buf_len);

    for (i = 0; i < buf_len; i++) {
        if (i % 32 == 0) {
            printf("\n%06d ", i);
        }
        printf("%02x ", p[i]);
    }
    printf("\n\n");
}

/*
 * Allocate <count> dummy packet to send
 */
void send_packets(int count)
{
    int i;

    struct client_buf bufs[count];

     /*Allocate mem for 10 packets */
    for(i = 0; i < count; i++) {
        bufs[i].base = malloc(DESCSOCK_CLIENT_BUF_SIZE);
        if(bufs[i].base == NULL) {
            printf("FAiled\n");
            exit(EXIT_FAILURE);
        }

        prep_dummypkt(bufs[i].base);
        bufs[i].len = ETHER_HEADER_LEN + IP_HEADER_LEN + sizeof(struct client_buf);
    }

    /* send packets */
    for(i = 0; i < count; i++) {
        descsock_client_send(bufs[i].base, bufs[i].len, 0);
    }

    /*Free sent packets */
    for(i = 0; i < count; i++) {
        free(bufs[i].base);
    }

}
void prep_dummypkt(void *buf_base)
{
     /* Add ether header to buf */
    struct ether_header *eth = (struct ether_header *)buf_base;
    eth->ether_dhost[0] = 0x00;
    eth->ether_dhost[1] = 0x00;
    eth->ether_dhost[2] = 0x00;
    eth->ether_dhost[3] = 0x00;
    eth->ether_dhost[4] = 0x00;
    eth->ether_dhost[5] = 0x06;

    eth->ether_shost[0] = 0x00;
    eth->ether_shost[1] = 0x00;
    eth->ether_shost[2] = 0x00;
    eth->ether_shost[3] = 0x00;
    eth->ether_shost[4] = 0x00;
    eth->ether_shost[5] = 0x05;
    eth->ether_type = 0x0800;



    /* add IP header to buf */
    struct iphdr *ip = (struct iphdr *) (buf_base + ETHER_HEADER_LEN);
    ip->ttl = 64;
    ip->tot_len = ETHER_HEADER_LEN + IP_HEADER_LEN;
    ip->protocol = 1;
    ip->version = 4;
    ip->saddr = 3232235777; /* 192.168.1.1 */
    ip->daddr = 3232235876; /* 192.168.1.100 */
    //ip->check = chksum( (uint16_t*)buf_base,  ETHER_HEADER_LEN + IP_HEADER_LEN);

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
