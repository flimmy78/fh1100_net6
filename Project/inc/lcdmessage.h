#ifndef __LCDMESSAGE_H_
#define __LCDMESSAGE_H_

#include "main.h"
#include "share.h"






//使用串口发送
//typedef struct ARMtoFPGA{   //godin 串口发送信息结构
//  unsigned char synA;				 //同步符  90
//  unsigned char synB;				 //同步符  eb
////  unsigned char command;				 //命令码
////  unsigned char addencode;			 //板卡地址
//  unsigned char messagelen;			 //报文长度	0x1a
//  unsigned short year;		 
//  unsigned char	 month;
//  unsigned char	 day;
//  unsigned char  hour;
//  unsigned char  minute;
//  unsigned char  seconds;
//  char leap;				 		 //闰秒标记	 0正常，1正闰秒，2负闰秒，3未知
//  char sync_state;			 //同步状态	 A有效41H  V无效56H
//  char sync_mode; 			 //同步模式	
//  char transport;				 //传输模式
//  char Running_state;		 //ptp运行状态
//  char bestmac_addr[6];		
//  //char mac_addr[6];		
//  int  path_delay;			
//  int  correctionField;		
//  char chksum;				 //校验和	
//}ARMtoFPGA_PTP;


void ARM_to_FPGA(void);
//void ReadBoardADDR(void);
void CanRxHandle(void);

#endif
