[![pipeline status](https://gitlab.f5net.com/datapath/generic-descsock-library/badges/master/pipeline.svg)](https://gitlab.f5net.com/datapath/generic-descsock-library/commits/master)
<br />
### GENERAL USAGE
*See `test_client.c` for an example*
```
make rpm
sudo rpm -i rpmbuild/RPMS/x86_64/libdescsock-client-THEVERSION-dev.x86_64.rpm 
make example
./test_client
```
***
<br />

1)  Call `descsock_open()` with an `descsock_client_spec_t` defining
   client requirements, as well as any flags. The client library will
   create a thread in the caller's process which manages the DMA Agent
   SEP opened based on the client spec, and buffers frames received
   from the DMA Agent over the SEP and sends frames from the client
   calls to `descksock_send()`.

2)  Use `descsock_poll()` or `descsock_send()` or `descsock_recv()`, depending on
   the blocking / nonblocking requirement of your program's IO
   model, to check for, send, and receive Ethernet frames,
   respectively.

3)  Call `descsock_close()` to terminate the client connection with the DMA Agent
***
<br />

**Structure containing hugepages path, master socket path and svc id**
```
typedef struct {
    /*
     * This specifies the path to the shared memory file to be used to buffer
     * DMA operations - the descsock client library will create this file and
     * manage its geometry. If the file exists, it will be zeroed and
     * truncated or extended to meet the requirements of the library.
     * Preferably, this path resides under a 2MB-pagesize hugetlbfs mount, but
     * must have at least (XXX - TBD) MB of free space.
     */
    char    dma_shmem_path[DESCSOCK_PATHLEN];
    /*
     * This is the path to the DMA Agent or CPProxy master domain socket.
     */
    char    master_socket_path[DESCSOCK_PATHLEN];
    /*
     * This specifies the Service ID the library will attempt to register
     * this client with.
     */
    int     svc_id;
} descsock_client_spec_t;


/*
 * Poll event bits requested in @event_mask of calls to `descsock_poll()`, and
 * returned by descsock_poll() if requested condition(s) are met.
 */
#define DESCSOCK_POLLIN     (1 <<  0)   // There is data to receive.
#define DESCSOCK_POLLOUT    (1 <<  1)   // Sending now would not block.
#define DESCSOCK_POLLERR    (1 << 31)   // Error condition (return only).
```
***
<br />

**At present, this library does not support tenants in VMs using VirtIO serial sockets.**
***
<br />
<br />

Opens a new tenant datapath connection into the DMA agent, as specified by _@spec_ (see definition of **descock_client_spec_t** above),
and starts the client library worker thread.

The _@flags_ argument can be used to set default flags applied to all subsequent _descsock_send()_  and _descosck_recv()_ calls.
Only a **single SEP** is opened, and only Rx/Tx QoS 0 on that SEP will be operated by the client library.
As a result of calling this function, the client library creates a new thread in the caller's  context which manages the client's SEP,
providing empty buffer descriptors to the DMA Agent and buffering returned laden descriptors until the user calls _descsock_recv()_,
as well as sending buffers supplied in _descsock_send()_.
Returns 0 on success, -1 on failure and errno will be set appropriately.  
`int descsock_open(descksock_client_spec_t * const spec, const int flags);`
***
<br />
<br />

Takes an _@event_mask_ of _DESCSOCK_POLL_ events and checks if the current descsock state meets the condition of the requested events.
Returns a mask of event conditions which are satisfied and in the _@event_mask_, or _DESCSOCK_POLLERR_ (-1) if an error is encountered.  
`int descsock_poll(int event_mask);`
***
<br />
<br />

Takes a _@buf_ and a corresponding _@len_ and queues it for sending by the library worker thread over the DMA Agent SEP associated with this process.
_@flags_ specifies any flags which override the defaults specified in _descsock_open()_.
Note that @buf must point to a complete Ethernet frame, including 4 trailing empty bytes for the CRC. This is required by hardware.
Returns number of bytes sent, or -1 on failure and errno will be set appropriately.
If errno is EWOULDBLOCK, and the DESCSOCK_NONBLOCK flag is set, then there is
back-pressure on the transmit descriptor socket (no room to send).  
`ssize_t descsock_send(const void * const buf, const size_t len, const int flags);`
***
<br />
<br />

Takes a _@buf_ and a corresponding _@len_ and copies into it the next ready frame buffered by the library worker thread from the DMA Agent SEP associated
with this process. @flags specifies any flags which override the defaults specified in _descsock_open()_.
Note that if data is received, _@buf_ will point to a complete Ethernet frame, including 4 trailing empty bytes for the CRC. This is required by
hardware.
Returns number of bytes recieved into _@buf_, or -1 on failure and errno will be set appropriately.

If errno is EWOULDBLOCK, and the DESCSOCK_NONBLOCK flag is set, then no complete packets have been buffered internally by the client library
worke thread (no data to recv).  
`ssize_t descsock_recv(void * const buf, const size_t len, const int flags);`
***
<br />
<br />

This is to provide a conceptual analogue to fnctl on a socket. Currently no
commands _(@cmd)_ are implemented, but this is defined for future uses like setting advanced options, etc, as a general way to achieve mostly
backwards-compatible / non-breaking extensions to the API. Returns 0 on success, -1 on failure and errno will be set appropriately.  
`int descsock_cntl(const int cmd, ...);`
***
<br />
<br />

This cleanly closes the connection to the DMA Agent associated with this process / library worker thread, unmaps any shared memory, and frees any
internal resources. Returns 0 on success, -1 on failure and errno will be set appropriately.  
`int descsock_close(void);`
<br />
<br />

