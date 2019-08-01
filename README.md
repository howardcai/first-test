**Folder structure**
*  `tester2.c`  
    tester for seding and receving packets though the library
*  `descsock.c`  
*  `descsock.h`    
    Main core for library
*  `src/kern/sys.c`  
*  `src/kern/sys.h`  
    File I/O, mmap memory, epoll events, cli args parser  
*   `src/kern/err.h`  
    Errno file grabbed from TMM  
*  `src/sys/fixed_queue.h`  
    Fixed queue data structure grabbed from TMM  
*   `src/sys/types.h`  
    TMM source types  
*  `src/net/packet.c`  
*  `src/net/packet.h`  
    Packet object stack and memory allocator  
*   `src/net/xfrag_mem.c`  
*   `src/net/xfrag_mem.h`  
    xfrag objects used Tx, RX DMA, queues and memory allocator
