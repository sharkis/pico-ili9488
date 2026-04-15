#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// --- Infrastructure ---
#define NO_SYS                      1   // 1 for Bare-metal/Poll/Background, 0 for FreeRTOS
#define MEM_ALIGNMENT               4
#define SYS_LIGHTWEIGHT_PROT        1

// --- Memory Optimization ---
// Pico 2 has 520KB RAM, so we can use 16KB+ for the heap
#define MEM_SIZE                    (16 * 1024) 
#define MEMP_NUM_TCP_SEG            32
#define PBUF_POOL_SIZE              24          // Increase if you see packet loss
#define PBUF_POOL_BUFSIZE           1536        // Fits a standard Ethernet frame

// --- Protocols ---
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_NETIF_HOSTNAME         1

// --- TCP Performance ---
#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

// --- Pico SDK Specifics ---
#define LWIP_CHKSUM_ALGORITHM       3           // Optimized for ARM
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1

#define LWIP_TIMEVAL_PRIVATE 0
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0

#endif
