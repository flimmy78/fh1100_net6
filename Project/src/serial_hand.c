#include "ptpd.h"
#include "ntp.h"
#include "main.h"
#include "netconf.h"
#include "share.h"

extern sysTime sTime;
extern RunTimeOpts rtOpts;
extern PtpClock  G_ptpClock;

//#if defined(Sync_Device) 
struct Sync_UartData sync_UartData;			//���ڱ��Ľṹ
unsigned char SyncIndex = 0;						//���ڱ���ָ��
unsigned char SyncUart_OK;//���ڱ��Ľ��ܱ�ʶ
unsigned char leap61;			//������
unsigned char leap59;			//������
unsigned char leapwring;	//����Ԥ��
unsigned char leapNum;		//�����־�ļ���
//unsigned char lastseconds;//����ά����ʱ�����һ��ֵ2017.06.05

void USART1_IRQHandler(void)//godin ptp���մ���ʱ����Ϣ
{	
	if(USART_GetITStatus(USART1,USART_IT_RXNE) != RESET)//����Ƿ���ָ�����жϷ���
	{	//���յ����ݲ����ж� 
		USART_ClearITPendingBit(USART1,USART_IT_RXNE);//��������жϱ��	
		/*		
		rev = USART_ReceiveData(USART1);	
		if(rev == 0x0a)//�յ��ڶ������ձ��
		{
			if(sync_UartData.end_flag1==0x0d)//��һ������Ѿ������ҽ��ճ�����ȷ
			{
				SyncUart_OK = 1;//���uart��һ������������֡
				GPIO_ResetBits(GPIOD,mLED3);
			}
		}
		else if(rev == '#'){ //'#'֡��ʼ��
			SyncIndex = 0;
			SyncUart_OK = 0;
			memset(&sync_UartData,0,sizeof(sync_UartData));
		}
		else if(SyncIndex > sizeof(sync_UartData)){ //���մ�С��������Ҫ����ȷ֡
			SyncIndex = 0;
		}
		
		*( (unsigned char*)&sync_UartData + SyncIndex) = rev;
		SyncIndex++;
		*/	
		if(SyncIndex==0)//һ��ָ�������ƶ�����λ��a[i] i++;
		{
				memset(&sync_UartData,0,sizeof(sync_UartData));
		}
		//�����յ��ĵ�һ��������0x23
		*( (unsigned char*)&sync_UartData + SyncIndex) = USART_ReceiveData(USART1);
		SyncIndex++;
		if(SyncIndex == 1)//�ж��յ�ͷ�ĵ�һ���ֶ�
		{		
			//�ж�֡����ʼ�ַ�(#)0x23
			if( sync_UartData.frame_head != 0x23)
			{
				SyncIndex--;	
			}
		}
		//���յ������ݴ�С����֡ͷ�Ĵ�С֡ͷ�������
		if(SyncIndex==sizeof(sync_UartData))
		{	//��β���(0x0d��0x0a)ͬʱ����
			if((sync_UartData.end_flag2==0x0a)&&(sync_UartData.end_flag1==0x0d))
			{
				//printf("\n");
				SyncUart_OK = 1;//���uart�յ�����
				SyncIndex = 0;//��ַָ��ƫ������0
				GPIO_ResetBits(GPIOD,mLED3);//Ϩ��LED3(PPS�յ���˸Ч��)
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
	if(SyncUart_OK == 1)//���ڱ���յ���ʱ����
	{
		unsigned char state_temp;
		tTime sulocaltime;								//UTCʱ��
		TimeInternal time;
		SyncUart_OK = 0;
		state_temp = sync_UartData.state_flag4 - 0x30;
		if(state_temp >=0x0a)
		{
			leapwring = 1;									//����Ԥ��
		}
		sTime.SubTime.seconds = 0;
		sTime.SubTime.nanoseconds = 0;
		getTime(&time);										//ȡETH��Ӳ��ʱ��
		//lastseconds = time.seconds%60;		//��һ��ȡ��2017.06.05
		state_temp =  sync_UartData.state_flag1 - 0x30;//ascii��0��Ӧ��ʮ����������0x30
	
		if(state_temp != 0){
			if(leapNum<5)
				leapNum++;
			else{
				if(state_temp == 2)//������
					leap61=1;
				else if(state_temp == 3)//������
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
		//���ڱ���ת����UTCʱ��
		sulocaltime.usYear=(sync_UartData.year_4-0x30)*1000+(sync_UartData.year_3-0x30)*100+((sync_UartData.year_2 - 0x30 )*10+(sync_UartData.year_1-0x30));
		sulocaltime.ucMon =(sync_UartData.month_2 - 0x30)*10+(sync_UartData.month_1-0x30);//�˴��µ�ֵΪ1��12��ϵͳttime����ֵӦ����0��11.�ں����timetoseconds�����У��˴�����ֵ����1.
		sulocaltime.ucMday=(sync_UartData.day_2 - 0x30)*10+(sync_UartData.day_1- 0x30);
		sulocaltime.ucHour=(sync_UartData.hour_2 - 0x30)*10+(sync_UartData.hour_1-0x30);
		sulocaltime.ucMin =(sync_UartData.min_2 - 0x30)*10+(sync_UartData.min_1- 0x30);
		sulocaltime.ucSec =(sync_UartData.sec_2 - 0x30)*10+(sync_UartData.sec_1- 0x30);
		//ϵͳ�Ĵ���ʱ��
		sTime.serailTime.seconds = Serial_Htime(&sulocaltime);		//UTCʱ��ת��������
		//printf("sec= %d,serialtime= %d\n",sulocaltime.ucSec,sTime.serailTime.seconds);
		sTime.serailTime.nanoseconds = 0;													//ns������0
		offset_time(&sTime);			//���㴮��ʱ��ͱ���ʱ���ƫ��	
		abjClock(sTime.SubTime); 	//����ƫ��
	}
}

void handleap(void)//����ģ������
{
	if(leapNum >= 3)//����3������Ԥ��
	{
		TimeInternal time1;
		struct ptptime_t timeupdata;
		getTime(&time1);
		//����ʱ�����
		if(leap61 != 0)//������
		{
			if((time1.seconds%60) == 1)//g_CFL_ucSec2���յ�ģ������֡��һ��ʱ��ÿ����һ��
			{
				timeupdata.tv_sec = -1;
				timeupdata.tv_nsec = 0;
				ETH_PTPTime_UpdateOffset(&timeupdata);
				leap61 = 0;	
				leapNum = 0;
				printf("leap61\n");
			}
		}
		else if(leap59 != 0)//������
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


