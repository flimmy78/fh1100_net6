#include "fh1000.h"
#include "ethernetif.h"
#include "ntp.h"
#include "flash_conf.h"
#include "share.h"

unsigned int rec_SYNIndex;
CfgData   g_recvHead;
unsigned char leap59; 
unsigned char leap61;
unsigned char leapwarning;
unsigned char leapNUM;
//CfgData  g_recvHead_temp;
unsigned char rec_data_flag=0;
unsigned char lastseconds;
void handleap(void)
{
	if(leapNUM >= 3)
	{
	struct ptptime_t timeupdata;
	//天线闰秒
	if(leap61 )//天线正润秒
	{
		if(lastseconds == 1)
		{
			timeupdata.tv_sec = -1;
			timeupdata.tv_nsec = 0;
			ETH_PTPTime_UpdateOffset(&timeupdata);
			leap61 = 0;
			leapNUM = 0;
//			printf("hand leap 61\n");
		}
	}
	else if(leap59)//天线负闰秒
	{
		if(lastseconds == 59)
		{
			timeupdata.tv_sec = 1;//
			timeupdata.tv_nsec = 0;
			ETH_PTPTime_UpdateOffset(&timeupdata);
			leap59 = 0;
			leapNUM = 0;
//			printf("hand leap 58\n");
		}		
	}
	}
}

void USART1_IRQHandler(void)
{
	if(USART_GetITStatus(USART1,USART_IT_RXNE) != RESET)//检查是否是指定的中断发生
	{//接收到数据产生中断 
		if(rec_SYNIndex==0)//一个指向数据移动到的位置a[i] i++;
		{
			memset(&g_recvHead,0,sizeof(CfgData));
		}
		//这里收到的第一个数据是0x23
		*((unsigned char*)(&g_recvHead)+rec_SYNIndex) = USART_ReceiveData(USART1);
		rec_SYNIndex++;
		if(rec_SYNIndex == 1)
		{
			if(g_recvHead.frame_head != 0x23)
				rec_SYNIndex--;
		}
		if(rec_SYNIndex == sizeof(CfgData))
		{
			rec_data_flag = 1;
			rec_SYNIndex = 0;
		}
		
		USART_ClearITPendingBit(USART1,USART_IT_RXNE);//清除接收中断标记	
	}	
}

void Hand_FH1000(void)
{
	if(rec_data_flag == 1)
	{
		struct ptptime_t  timestamp;
		tTime sulocaltime;
		unsigned long ulTime;
		unsigned char state_temp;
		
		ETH_PTPTime_GetTime(&timestamp);
		lastseconds = timestamp.tv_sec%60;
		state_temp = g_recvHead.state_flag4 - 0x30;
		if(state_temp >=0x0a)
		{
					leapwarning = 3;
		}	
		else
		{
				state_temp =  g_recvHead.state_flag1 - 0x30;//ascii的0对应的十六进制数是0x30
				if(state_temp != 0)
						leapNUM++;
				if(state_temp == 2)//正闰秒
				{
					leap61 = 1;
				}
				else if(state_temp == 3)//负闰秒
				{
					leap59 = 1;
				}
				else{//正常状态不闰秒
					leapwarning = 0;
					leap61 = 0;
					leap59 = 0;
					leapNUM = 0;
				}
		}
		handleap();
		sulocaltime.usYear=(g_recvHead.year_4 - 0x30)*1000 + ( g_recvHead.year_3 - 0x30)*100+((g_recvHead.year_2 - 0x30 )*10+(g_recvHead.year_1 - 0x30));
		sulocaltime.ucMon=(g_recvHead.month_2 - 0x30)*10 + (g_recvHead.month_1 - 0x30);//此处月的值为1－12，系统ttime的月值应该是0－11.在后面的timetoseconds函数中，此处的月值减了1.
		sulocaltime.ucMday=(g_recvHead.day_2 - 0x30)*10 + (g_recvHead.day_1- 0x30);
		sulocaltime.ucHour=(g_recvHead.hour_2 - 0x30)*10 + (g_recvHead.hour_1 - 0x30);
		sulocaltime.ucMin =(g_recvHead.min_2 - 0x30)*10+(g_recvHead.min_1- 0x30);
		sulocaltime.ucSec =(g_recvHead.sec_2 - 0x30)*10+(g_recvHead.sec_1- 0x30);
		ulTime = Serial_Htime(&sulocaltime);
		offset_time(&sTime);	//计算偏差
		abjClock(sTime.SubTime); //调整偏差
		rec_data_flag = 0;
	}
}