#ifndef __HUD_SYS_H__
#define __HUD_SYS_H__

#include "types.h"


/* non-blocking file/socket/pipe/serial I/O setting */
err_t sys_set_non_blocking(int fd, int non_blocking);
BOOL sys_file_descriptor_exists(int fd);

/*
 * TMM DESCSOCK/VNIC Unix Domain Socket wrapper calls
 */
int descsock_get_unixsocket(char *path);

int descsock_poll_fd(int fd, int num_of_fds, int timeout);

int descsock_socket_write(int fd, void * buf, UINT32 len);

int descsock_socket_read(int fd, void * buf, UINT32 len);

void * descsock_map_dmaregion(char * path, UINT64 size);

int descsock_get_errno(void);

int descsock_getpid(void);

/* Epoll System call wrappers to be able to talk to the DMA Agent*/
struct epoll_event;
struct mmsghdr;
struct sockaddr_un;

int descsock_epoll_create(int size);

int descsock_epoll_create1(int flags);

int descsock_epoll_ctl(int epfd, int op, int fd, struct epoll_event *events);

int descsock_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

err_t descsock_recv_socket_conns(int fd, int * socks);

struct tmm_memory * sys_get_tmm_mem(void);

SIZE descsock_readv_file(file_t fd, struct iovec *iov, int iovcnt);

SIZE descsock_writev_file(file_t fd, const struct iovec *iov, int iovcnt);

char * descsock_getenv(char *env_var);

char * descsock_get_basename(char *path);



#endif /* __HUD_SYS_H__ */