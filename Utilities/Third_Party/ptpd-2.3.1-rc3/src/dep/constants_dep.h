
/* constants_dep.h */

#ifndef CONSTANTS_DEP_H
#define CONSTANTS_DEP_H

/**
*\file
* \brief Plateform-dependent constants definition
*
* This header defines all includes and constants which are plateform-dependent
*
* ptpdv2 is only implemented for linux, NetBSD and FreeBSD
 */

/* platform dependent */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stm32f4xx.h>
#include <limits.h>
#include "lwip/mem.h"
#include "lwip/udp.h"
#include "lwip/igmp.h"
#include "lwip/arch.h"
#include "stm32f4x7_eth.h"
#include "ethernetif.h"
//#include "stm32f4x7_eval.h"

#ifndef stdout
#define stdout 1
#endif

/* pow2ms(a) = round(pow(2,a)*1000) */
#define pow2ms(a) (((a)>0) ? (1000 << (a)) : (1000 >>(-(a))))

#define CLOCK_IDENTITY_LENGTH 8

#define ADJ_FREQ_MAX 512000

#define IF_NAMESIZE             2
#define INET_ADDRSTRLEN         16

#define IFACE_NAME_LENGTH         IF_NAMESIZE
#define NET_ADDRESS_LENGTH        INET_ADDRSTRLEN

#define IFCONF_LENGTH 10

#if BYTE_ORDER == LITTLE_ENDIAN
#define PTPD_LSBF
#elif BYTE_ORDER == BIG_ENDIAN
#define PTPD_MSBF
#endif

/* UDP/IPv4 dependent */
//#ifndef INADDR_LOOPBACK  //
//#define INADDR_LOOPBACK 0x7f000001UL
//#endif

#define SUBDOMAIN_ADDRESS_LENGTH  4
#define PORT_ADDRESS_LENGTH       2
#define PTP_UUID_LENGTH           6
#define CLOCK_IDENTITY_LENGTH	 		8
#define FLAG_FIELD_LENGTH         2

#define PACKET_SIZE  300 //ptpdv1 value kept because of use of TLV...
#define PACKET_BEGIN_UDP (ETHER_HDR_LEN + sizeof(struct ip) + \
	    sizeof(struct udphdr))
#define PACKET_BEGIN_ETHER (ETHER_HDR_LEN)

#define PTP_EVENT_PORT    319
#define PTP_GENERAL_PORT  320

#define DEFAULT_PTP_DOMAIN_ADDRESS     "224.0.1.129"
#define PEER_PTP_DOMAIN_ADDRESS        "224.0.0.107"

/* 802.3 Support */

#define PTP_ETHER_DST "01:1b:19:00:00:00"
#define PTP_ETHER_TYPE 0x88f7
#define PTP_ETHER_PEER "01:80:c2:00:00:0E"


/* Highest log level (default) catches all */
#define LOG_ALL LOG_DEBUGV

/* drift recovery metod for use with -F */
enum {
	DRIFT_RESET = 0,
	DRIFT_KERNEL,
	DRIFT_FILE
};
/* IP transmission mode */
enum {
	IPMODE_MULTICAST = 0,	//×é²¥-0
	IPMODE_UNICAST,				//µ¥²¥-1
	IPMODE_HYBRID,				//¹ã²¥-2	
#if 0
	IPMODE_UNICAST_SIGNALING
#endif
};

/* log timestamp mode */
enum {
	TIMESTAMP_DATETIME,
	TIMESTAMP_UNIX,
	TIMESTAMP_BOTH
};

/* servo dT calculation mode */
enum {
	DT_NONE,
	DT_CONSTANT,
	DT_MEASURED
};

/* Preset definitions */
enum {
    PTP_PRESET_NONE,
    PTP_PRESET_SLAVEONLY,
    PTP_PRESET_MASTERSLAVE,
    PTP_PRESET_MASTERONLY,
    PTP_PRESET_MAX
};

#define MM_STARTING_BOUNDARY_HOPS  0x7fff

/* others */

/* bigger screen size constants */
#define SCREEN_BUFSZ  228
#define SCREEN_MAXSZ  180

/* default size for string buffers */
#define BUF_SIZE  1000

#define NANOSECONDS_MAX 999999999

// limit operator messages to once every X seconds
#define OPERATOR_MESSAGES_INTERVAL 300.0

#define MAXTIMESTR 32

#endif /*CONSTANTS_DEP_H_*/
