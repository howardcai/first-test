#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

/* Tun/Tap related. */
#include <netinet/ether.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

//#include <descsock_client.h>
#include "./src/descsock_client.h"

#define ETHER_HEADER_LEN    sizeof(struct ether_header)
#define IP_HEADER_LEN       sizeof(struct iphdr)

/*
 * Hard coded values used for this client config
 */
#define SVC_ID              1
#define MASTER_SOCKET_PATH  "/var/run/platform/tenant_doorbell.sock"
#define HUGEPAGES_PATH      "/var/huge_pages/2048kB"

/* structure buf used for Tx or Rx  */
struct client_buf {
    void *base;
    uint32_t len;
};

typedef struct {
    char tap_ip[sizeof("255.255.255.255")];
    char tap_netmask[sizeof("255.255.255.255")];
} dsk_tap_net_t;


static dsk_tap_net_t client_tap_info;

uint16_t chksum(uint16_t *buf, int nwords);
void prep_dummypkt(void *buf_base);
void send_packets(int count);
void descsock_client_print_buf(void * buf, int buf_len);
int tap_open(const char *name);

/*
 * Kernel facing read, write calls
 */
size_t tap_read(int tapfd, void *buf, uint32_t len);
size_t tap_write(int tapfd, void *buf, uint32_t len);

/*
 * Descsock facing API, read/write calls
 */
size_t tap_send(int tapfd, void *buf, uint32_t len);
size_t tap_recv(int tapfd, void *buf, uint32_t len);


int main(int argc, char *argv[]) {

    int ret = 0;;
    int epoll_timeout_millisecs = 0;
    char *tap_ifname = "if_tap";

    int epfd;
    int tapfd;
    struct epoll_event ev;
    struct epoll_event events[2];

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

    epfd = epoll_create(1);
    if(epfd < 0) {
        printf("failed to create epoll: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* XXX: add ip address and set interface to up */
    tapfd = tap_open(tap_ifname);

    if(tapfd < 0) {
        printf("Failed to create tap interface %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ev.data.fd = tapfd;
    ev.events = EPOLLIN;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if(ret < 0) {
        printf("Failed to create epoll%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int read = 0;
    int i;
    printf("Wating for traffic\n");
    while(1) {

        /* Poll descsock library first */
        ret = descsock_client_poll(DESCSOCK_POLLIN | DESCSOCK_POLLOUT);

        int evn_count = epoll_wait(epfd, events, 10, epoll_timeout_millisecs);
        for(i = 0; i < evn_count; i++) {

            if(ret & DESCSOCK_POLLERR) {
                printf("descsock library in ERR state\n");
                break;
            }

            /* There is data to be read from Kernel */
            if(events[i].events & EPOLLIN) {
                /* Can we write to descsoc library */
                if(ret & DESCSOCK_POLLOUT) {
                    void *rxbuf2 = malloc(DESCSOCK_CLIENT_BUF_SIZE);
                    read = tap_read(events[i].data.fd, rxbuf2, 2048);
                    /* Write packet to descsock library */
                    tap_send(events[i].data.fd, rxbuf2, read);

                    free(rxbuf2);
                }
                else {
                    /* drop tx packet, there is no room in descsock library */
                    printf("Dropping packet descsock framework is full\n");
                }
            }
        }
        /* descsock framework has data for us to read */
        if(ret & DESCSOCK_POLLIN) {
            void *rxbuf = malloc(DESCSOCK_CLIENT_BUF_SIZE);
            /* read data from descsock lib */
            read = tap_recv(tapfd, rxbuf, 2048);
            /* Write data to tap interface */
            tap_write(tapfd, rxbuf, read);

            free(rxbuf);
        }

        usleep(20);
    }

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

int tap_open(const char *name) {
    struct ifreq ifr = {{{0}}} ;
    int fd = -1, res = -1;

    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        printf("Error opening /dev/net/tun");
        return fd;
    }

    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, name, IFNAMSIZ);

    res = ioctl(fd, TUNSETIFF, &ifr);
    if (res < 0) {
        printf("TUNSETIFF ioctl failed");
        close(fd);
        return res;
    }

    return fd;
}

size_t tap_read(int tapfd, void *buf, uint32_t len)
{
    return read(tapfd, buf, len);
}
size_t tap_write(int tapfd, void *buf, uint32_t len)
{
    return write(tapfd, buf, len);
}
size_t tap_send(int tapfd, void *buf, uint32_t len)
{
    return descsock_client_send(buf, len, 0);
}
size_t tap_recv(int tapfd, void *buf, uint32_t len)
{
    /* Consume 32 packets per poll */
    return descsock_client_recv(buf, DESCSOCK_CLIENT_BUF_SIZE, 0);
}
