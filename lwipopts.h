#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Generally you would define your own explicit list of lwIP options
// (see https://www.nongnu.org/lwip/2_1_x/group__lwip__opts.html)
//
// This example uses a common include to avoid repetition
#include "lwipopts_examples_common.h"


#if !NO_SYS
#define TCPIP_THREAD_STACKSIZE 2048
#define DEFAULT_THREAD_STACKSIZE 1024
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define TCPIP_MBOX_SIZE 8

#define DEFAULT_UDP_RECVMBOX_SIZE TCPIP_MBOX_SIZE
#define DEFAULT_TCP_RECVMBOX_SIZE TCPIP_MBOX_SIZE
#define DEFAULT_ACCEPTMBOX_SIZE TCPIP_MBOX_SIZE
#define MEMP_NUM_NETCONN (TCPIP_MBOX_SIZE * 2)

#define LWIP_TIMEVAL_PRIVATE 0

// not necessary, can be done either way
#define LWIP_TCPIP_CORE_LOCKING_INPUT 1
#endif

//#define HTTPD_FSDATA_FILE "pico_fsdata.inc"

#endif