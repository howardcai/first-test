//#include "gnu_source.h"

#include "gnu_source.h"
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "types.h"



/*
 * Warning:  This function performs a potentially blocking ioctl() system call.
 */
err_t
sys_set_non_blocking(int fd, int non_blocking)
{
    return (ioctl(fd, FIONBIO, &non_blocking) == 0) ? ERR_OK : ERR_UNKNOWN;
}

BOOL
sys_file_descriptor_exists(int fd)
{
    return (fcntl(fd, F_GETFD, 0) < 0) ? FALSE : TRUE;
}

/*
 * Add descsock unix domain socket wrappers
 */

/* Open a unix socket via the given path*/
int
descsock_get_unixsocket(char *path)
{
    struct sockaddr_un skt;
    int ret = 1;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) {
        perror("descsock_get_unixskt()\n");
        return -1;
    }
    memset(&skt, 0, sizeof(skt));
    skt.sun_family = AF_UNIX;
    strncpy(skt.sun_path, path, sizeof(skt.sun_path) -1);

    ret = connect(fd, (struct sockaddr *)&skt, sizeof(skt));
    if(ret < 0) {
        perror("connect to skt\n");
        return -1;
    }

    return fd;
}

int
descsock_poll_fd(int fd, int num_of_fds, int timeout)
{
    return 0;
}

/*
 * Wrapper function for write()
 * return 0 if the socket is not available for write which means busy
 * return -1 for an actual error
 * return n where n > 0 bytes written to socket
 * Non-blocking system call
 */

int
descsock_socket_write(int fd, void * buf, UINT32 len)
{
    int n = write(fd, buf, len);
    if(n == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        else {
            /* error occoured */
            perror("descsock_socket_write");
            return -1;
        }
    }
    /* return the actual written bytes */
    return n;
}

/*
 * Wrapper function for read()
 * return 0 if the socket is not available for read which means busy
 * return -1 for an actual error
 * return n where n > 0 bytes read from socket
 * Non-blocking system call
 */
int
descsock_socket_read(int fd, void * buf, UINT32 len)
{
    int n = read(fd, buf, len);
    if(n == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        else {
            /* error occoured */
            perror("descsock_socket_read");
            return -1;
        }
    }
    /* return the actual written bytes */
    return n;
}

int
descsock_get_errno(void) {
    return errno;
}

/* Map a DMA area using mmap, returns the base address of mmap */
void *
descsock_map_dmaregion(char * path, UINT64 size)
{
    void * base_virt;
    int fd = open(path, O_CREAT | O_RDWR, S_IRWXU);
    if(fd == -1) {
        perror("---------- Failed to open path");
        return NULL;
    }

    base_virt = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED , fd, 0);
    if(base_virt == MAP_FAILED) {
        perror("-------------- descsock_map_mempool failed");
        return NULL;
    }

    return base_virt;
}

int
descsock_getpid(void) {
    return getpid();
}


/* Create an epoll instance and return the fd */
int
descsock_epoll_create(int size)
{
    int res = epoll_create(size);
    if(res < 0) {
        perror("descsock_epoll_create()");
        return -1;
    }

    return res;
}

/* Wrapper for creating an epoll instance, return fd, -1 for failure */
int
descsock_epoll_create1(int flags)
{
    int res = epoll_create1(flags);
    if(res < 0) {
        perror("descsock_epoll_create1()");
        return -1;
    }

    return res;
}

/* Wrapper for epoll_ctl(), return -1 if failure */
int
descsock_epoll_ctl(int epfd, int op, int fd, struct epoll_event *events)
{
    int res =  epoll_ctl(epfd, op, fd, events);
    if(res < 0) {
        perror("descsock_epoll_ctl()");
        return -1;
    }

    return res;
}

/* Warpper for epoll_wait() */
int
descsock_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
    int res = epoll_wait(epfd, events, maxevents, timeout);
    if(res < 0) {
        perror("descsock_epoll_wait()");
        return -1;
    }
    return res;
}

/* Recieve and parse messages/file descriptors from the DMA Agent */
err_t
descsock_recv_socket_conns(int fd, int * socks)
{
    /* since there is an rx socket and tx socket for every tier */
    unsigned int num_socks = 4 * 2;
    struct mmsghdr msgvec[num_socks];
    int flags = 0;
    struct timespec * timeout;
    struct msghdr msg[num_socks];
    unsigned int vec_len;
    struct cmsghdr * cmsghdr[num_socks];
    struct iovec iov[num_socks];
    char buf[num_socks][CMSG_SPACE(sizeof(int))];
    char c[num_socks];

    memset(msgvec, 0, sizeof msgvec);
    memset(iov, 0, sizeof(iov));
    memset(c,'\0', sizeof c);

    vec_len = num_socks;
    timeout = NULL;    /* blocks indefinitely */

    /* grab individual messages */
    int i;
    for (i = 0; i < num_socks; i++) {
        iov[i].iov_base = &c[i];
        iov[i].iov_len = sizeof c[i];
        memset(buf[i], 0x0d, sizeof(buf[i]));
        cmsghdr[i] = (struct cmsghdr *)buf[i];
        cmsghdr[i]->cmsg_len = CMSG_LEN(sizeof(int));
        cmsghdr[i]->cmsg_level = SOL_SOCKET;
        cmsghdr[i]->cmsg_type = SCM_RIGHTS;
        msg[i].msg_name = NULL;
        msg[i].msg_namelen = 0;
        msg[i].msg_iov = &iov[i];
        msg[i].msg_iovlen = 1;
        msg[i].msg_control = cmsghdr[i];
        msg[i].msg_controllen = CMSG_LEN(sizeof(int));
        msg[i].msg_flags = 0;

        memcpy(&msgvec[i].msg_hdr, &msg[i], sizeof(struct msghdr));
        msgvec[i].msg_len = sizeof(struct msghdr);
    }

    /* recieve vector of messages. single recieve for efficiency */
    int num_recv = recvmmsg(fd, msgvec, vec_len, flags, timeout);

    printf("num_recv: %d\n", num_recv);

    if (num_recv < num_socks) {
        printf("not all fds recieved\n");
        if (num_recv < 0) {
            perror("recvmmsg failed");
        }
        else {
            printf("recvmmsg didn't receive all of them, missed %d msgs\n", (int)num_socks - num_recv);
        }
        /* Error getting all Rx, Tx file descriptors */
        return ERR_CONN;
    }

    for (i = 0; i < num_recv; i++) {
        socks[i] = *(int *)CMSG_DATA((struct cmsghdr *)msgvec[i].msg_hdr.msg_control);
    }

    return ERR_OK;
}

/* Get the hugepage memory for this descsock lib */
struct tmm_memory *
sys_get_tmm_mem(void) {

    return NULL;
}

/*
 * Read into vector from file.
 */
SIZE
descsock_readv_file(file_t fd, struct iovec *iov, int iovcnt)
{
    int n = readv(fd, iov, iovcnt);
    if(n == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        else {
            perror("descsock_readv_file: ");
            return -1;
        }
    }

    return n;
}

/*
 * Write vector to the file.
 */
SIZE
descsock_writev_file(file_t fd, const struct iovec *iov, int iovcnt)
{
    int n = writev(fd, iov, iovcnt);
    if(n == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        else {
            /* error occoured */
            perror("error descsock_writev_file: ");
            return -1;
        }
    }
    return n;
}

char *
descsock_getenv(char *env_var)
{
    return getenv(env_var);
}

char *
descsock_get_basename(char *path)
{
    return basename(path);
}

int main(int argc, char *arvc[])
{


    return EXIT_SUCCESS;
}