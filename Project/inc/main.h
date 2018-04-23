#ifndef __MAIN_H
#define __MAIN_H
#ifdef __cplusplus
 extern "C" {
#endif
#include "stm32f4xx.h"
#include "stm32f4x7_eth_bsp.h"
#include "serial_hand.h"
//#define Sync_Device 			//时间同步设备
//#define Device_Slave 			//从设备
extern	 uint8_t PTP_MS_MODE ;
//功能开关
#define NTP_Server
#define PTP_Device	 

//#define Gx_PTP_Device						//ptp测试仪模块
//godin 功能模块开关定义	 
//#define NTP_Philips	 
//#define NTP_Client	
//#define FreqMassage

	 
#define mLED1 GPIO_Pin_11
#define mLED2 GPIO_Pin_12
#define mLED3 GPIO_Pin_13
#define mLED4 GPIO_Pin_14

#define Default_System_Time    1388592000
#define myprintf  printf

typedef struct FigStruct//CAN配置信息结构
{             
	uint32_t IPaddr;  		//本机IP
	uint32_t NETmark;			//本机子网掩码

	uint32_t GWaddr;			//本机网关
	uint32_t DstIpaddr;		//对端IP

	uint16_t LocalPort;		//本地端口
	uint16_t DstPort;			//目的端口
	uint8_t  WorkMode;		//ptp工作模式
	uint8_t  clockClass;
	uint8_t	 UTCoffset;		
  uint8_t  ip_mode;			//NTP网络模式(单播组播广播)	


	uint8_t  MACaddr[6];	//本机MAC地址
	uint8_t  MSorStep;		//一步法两步法	
	uint8_t  ClockDomain;	//时钟域
	

	char  AnnounceInterval;	
	uint8_t AnnounceOutTime;	 
	char  SyncInterval;			
	uint8_t PDelayInterval;		
	uint8_t DelayInterval;
	uint8_t priority1;
	uint8_t priority2;
	uint8_t	tmp2;
}FigStructData;


void Time_Update(void);
void Delay(uint32_t nCount);
void STM_LEDon(uint16_t LEDx);
void STM_LEDoff(uint16_t LEDx);


#ifdef __cplusplus
}
#endif

#endif

