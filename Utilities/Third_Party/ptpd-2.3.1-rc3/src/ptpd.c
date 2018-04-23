/*-
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file   ptpd.c
 * @date   Wed Jun 23 10:13:38 2010
 *
 * @brief  The main() function for the PTP daemon
 *
 * This file contains very little code, as should be obvious,
 * and only serves to tie together the rest of the daemon.
 * All of the default options are set here, but command line
 * arguments and configuration file is processed in the 
 * ptpdStartup() routine called
 * below.
 */

#include "ptpd.h"
#include "share.h"
extern int 	 oldseconds;
extern void doState(RunTimeOpts*,PtpClock*);
void ptpHandleap(RunTimeOpts *rtOpts, PtpClock *ptpClock);
RunTimeOpts rtOpts;			/* statically allocated run-time configuration data */
PtpClock G_ptpClock;

extern FigStructData GlobalConfig;
extern err_t eth_send_llc( u16_t length);


 
#ifdef   NTP_Philips
void LLC_send(UInteger8 llc_lenth)
{
  eth_send_llc(llc_lenth);
}
#endif


int PTPd_Init(void)
{
	Integer16 ret;
	memset(&rtOpts,0,sizeof(RunTimeOpts)); 		//结构体数据清0
	memset(&G_ptpClock, 0, sizeof(PtpClock));	//结构体数据清0
	
	rtOpts.clockQuality.clockAccuracy = DEFAULT_CLOCK_ACCURACY;					  //时钟精度0xFE
	rtOpts.clockQuality.clockClass 		= GlobalConfig.clockClass;					//时钟等级248			只做从：255，主：0-127，可主可从：128-254
	rtOpts.clockQuality.offsetScaledLogVariance = DEFAULT_CLOCK_VARIANCE;	//0xFFFF
	rtOpts.priority1    						= GlobalConfig.priority1;							// 优先级1					= 128
	rtOpts.priority2   						  = GlobalConfig.priority2;							// 优先级2					= 优先级1
	
	rtOpts.announceInterval 				= GlobalConfig.AnnounceInterval;	 		// announce 发布	间隔		= 0^2（s）
	rtOpts.syncInterval 						= GlobalConfig.SyncInterval; 					// sync			发布	间隔		= 0^2（s）
	rtOpts.initial_delayreq 				= GlobalConfig.DelayInterval; 				// delayreq	初始化	间隔		=	0^2（s）（可以协商和修改）
	rtOpts.logMinPdelayReqInterval 	= GlobalConfig.PDelayInterval; 				// pdelayreq当前	间隔		=	0^2（s）
	rtOpts.announceReceiptTimeout   		= GlobalConfig.AnnounceOutTime;		// announce	接收	间隔		= 2^6（s）
	rtOpts.announceTimeoutGracePeriod  	= 0;															// announce接收超时宽限次数 =	0次
	
	rtOpts.ip_mode 	 = IPMODE_MULTICAST;																	//组播模式	,协议是基于组播(多播)设计，但是也可以支持单播
	rtOpts.slaveOnly = FALSE;																		  				//可做主时钟
	
	switch(GlobalConfig.WorkMode)
		{
			case 0: 
				  rtOpts.transport 			= IEEE_802_3;	//0:主E2E-MAC
					rtOpts.delayMechanism = E2E; 				
					break;
			case 1:
					rtOpts.transport 			= IEEE_802_3;	//1:主P2P-MAC
					rtOpts.delayMechanism = P2P; 
					break;
			case 2: 
					rtOpts.transport 			= UDP_IPV4;		//2:主E2E-UDP
					rtOpts.delayMechanism = E2E; 
					break;
			case 3: 
					rtOpts.transport 			= UDP_IPV4;		//3:主P2P-UDP
					rtOpts.delayMechanism = P2P; 
					break;
			case 4: 
				  rtOpts.transport 			= IEEE_802_3;	//4:从E2E-MAC
					rtOpts.delayMechanism = E2E;
					rtOpts.slaveOnly      = TRUE;
					break;
			case 5:
					rtOpts.transport 			= IEEE_802_3;	//5:从P2P-MAC
					rtOpts.delayMechanism = P2P; 
					rtOpts.slaveOnly      = TRUE;
					break;
			case 6: 
					rtOpts.transport 			= UDP_IPV4;		
					rtOpts.delayMechanism = E2E; 
					rtOpts.slaveOnly      = TRUE;
					break;
			case 7: 
					rtOpts.transport 			= UDP_IPV4;		
					rtOpts.delayMechanism = P2P; 
					rtOpts.slaveOnly      = TRUE;
					break;
			default: 					
					rtOpts.transport			= IEEE_802_3;	//从E2E-MAC
					rtOpts.delayMechanism = E2E;	
					rtOpts.slaveOnly      = TRUE;
					break;
			
		}
	rtOpts.domainNumber												  = GlobalConfig.ClockDomain; //时钟域				=	0
	rtOpts.timeProperties.currentUtcOffsetValid = TRUE;									//UtcOffset		=	无效
	if(GlobalConfig.UTCoffset==0)
		rtOpts.timeProperties.currentUtcOffsetValid = FALSE;									//UtcOffset		=	无效
	rtOpts.timeProperties.currentUtcOffset 			= GlobalConfig.UTCoffset;		//UtcOffset值	=	UTCoffset
	rtOpts.timeProperties.timeSource 						= INTERNAL_OSCILLATOR;	//时间源				= 晶振
	rtOpts.timeProperties.timeTraceable 				= FALSE;								//时间同步			= FALSE
	rtOpts.timeProperties.frequencyTraceable 		= FALSE;								//频率锁定			= FALSE
	rtOpts.timeProperties.ptpTimescale 					= FALSE;								//PTP状态			= FALSE
	rtOpts.unicastAddr												  = 0;										//单播地址			= 0
	
	rtOpts.servo.sDelay	  = 6;																	//sDelay								=	6
	rtOpts.servo.sOffset  = 1;																	//sOffset								=	1
	rtOpts.servo.ap 			= 3;																	//PI控制算法-比例系数	P	=	2					
	rtOpts.servo.ai 			= 32;																	//PI控制算法积分时间常数I	=	16	
	
//	rtOpts.servo.sDelay	  = 3;																					//sDelay								=	6
//	rtOpts.servo.sOffset  = DEFAULT_OFFSET_S;														//sOffset								=	1
//	rtOpts.servo.ap 			= DEFAULT_AP;																	//PI控制算法-比例系数	P	=	2					
//	rtOpts.servo.ai 			= DEFAULT_AI;																	//PI控制算法积分时间常数I	=	16	
	
	rtOpts.inboundLatency.nanoseconds  = DEFAULT_INBOUND_LATENCY;				//进入系统的边界延时 0
	rtOpts.outboundLatency.nanoseconds = DEFAULT_OUTBOUND_LATENCY;			//离开系统时边界延时 0
	rtOpts.max_foreign_records 				 = DEFAULT_MAX_FOREIGN_RECORDS;		//最大外来主时钟		 5
	
	 
	rtOpts.alwaysRespectUtcOffset			 =	FALSE;		// 默认关闭（按协议规定）
	rtOpts.preferUtcValid							 =	FALSE;		// 默认关闭（按协议规定）
	rtOpts.requireUtcValid						 =	FALSE;		// 默认关闭（按协议规定）

	rtOpts.noAdjust 						= NO_ADJUST;  						//是否通过PTP同步自己的时间
	rtOpts.noResetClock  			  = DEFAULT_NO_RESET_CLOCK; //PTP同步自己的时间时不重启时钟
	rtOpts.maxListen 						= 5;											//5个监听状态周期后，重启协议栈
	rtOpts.syncSequenceChecking = TRUE;										//sync序列号校验

  
	if(rtOpts.slaveOnly)
		printf("PTP模式 :从");
	else
		printf("PTP模式 :主");
	if(rtOpts.transport==UDP_IPV4)
		printf(" UPD");
	else if(rtOpts.transport==IEEE_802_3)
		printf(" ETH");
	if(rtOpts.delayMechanism==E2E)
		printf("-E2E\n");
	else if(rtOpts.delayMechanism==P2P)
		printf("-P2P\n");
	if(rtOpts.timeProperties.currentUtcOffsetValid)
		{
			printf("UtcOffset有效)\n");
		}
	printf("UtcOffs :%d\n",rtOpts.timeProperties.currentUtcOffset);	
	printf("时钟域  :%d\n",rtOpts.domainNumber);			
	printf("时钟级别:%d\n",rtOpts.clockQuality.clockClass);			
  printf("优先级1 :%d\n",rtOpts.priority1);	
  printf("优先级2 :%d\n",rtOpts.priority2);	
	printf("announce:%d\n",rtOpts.announceInterval);
	printf("sync间隔:%d\n",rtOpts.syncInterval);
	printf("aTimeout:%d\n",rtOpts.announceReceiptTimeout);
	printf("delayreq:%d\n",rtOpts.initial_delayreq);
	printf("PdelayRe:%d\n",rtOpts.logMinPdelayReqInterval);
	
	if ((ptpdStartup(&ret,&G_ptpClock ,&rtOpts)) != 1)  //开启协议栈
	{
		if (ret != 0)
			ERROR(USER_DESCRIPTION" startup failed\n");
			return ret;
	}
	toState(PTP_INITIALIZING, &rtOpts, &G_ptpClock);		//协议状态转换->初始化状态
	adjFreq(0);		//初始化频率值，加快收敛速度，2017.06.05
	return 1;
}


/* loop forever. doState() has a switch for the actions and events to be
   checked for 'port_state'. the actions and events may or may not change
   'port_state' by calling toState(), but once they are done we loop around
   again and perform the actions required for the new 'port_state'. */
void ptpd_Periodic_Handle(__IO UInteger32 localtime)
{
		/* do the protocol engine */
    static UInteger32 old_localtime = 0;		//static定义的全局变量

    catch_alarm(localtime - old_localtime);	//取一次时间间隔
    old_localtime = localtime;

    /* at least once run stack */
		/* forever loop.. */
    do
    {
			 if (G_ptpClock.portState == PTP_INITIALIZING)
			  {
					WARNING("Debug Initializing...\n");
					if (!doInit(&rtOpts,&G_ptpClock)) 
					{//初始化
						ERROR("Initializing was FAILED\n");
						return;
					}
				}
				else 
				{
					doState(&rtOpts,&G_ptpClock);//运行协议
				}
        /* if there are still some packets - run stack again */
    }
    while (netSelect(NULL,&(G_ptpClock.netPath)) > 0);//如果有数据包（描述符地址>0），则一直运行协议栈来处理
}



void ptpHandleap(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
	struct ptptime_t timeupdata;
	TimeInternal lastseconds;
	
	if(ptpClock->timePropertiesDS.leap61 || ptpClock->timePropertiesDS.leap59)
	{
		ptpClock->leapSecondPending = TRUE;
		getTime(&lastseconds);
	}
	
	if(ptpClock->timePropertiesDS.leap61)//正闰秒
	{
		if(lastseconds.seconds%60 == 1)//
		{
			timeupdata.tv_sec = -1;
			timeupdata.tv_nsec = 0;
			//ptpClock->leapSecondInProgress = TRUE;
			ETH_PTPTime_UpdateOffset(&timeupdata);
			ptpClock->timePropertiesDS.leap61=FALSE;
			ptpClock->timePropertiesDS.leap59=FALSE;
			oldseconds=oldseconds-1;
			ptpClock->leapSecondPending = FALSE;
			printf("ptpleap61\n");
		}
	}
	else if(ptpClock->timePropertiesDS.leap59)//负闰秒
	{
		if(lastseconds.seconds%60 == 59)
		{
			timeupdata.tv_sec = 1;//
			timeupdata.tv_nsec = 0;
			//ptpClock->leapSecondInProgress = TRUE;
			ETH_PTPTime_UpdateOffset(&timeupdata);
			ptpClock->timePropertiesDS.leap61=FALSE;
			ptpClock->timePropertiesDS.leap59=FALSE;
			oldseconds=oldseconds+1;
			ptpClock->leapSecondPending = FALSE;
			printf("ptpleap59\n");
		}
	}
}

char *dump_TimeInternal2(const char *st1, const TimeInternal * p1, const char *st2, const TimeInternal * p2)
{
	static char buf[BUF_SIZE];

	return buf;
}



