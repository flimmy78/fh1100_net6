#ifndef __NTP_H__
#define __NTP_H__

#include "stm32f4x7_eth.h"
#include "ptpd.h"
#include "sys.h"
#include "lwip/memp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include <stdio.h>

typedef struct 
{
	unsigned int seconds;
	unsigned int fraction;
}ntp_time;

#define NTP_PORT    		123 //NTP端口规定使用123 
#define NTP_PCK_LEN		  48
#define MAX_GOOD_DATA	  1

#define LI_NOWARNING		(0 << 6)	
#define LI_PLUSSEC			(1 << 6)	
#define LI_MINUSSEC			(2 << 6)
#define LI_ALARM				(3 << 6)

#define NTP_MAXSTRATUM	 15
#define MODEMASK				(7 << 0)
#define VERSIONMASK			(7 << 3)
#define LIMASK					(3 << 6)

#define JAN_1970 		0x83AA7E80 
#define JAN_2030	 	0x70DBD880 + JAN_1970

#define LOC_STRATUM			1
#define LOC_PRECISION 	0xe3//0xe9 //内蒙的取得是0xe9
#define LOC_RTDELAY			0
#define LOC_DISPER			0
#define LOC_FERID 			0x00535047 //0x53504700   //0x4d4f5441//gps的ascii码//内蒙的取得是0x4d4f5441

#define DEFAULT_STATUS	0xdb		//客户端未同步
#define NOMAL_STATUS		0x1b		
#define DEFAULT_STRATUM	0
#define DEFAULT_POLL		6
#define DEFAULT_PREC		0xf0
#define DEFAULT_RTDELAY	0
#define DEFAULT_DISPER	0
#define DEFAULT_FERID		0x53504700
#define Test_LOC_FERID  0x4d535354

enum
{	ACT_MOD=1,    //1
	PAS_MOD,   		//2
	CLIENT_MOD,		//3
	SERVER_MOD,		//4
	BDC_MOD				//5
};

//typedef struct 
//{
//	unsigned int seconds;
//	unsigned int fraction;
//}ntp_time;

//#if NTP_Server || NTP_Client
typedef struct cfgDataHead
{
	unsigned char  frame_head;  //*
	unsigned char	 cmd_type;
	unsigned char  temp0;// Out delay（ns）signed      bit	
	unsigned char	 temp1;// 			// 客服端运行标志 	   				1
	unsigned int	 Local_ip_32;		// Local ip 						4
	unsigned int	 Server_ip_32;		// Server ip 	   					4
	unsigned int	 Gate_ip_32;			// Gate ip 4
	unsigned int   Mask_ip_32;
	unsigned int	 testMark;// 		// Out delay（ns） 			4
	unsigned int	 temp3;// 			// 客户端延时补偿（us） 	   		4
	unsigned int	 temp4;// 			// 服务端延时补偿（us）				4
	unsigned char  temp5;//			 //时区设置
	unsigned char  temp6;// 
	unsigned char	 temp7;// 			// CHKSUM 代码和                                                      4
	unsigned char  frame_end;        //  #
}ConfigHead;
//#endif 
/*
*@ntp 数据结构体
*/
typedef struct 
{
	unsigned char 	status;		//bit7-6闰秒提示,bit5-3NTP版本,bit2-0工作模式
	unsigned char  	stratum;	//时钟等级
	unsigned char  	poll;			//两个连续NTP的轮询间隔
	signed char  	precision;	//系统时钟精度
	unsigned int 	rootdelay;	//本地到主参考时钟源的往返时间
	unsigned int 	dispersion;	//系统时钟相对于主参考时钟的最大误差
	unsigned int 	refid;			//参考时钟源的标识
	ntp_time 		reftime;			//系统时钟最后一次被设定或更新的时间
	ntp_time 		orgtime;			//NTP请求报文离开发送端时发送端的本地时间
	ntp_time 		rxtime;				//NTP请求报文到达接收端时接收端的本地时间
	ntp_time 		txtime;				//应答报文离开应答者时应答者的本地时间
}ntp_msg;


typedef struct timeData
{
	unsigned char    frame_head; //帧头0xeb
	unsigned char    frame_head1; //帧头0x90
	unsigned char    frame_head2; //帧头0xeb
	unsigned char    frame_head3; //帧头0x90
	unsigned int     T1secondsFrom1900; //1900年1月1日0时0分以来的秒数
	unsigned int     T1fraction;        //微秒的4294.967296(=2^32/10^6)倍
	unsigned int     T2secondsFrom1900; //1900年1月1日0时0分以来的秒数
	unsigned int     T2fraction;        //微秒的4294.967296(=2^32/10^6)倍
	unsigned int     T3secondsFrom1900;  //1900年1月1日0时0分以来的秒数
	unsigned int     T3fraction;        //微秒的4294.967296(=2^32/10^6)倍
	unsigned int     T4secondsFrom1900;   //1900年1月1日0时0分以来的秒数
	unsigned int     T4fraction;          //微秒的4294.967296(=2^32/10^6)倍

	unsigned char    data_type; //对应的命令格式，见DATA_TYPE枚举
	unsigned char    updateNum; //数据循环标记--用于检查数据是否更新
	unsigned char    chkSum_1; //时区设置
	unsigned char    CFL_leapflag;//temp8;//用于闰秒的标志位传送
	unsigned int     fjm;
	/* 
	unsigned int  temp2; // 客户端延时补偿（us）      4
	unsigned int  temp3; // 客户端延时补偿（us）      4
	unsigned int  temp4; // 客户端延时补偿（us）      4
	unsigned int  temp5; // 客户端延时补偿（us）      4
	unsigned int  temp6; // 客户端延时补偿（us）      4
	*/
	unsigned char    frame_end0; //帧尾=0xEF 
	unsigned char    frame_end1; //帧尾=0x9F
	unsigned char    frame_end2; //帧尾=0xEF 
	unsigned char    frame_end3; //帧尾=0x9F
}TimeData;
/*
NTP数据帧头格式
LI闰秒标记:00(无预告),01(闰秒61),10(闰秒59),11(警告)
VN版本号
MODE:协议模式(0-7)保留0,对称1,被动对称2,客户端3,服务器端4,广播5,NTP控制系统6,7自保留
Stratum:时钟的层次水平,0未指定,1主参考,2-255第二参考
Poll:轮询间隔signed integer,2的次幂
Precision:精度
Root Delay:根延迟,到主参考源的整个往返延迟
Root Disersion根离差:相对于主参考源的最大误差
Reference Identifier参考时钟标记:用32为标记特定的参考
			 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |LI | VN  |Mode |    Stratum    |     Poll      |   Precision   	|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                       Root Delay(32)                      			|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                       Root Disersion(32)                 		  	|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                       Reference Identifier(32)            			|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                       Reference Timestamp(64)             			|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          参考时间戳(64) T1                    	|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          原始时间戳(64) T2                    	|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          接收时间戳(64) T3                    	|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          传送时间戳(64) T4                    	|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			|                          保存认证者的信息(96)                 	|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

void Ntp_ClientInit(unsigned int ip);
//static void client_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
//               struct ip_addr *addr, u16_t port);
//void Ntp_server_init(void);
//long prmtread_decimal(unsigned char *p,unsigned char n);
//void ulocaltime(unsigned long ulTime, tTime *psTime);
//void ntpClientLoop(unsigned int ip);
#endif
