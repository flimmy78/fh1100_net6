#ifndef __FH1000_H
#define __FH1000_H

#include "main.h"
#ifdef NTP_Server_HuNan

#define COMDEF_GETSETINFO      1
#define COMDEF_ACKGETSETINFO   2
#define COMDEF_SETINFO         3
#define COMDEF_ACKSETINFO      4 
#define COMDEF_GETDISPLAY      5 
#define  COMDEF_ACKDISPLAY     6
#define COMDEF_SENDIP					 7
#define COMDEF_IP							 8

typedef struct SendIP
{
	unsigned int  net_ip;
	unsigned int  net_gw;
	unsigned int  net_mark;
}__attribute__((packed)) Send_IP;


typedef struct SetIP
{
	unsigned int  ip;
	unsigned short port;
	unsigned char time;
}__attribute__((packed)) Set_IP;

typedef struct tagUdpcom
{
   unsigned char comtype;
   unsigned char data[50];

}__attribute__((packed)) UDP_COM;

/*网络结构体定义*/
typedef struct TAGPTPSETMSG
{
	unsigned int ip;	
	unsigned int  gw_ip;
	unsigned int  SubnetMask;
	unsigned char ETHERNET_MODE;
	unsigned char MULTICAST_MODE;
	unsigned char E2E_mode;
	unsigned char ANNOUNCE_INTERVAL;
	unsigned char ANNOUNCE_RECEIPT_TIMEOUT;
	unsigned char SYNC_INTERVAL;
	unsigned char PDELAYREQ_INTERVAL;
  unsigned char DELAYREQ_INTERVAL;
	unsigned char SLAVE_PRIORITY1 ;//优先级1
	 unsigned char SLAVE_PRIORITY2 ;//优先级2
	 unsigned char DOMAINNUMBER ;//时钟域
	 unsigned char TWOSTEPFLAG ;//两步法
	
}__attribute__((packed)) PTP_SETMSG;

typedef struct  TAGPTPDISPLAY
{
  unsigned char    clock_type;
	unsigned char    twostepflag;
	unsigned char    ethernet_mode;
	unsigned char    e2e_mode;
	unsigned char    portState;
	unsigned char    timeSource;
  unsigned char    bestmasterclock_mac[6];	
	char     				 local_mac[6];
	unsigned char    warning_signal;
	int                 offsetFromMaster_seconds;
	int                 offsetFromMaster_nanoseconds;
	int                meanPathDelay_seconds;
	int                meanPathDelay_nanoseconds;
}__attribute__((packed))  PTP_DISPLAY;//实际占用字节模式(取消自动对齐)


typedef struct cfg_DataHead
{
	unsigned char frame_head;
	unsigned char state_flag1;
	unsigned char state_flag2;
	unsigned char state_flag3;
	unsigned char state_flag4;
	unsigned char year_4;
	unsigned char year_3;
	unsigned char year_2;
	unsigned char year_1;
	unsigned char month_2;
	unsigned char month_1;
	unsigned char day_2;
	unsigned char day_1;
	unsigned char hour_2;
	unsigned char hour_1;
	unsigned char min_2;
	unsigned char min_1;
	unsigned char sec_2;
	unsigned char sec_1;
	unsigned char chksum_2;
	unsigned char chksum_1;
	unsigned char end_flag2;
	unsigned char end_flag1;
}CfgData;
#endif

#endif
