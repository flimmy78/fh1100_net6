#include "ptpd.h"
#include "ntp.h"
#include "main.h"
#include "netconf.h"
#include "share.h"

extern sysTime sTime;
extern RunTimeOpts rtOpts;
extern PtpClock  G_ptpClock;

//#if defined(Sync_Device) 
struct Sync_UartData sync_UartData;			//串口报文结构
unsigned char SyncIndex = 0;						//串口报文指针
unsigned char SyncUart_OK;//串口报文接受标识
unsigned char leap61;			//正闰秒
unsigned char leap59;			//负闰秒
unsigned char leapwring;	//闰秒预告
unsigned char leapNum;		//闰秒标志的计数
//unsigned char lastseconds;//用于维护大时间的上一秒值2017.06.05

void USART1_IRQHandler(void)//godin ptp接收串口时间信息
{	
	if(USART_GetITStatus(USART1,USART_IT_RXNE) != RESET)//检查是否是指定的中断发生
	{	//接收到数据产生中断 
		USART_ClearITPendingBit(USART1,USART_IT_RXNE);//清除接收中断标记	
		/*		
		rev = USART_ReceiveData(USART1);	
		if(rev == 0x0a)//收到第二个接收标记
		{
			if(sync_UartData.end_flag1==0x0d)//第一个标记已经接收且接收长度真确
			{
				SyncUart_OK = 1;//标记uart收一个完整的数据帧
				GPIO_ResetBits(GPIOD,mLED3);
			}
		}
		else if(rev == '#'){ //'#'帧起始符
			SyncIndex = 0;
			SyncUart_OK = 0;
			memset(&sync_UartData,0,sizeof(sync_UartData));
		}
		else if(SyncIndex > sizeof(sync_UartData)){ //接收大小超过我们要的真确帧
			SyncIndex = 0;
		}
		
		*( (unsigned char*)&sync_UartData + SyncIndex) = rev;
		SyncIndex++;
		*/	
		if(SyncIndex==0)//一个指向数据移动到的位置a[i] i++;
		{
				memset(&sync_UartData,0,sizeof(sync_UartData));
		}
		//这里收到的第一个数据是0x23
		*( (unsigned char*)&sync_UartData + SyncIndex) = USART_ReceiveData(USART1);
		SyncIndex++;
		if(SyncIndex == 1)//判断收到头的第一个字段
		{		
			//判断帧的起始字符(#)0x23
			if( sync_UartData.frame_head != 0x23)
			{
				SyncIndex--;	
			}
		}
		//接收到的数据大小等于帧头的大小帧头接收完成
		if(SyncIndex==sizeof(sync_UartData))
		{	//结尾标记(0x0d和0x0a)同时满足
			if((sync_UartData.end_flag2==0x0a)&&(sync_UartData.end_flag1==0x0d))
			{
				//printf("\n");
				SyncUart_OK = 1;//标记uart收到数据
				SyncIndex = 0;//地址指针偏移量清0
				GPIO_ResetBits(GPIOD,mLED3);//熄灭LED3(PPS收到闪烁效果)
			}
		}
		else if(SyncIndex>sizeof(sync_UartData))
		{
			SyncIndex = 0;
		}
	}
}

void Hand_serialSync(void)
{
	if(SyncUart_OK == 1)//串口标记收到对时报文
	{
		unsigned char state_temp;
		tTime sulocaltime;								//UTC时间
		TimeInternal time;
		SyncUart_OK = 0;
		state_temp = sync_UartData.state_flag4 - 0x30;
		if(state_temp >=0x0a)
		{
			leapwring = 1;									//闰秒预告
		}
		sTime.SubTime.seconds = 0;
		sTime.SubTime.nanoseconds = 0;
		getTime(&time);										//取ETH的硬件时间
		//lastseconds = time.seconds%60;		//上一秒取余2017.06.05
		state_temp =  sync_UartData.state_flag1 - 0x30;//ascii的0对应的十六进制数是0x30
	
		if(state_temp != 0){
			if(leapNum<5)
				leapNum++;
			else{
				if(state_temp == 2)//正闰秒
					leap61=1;
				else if(state_temp == 3)//负闰秒
					leap59=1;
			}	
		}
		else{
			if(leapNum<=0){
				leap61=0;
				leap59=0;
			}
			else{
				leapNum--;
			}	
		}				
		//串口报文转化成UTC时间
		sulocaltime.usYear=(sync_UartData.year_4-0x30)*1000+(sync_UartData.year_3-0x30)*100+((sync_UartData.year_2 - 0x30 )*10+(sync_UartData.year_1-0x30));
		sulocaltime.ucMon =(sync_UartData.month_2 - 0x30)*10+(sync_UartData.month_1-0x30);//此处月的值为1－12，系统ttime的月值应该是0－11.在后面的timetoseconds函数中，此处的月值减了1.
		sulocaltime.ucMday=(sync_UartData.day_2 - 0x30)*10+(sync_UartData.day_1- 0x30);
		sulocaltime.ucHour=(sync_UartData.hour_2 - 0x30)*10+(sync_UartData.hour_1-0x30);
		sulocaltime.ucMin =(sync_UartData.min_2 - 0x30)*10+(sync_UartData.min_1- 0x30);
		sulocaltime.ucSec =(sync_UartData.sec_2 - 0x30)*10+(sync_UartData.sec_1- 0x30);
		//系统的串口时间
		sTime.serailTime.seconds = Serial_Htime(&sulocaltime);		//UTC时间转化成整秒
		//printf("sec= %d,serialtime= %d\n",sulocaltime.ucSec,sTime.serailTime.seconds);
		sTime.serailTime.nanoseconds = 0;													//ns部分置0
		offset_time(&sTime);			//计算串口时间和本地时间的偏差	
		abjClock(sTime.SubTime); 	//调整偏差
	}
}

void handleap(void)//处理模拟闰秒
{
	if(leapNum >= 3)//连续3次闰秒预告
	{
		TimeInternal time1;
		struct ptptime_t timeupdata;
		getTime(&time1);
		//闰秒时间调整
		if(leap61 != 0)//正闰秒
		{
			if((time1.seconds%60) == 1)//g_CFL_ucSec2是收到模拟闰秒帧的一个时间每秒会加一次
			{
				timeupdata.tv_sec = -1;
				timeupdata.tv_nsec = 0;
				ETH_PTPTime_UpdateOffset(&timeupdata);
				leap61 = 0;	
				leapNum = 0;
				printf("leap61\n");
			}
		}
		else if(leap59 != 0)//负闰秒
		{
			if((time1.seconds%60) == 59)
			{
				timeupdata.tv_sec = 1;//
				timeupdata.tv_nsec = 0;
				ETH_PTPTime_UpdateOffset(&timeupdata);
				leap59 = 0;
				leapNum = 0;
				printf("leap59\n");
			}
		}
	}
}

//#endif


