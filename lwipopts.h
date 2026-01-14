#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// =====================================================
// LWIP OPTIONS FOR FREERTOS + PICO W
// =====================================================

// OS Configuration
#define NO_SYS 0 // 0 = Use RTOS (Required for FreeRTOS)
#define LWIP_SOCKET 1
#define LWIP_NETCONN 1
#define LWIP_COMPAT_SOCKETS 1
#define LWIP_TIMEVAL_PRIVATE 0

// Threading & Protection
#define SYS_LIGHTWEIGHT_PROT 1
#define LWIP_TCPIP_CORE_LOCKING 1
#define LWIP_TCPIP_CORE_LOCKING_INPUT 1

// Memory configuration
#define MEM_LIBC_MALLOC 0
#define MEM_ALIGNMENT 4
#define MEM_SIZE 16000

// Buffer pools
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_ARP_QUEUE 10
#define MEMP_NUM_NETCONN 8
#define MEMP_NUM_NETBUF 8
#define MEMP_NUM_PBUF 16
#define PBUF_POOL_SIZE 24

// TCP settings
#define TCP_WND (8 * TCP_MSS)
#define TCP_MSS 1460
#define TCP_SND_BUF (8 * TCP_MSS)
#define TCP_SND_QUEUELEN ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

// Protocols
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_DNS 1
#define LWIP_DHCP 1
#define LWIP_IPV4 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_NETIF_HOSTNAME 1
#define LWIP_NETIF_STATUS_CALLBACK 1

// Thread-safe mailbox
#define TCPIP_MBOX_SIZE 8
#define TCPIP_THREAD_STACKSIZE 2048
#define TCPIP_THREAD_PRIO 3

#define DEFAULT_THREAD_STACKSIZE 1024
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define DEFAULT_UDP_RECVMBOX_SIZE 8
#define DEFAULT_TCP_RECVMBOX_SIZE 8
#define DEFAULT_ACCEPTMBOX_SIZE 8

// Random number generator
#define LWIP_RAND() ((u32_t)rand())

// Checksum
#define LWIP_CHKSUM_ALGORITHM 3

#endif
