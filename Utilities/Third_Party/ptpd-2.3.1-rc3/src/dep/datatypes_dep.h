#ifndef DATATYPES_DEP_H_
#define DATATYPES_DEP_H_

#include "stdint.h"
/**
*\file
* \brief Implementation specific datatype

 */
//typedef enum {FALSE=0, TRUE} Boolean;
typedef bool Boolean;
typedef char Octet;
typedef signed char  Integer8;
typedef signed short Integer16;
typedef signed int Integer32;
typedef unsigned char  UInteger8;
typedef unsigned short UInteger16;
typedef unsigned int UInteger32;
typedef unsigned short Enumeration16;
typedef unsigned char Enumeration8;
typedef unsigned char Enumeration4;
typedef unsigned char Enumeration4Upper;
typedef unsigned char Enumeration4Lower;
typedef unsigned char UInteger4;
typedef unsigned char UInteger4Upper;
typedef unsigned char UInteger4Lower;
typedef unsigned char Nibble;
typedef unsigned char NibbleUpper;
typedef unsigned char NibbleLower;


#define PBUF_QUEUE_SIZE 4

/**
* \brief Implementation specific of UInteger48 type
 */
typedef struct {
	unsigned int lsb;
	unsigned short msb;
} UInteger48;

/**
* \brief Implementation specific of Integer64 type
 */
/*
typedef struct {
	uint32_t lsb;
	int32_t msb;
} Integer64;
*/

typedef int64_t Integer64;

typedef struct
{
    Integer32  y_prev, y_sum;
    Integer16  s;
    Integer16  s_prev;
    Integer32  n;
} Filter;

/**
* \brief Struct used to average the offset from master
*
* The FIR filtering of the offset from master input is a simple, two-sample average
 */
typedef struct {
    Integer32  nsec_prev, y;
} offset_from_master_filter;

/**
* \brief Struct used to average the one way delay
*
* It is a variable cutoff/delay low-pass, infinite impulse response (IIR) filter.
*
*  The one-way delay filter has the difference equation: s*y[n] - (s-1)*y[n-1] = x[n]/2 + x[n-1]/2, where increasing the stiffness (s) lowers the cutoff and increases the delay.
 */
typedef struct {
    Integer32  nsec_prev, y;
    Integer32  s_exp;
} one_way_delay_filter;

/**
* \brief Struct containing interface information and capabilities
 */
typedef struct {
        unsigned int flags;
        int addressFamily;
        Boolean hasHwAddress;
        Boolean hasAfAddress;
        unsigned char hwAddress[14];
//        struct sockaddr afAddress;
} InterfaceInfo;

typedef struct
{
    void        *pbuf[PBUF_QUEUE_SIZE];	//PBUF_QUEUE_SIZE==4
    Integer32   get;										
    Integer32   put;
    Integer32   count;
} BufQueue;
/**
* \brief Struct describing network transport data
 */
typedef struct {
	Integer32     multicastAddr;
	Integer32     peerMulticastAddr;
	Integer32     unicastAddr;

	struct udp_pcb    *eventPcb;

	struct udp_pcb    *generalPcb;
	BufQueue      eventQ;
	BufQueue      generalQ;
} NetPath;


typedef struct{

    UInteger8 minValue;
    UInteger8 maxValue;
    UInteger8 defaultValue;

} UInteger8_option;

typedef struct{

    Integer32  minValue;
    Integer32  maxValue;
    Integer32  defaultValue;

} Integer32_option;

typedef struct{

    UInteger32  minValue;
    UInteger32  maxValue;
    UInteger32  defaultValue;

} UInteger32_option;

typedef struct{

    Integer16  minValue;
    Integer16  maxValue;
    Integer16  defaultValue;

} Integer16_option;


/* define compiler specific symbols */
#if defined   ( __CC_ARM   )
typedef long ssize_t;
#elif defined ( __ICCARM__ )
typedef long ssize_t;
#elif defined (  __GNUC__  )

#elif defined   (  __TASKING__  )
typedef long ssize_t;
#endif

#endif /*DATATYPES_DEP_H_*/
