#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
/* Tun/Tap related. */
#include <netinet/ether.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

#include <descsock_client.h>

#define ETHER_HEADER_LEN    sizeof(struct ether_header)
#define IP_HEADER_LEN       sizeof(struct iphdr)

/* We want to hardcode these two becuase they're not likely to change anytime soon */
#define MASTER_SOCKET_PATH  "/var/run/platform/tenant_doorbell.sock"
#define HUGEPAGES_PATH      "/var/huge_pages/2048kB"


static bool init_conf(int argc, char **argv, descsock_client_spec_t *tenant_conf);
static void sys_usage();
static void descsock_client_print_buf(void * buf, int buf_len);
static int tap_open(const char *name, int mtu);
static void tap_fill_macaddr(int fd, uint8_t *mac);

/*
 * Kernel facing read, write calls
 */
static ssize_t tap_read(int tapfd, void *buf, uint32_t len);
static ssize_t tap_write(int tapfd, void *buf, uint32_t len);

/*
 * Descsock facing API, read/write calls
 */
static ssize_t tap_send(int tapfd, void *buf, uint32_t len);
static ssize_t tap_recv(int tapfd, void *buf, uint32_t len);


/*
 * TAP tenant config
 */
struct tenant_conf {
    char ip[sizeof("255.255.255.255")];
    char netmask[sizeof("255.255.255.255")];
    uint8_t mac[ETH_ALEN];
    descsock_client_spec_t *client_spec;
};

enum {
    FLAG_START=127,
    FLAG_TENANT_NAME,
    FLAG_SVC_ID,
    FLAG_TAP_IP,
    FLAG_TAP_NETMASK
};

static const struct option long_opt[] =
{
    {"tenant-name",         required_argument,  0, FLAG_TENANT_NAME},
    {"svc_id",              required_argument,  0, FLAG_SVC_ID},
    {"tenant-ip",           required_argument,  0, FLAG_TAP_IP},
    {"tenant-netmask",      required_argument,  0, FLAG_TAP_NETMASK},
    {0, 0, 0, 0}
};

static struct tenant_conf tenant_conf = {
    .client_spec = NULL,
};


int main(int argc, char *argv[]) {

    int ret = 0;;
    int epoll_timeout_millisecs = 0;
    char *tap_ifname = "if_tap";

    int epfd;
    int tapfd;
    struct epoll_event ev;
    struct epoll_event events[2];

    tenant_conf.client_spec = malloc(sizeof(descsock_client_spec_t));

    if(!init_conf(argc, argv, tenant_conf.client_spec)) {
        printf("Failed to initialize\n");
        exit(EXIT_FAILURE);
    }

    printf("Starting tenant\n"
            "name: %s\n"
            "svc_id %d\n"
            "ip: %s\n"
            "netmask: %s\n"
            "mac: %s\n",
            tenant_conf.client_spec->tenant_name, tenant_conf.client_spec->svc_id, tenant_conf.ip,
                tenant_conf.netmask, tenant_conf.mac);

    /* Initialize descsock library */
    ret = descsock_client_open(tenant_conf.client_spec, 0);
    if(ret == -1) {
        printf("Error client_open\n");
        exit(EXIT_FAILURE);
    }

    epfd = epoll_create(1);
    if(epfd < 0) {
        printf("failed to create epoll: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    tapfd = tap_open(tap_ifname, 0);

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
                    printf("Sending buf\n");
                    descsock_client_print_buf(rxbuf2, read);
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
            printf("Receiving buf\n");
            descsock_client_print_buf(rxbuf, read);

            /* Write data to tap interface */
            tap_write(tapfd, rxbuf, read);

            free(rxbuf);
        }

        usleep(20);
    }

    free(tenant_conf.client_spec);

    descsock_client_close();

    printf("Compile success\n");
    return EXIT_SUCCESS;
}

/* print buf in wireshark format */
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
        printf("%02x ", (unsigned char)p[i]);
    }
    printf("\n\n");
}

/* Open tap interface with IP/mask and mac address */
static int
tap_open(const char *name, int mtu) {
    struct ifreq ifr = {0};
    /* The static MAC address we will use for our TAP interface */
    uint8_t mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0xf5};
    strcpy((char*)tenant_conf.mac, (char*)mac);

    int fd = open("/dev/net/tun", O_RDWR);
    /* general socket to make IOCTL calls to net_device */
    int access_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        printf("Error opening /dev/net/tun");
        return -1;
    }

    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, name, IFNAMSIZ);

    if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
        printf("TUNSETIFF ioctl failed");
        close(fd);
        return -1;
    }

    /*
     * Set the MTU for this tap interface
     * If the user passed MTU via commandline args use that MTU,
     * else use 1500
     */
    ifr.ifr_mtu = (mtu != 0)? mtu : 1500;
    if(ioctl(access_socket, SIOCSIFMTU, &ifr) < 0 ) {
        printf("Setting MTU failed");
        exit(-1);
    }

    /* Set Up static MAC address to make things easier to debug */
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    memcpy(ifr.ifr_hwaddr.sa_data, mac, 6);
    if(ioctl(access_socket, SIOCSIFHWADDR, &ifr) < 0) {
        printf("Failed to SET MAC address.\n");
        exit(0);
    }
    /* Add IP address for TAP interface */
    struct sockaddr_in tap_ip_addr;
    tap_ip_addr.sin_family = AF_INET;
    tap_ip_addr.sin_port = 0;
    inet_pton(AF_INET, tenant_conf.ip, &(tap_ip_addr.sin_addr));
    memcpy(&ifr.ifr_addr, &tap_ip_addr, sizeof(struct sockaddr));
    if(ioctl(access_socket, SIOCSIFADDR, &ifr) < 0) {
        printf("Error setting IP with iocl() ");
        exit(-1);
    }

    /* Add Subnet Mask for TAP interface  */
    struct sockaddr_in tap_subnet_mask;
    tap_subnet_mask.sin_family = AF_INET;
    tap_subnet_mask.sin_port = 0;
    inet_pton(AF_INET, tenant_conf.netmask, &(tap_subnet_mask.sin_addr));
    memcpy(&ifr.ifr_addr, &tap_subnet_mask, sizeof(struct sockaddr));
    if(ioctl(access_socket, SIOCSIFNETMASK, &ifr) < 0) {
        printf("Error setting IP with iocl() ");
        exit(-1);
    }

    /* Enable the interface */
    ifr.ifr_flags = IFF_UP | IFF_RUNNING | IFF_MULTICAST | IFF_PROMISC;
    if(ioctl(access_socket, SIOCSIFFLAGS, &ifr) < 0 ) {
        printf("Enabling Tap interface ");
    }

    /* Get and print MAC */
    printf("MAC address for %s TAP interface\n", name);
    tap_fill_macaddr(fd, mac);

    return fd;
}

/*
 * Fill out an array of u8 from a TAP interface mac address
 */
static void
tap_fill_macaddr(int fd, uint8_t *mac) {
    struct ifreq ifr = {0};
    int i;
    if(fd < 0) {
        printf("Bad tap fd for tap_get_macaddr");
        exit(-1);
    }

   /* Get MAC */
    if(ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        printf("Failed to get MAC address.\n");
        exit(-1);
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

    /* print mac address */
    printf("TAP MAC address: ");
    for(i = 0; i < 6; i++) {
        printf("%02x:", mac[i]);
    }
    printf("\n");
}

/* Read from Kernel */
ssize_t
tap_read(int tapfd, void *buf, uint32_t len)
{
    return read(tapfd, buf, len);
}

/* Write to Kernel */
ssize_t
tap_write(int tapfd, void *buf, uint32_t len)
{
    return write(tapfd, buf, len);
}

/* Send buf to descsock driver */
ssize_t
tap_send(int tapfd, void *buf, uint32_t len)
{
    dsk_ifh_fields_t ifh;
    ifh.did = 4;
    ifh.qos_tier = 0;
    ifh.sep = 0 ;
    ifh.svc = 10;
    ifh.nti = 4095;
    ifh.directed = true;

    return descsock_client_send_extended(&ifh, buf, len, 0);
    //return descsock_client_send(buf, len, 0);
}

/* Receive buf from descsock driver */
ssize_t
tap_recv(int tapfd, void *buf, uint32_t len)
{
    int ret = 0;
    /* Consume 32 packets per poll */
    dsk_ifh_fields_t ifh = { 0 };

    //return descsock_client_recv(buf, DESCSOCK_CLIENT_BUF_SIZE, 0);
    ret = descsock_client_recv_extended(&ifh, buf, DESCSOCK_CLIENT_BUF_SIZE, 0);
    printf("sid=%d sep=%d svc=%d qos=%d nti=%d dm=%d\n",
        ifh.sid, ifh.sep, ifh.svc, ifh.qos_tier, ifh.nti, ifh.dm);

    return ret;
}

extern char *optarg;

/* Parse command line user args to build config for tenant */
static bool
init_conf(int sys_argc, char **sys_argv, descsock_client_spec_t *client)
{
    int c;
    int long_idx  = 1;

    if(sys_argc <= 4) {
        printf("\n");
        sys_usage();
        return false;
    }

    /* set dma path and master socket path */
    snprintf(client->master_socket_path, DESCSOCK_CLIENT_PATHLEN, "%s", MASTER_SOCKET_PATH);
    snprintf(client->dma_shmem_path, DESCSOCK_CLIENT_PATHLEN, "%s", HUGEPAGES_PATH);

    do {
        c = getopt_long(sys_argc, sys_argv, "", long_opt, &long_idx);

        switch(c) {
            case FLAG_SVC_ID:
                client->svc_id = atoi(optarg);
                if(client->svc_id < 0) {
                    printf("Bad serviceid for tenant\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case FLAG_TENANT_NAME:
                snprintf(client->tenant_name, DESCSOCK_CLIENT_PATHLEN, "%s", optarg);
                break;
            case FLAG_TAP_IP:
                strcpy(tenant_conf.ip, optarg);
                break;
            case FLAG_TAP_NETMASK:
                strcpy(tenant_conf.netmask, optarg);
                break;
            case 'h':
            case '?':
            case ':':
                printf("\n");
                sys_usage();
                return (false);
        }


    } while(c != -1);

    return true;
}

static void
sys_usage()
{
    const char *unix_usage =
        "usage: ./tap_tenant --tenant-name=name --svc_id=99  --tenant-ip=192.168.99.99 --tenant-netmask=255.255.255.0\n";

    printf(unix_usage);
}
