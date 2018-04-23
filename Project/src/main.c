#include "stm32f4x7_eth.h"
#include "netconf.h"
#include "main.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "string.h"
#include "serial_debug.h"
#include "ptpd.h"
#include "share.h"
#include "sys.h"
#include "lcdmessage.h"
/************************************���Զ������printf*******************************************/
#define SYSTEMTICK_PERIOD_MS  10
#define ITM_Port8(n)    (*((volatile unsigned char *)(0xE0000000+4*n)))
#define ITM_Port16(n)   (*((volatile unsigned short*)(0xE0000000+4*n)))
#define ITM_Port32(n)   (*((volatile unsigned long *)(0xE0000000+4*n)))
#define DEMCR           (*((volatile unsigned long *)(0xE000EDFC)))
#define TRCENA          0x01000000
extern void ptpHandleap(RunTimeOpts *rtOpts, PtpClock *ptpClock);
struct __FILE 
{ 
	int handle1; /* Add whatever you need here */ 
};
FILE __stdout;
FILE __stdin;
int fputc(int ch, FILE *f)
{
	return ITM_SendChar(ch);
}

void cpuidGetId(void);
void Read_ConfigData(FigStructData *FigData);
__IO uint32_t LocalTime = 0; /* this variable is used to create a time reference incremented by 10ms *///����ʱ��
uint32_t LocalTime1 = 0;
uint32_t timingdelay;
volatile sysTime sTime;
volatile Servo Mservo; //���ڱ���ʱ�Ӻ�GPS��ʱ�ĵ�������ָ��
Filter	 MofM_filt;			//����ʱ�����ݼ��������˲���
FigStructData GlobalConfig;
unsigned int test_fjm;
uint32_t mcuID[3];
extern uint8_t CAN1_Mode_Init(void);
extern void ARM_to_FPGA(void);
extern PtpClock G_ptpClock;
extern RunTimeOpts rtOpts;
extern uint8_t synflags;
int 	 oldseconds = 0;

uint8_t PTP_MS_MODE = 0;
uint8_t vsmsg=0x11;//����汾V1.1
//LED1-����
//LED2-ͬ��
//LED3-PPS
//LED4-����ѱ��
static void sendtime(void);	
int main(void)
{
	Mservo.ai	  = 16;	//����ʱ�䳣��I	 = 16	
	Mservo.ap		= 2;	//����ϵ��P		   = 2
	MofM_filt.n = 0;				
	MofM_filt.s = 1;		//alpha 0.1-0.4 
	sTime.noAdjust 		 = FALSE;			//�������ϵͳʱ��
	sTime.noResetClock = FALSE;			//��������ϵͳʱ��
	RCC_Configuration();
	Read_ConfigData(&GlobalConfig); //ע�⣺��Ҫ�ȶ�дFLASH���ڿ�Ӳ���ж�ǰ
//	GlobalConfig.WorkMode =4;
  printf("WorkMode=%d",GlobalConfig.WorkMode);
	GPIO_Configuration();
	EXTI_Configuration();								
	ETH_GPIO_Config();
	ETH_MACDMA_Config();
 	LwIP_Init();
	switch(GlobalConfig.WorkMode)
	{
		case 0:case 1:case 2:case 3:
			printf("PTP��\n");
			PTP_MS_MODE=1;		//PTP��
		  UART1_Configuration();//����ͨ������1��ʱ
			PTPd_Init();
			break;
		case 4:case 5:case 6:case 7:
			printf("PTP��\n");
			PTP_MS_MODE=2;		//PTP��
		  UART6_Configuration();//�ӣ�ͨ������6���ʱ��
			PTPd_Init();
			break;
		default:
			printf("NTP\n");
			PTP_MS_MODE=0;	  //NTP
		  UART1_Configuration();//ͨ������1��ʱ
		  NTP_Init();
	}
	CAN1_Mode_Init();								//���忨��ַ����ʼ��CAN
	NVIC_Configuration();		
	STM_LEDon(mLED1); 				 			//��ʼ����ɣ�����LED1
	printf("�ȴ�ͬ��...\n");
	
  while (1)
		{ 
			CanRxHandle();
			LwIP_Periodic_Handle(LocalTime);
			switch(PTP_MS_MODE)
				{
					case 0:
							handleap(); 				
							Hand_serialSync();
							break;
					case 1:
							handleap(); 				
							Hand_serialSync();
							if(synflags)				//ͬ��֮��ſ�ʼ��PTP����
								ptpd_Periodic_Handle(LocalTime);	
							break;
					case 2:
							ptpd_Periodic_Handle(LocalTime);
							ptpHandleap(&rtOpts, &G_ptpClock);	
							sendtime();
							break;
				}
		}
}


void sendtime(void)
{
	TimeInternal Itime;
	if(LocalTime%50 == 0 )
					{
						getTime(&Itime);	
						 if(Itime.nanoseconds>140000000  && Itime.nanoseconds<250000000 )
							 { 
									if( Itime.seconds != oldseconds )
									{				
											ARM_to_FPGA();
											oldseconds = Itime.seconds;
										  //printf("ARM_to_FPGA:");
									}	
							 }
					}			
}


void Delay(uint32_t nCount)
{
  timingdelay = LocalTime + nCount;  
  while(timingdelay > LocalTime)
		{     
		}
}

void Time_Update(void)
{
  LocalTime += SYSTEMTICK_PERIOD_MS;
}


void Read_ConfigData(FigStructData *FigData)//godin flash������Ϣ��ȡ
{
	int32_t readflash[10] = {0},i;
	FLASH_Read(FLASH_SAVE_ADDR1,readflash,10);//������Ϣ��readflash��
	for(i=0;i<10;i++)
		printf("FLASH_Read[%02d]: %08x\n",i,readflash[i]);
	if(readflash[0] == 0xffffffff)//Ĭ������
		{
			FigData->IPaddr		 = 0xc0a80100|13;	//192.168.1.13 Ĭ��IP
			FigData->NETmark	 = 0xffffff00;	
			FigData->GWaddr		 = 0xc0a80101;	
			FigData->MACaddr[0] = 0x06;
			FigData->MACaddr[1] = 0x04;
			FigData->MACaddr[2] = 0x00;
			FigData->MACaddr[3] = 0xFF;
			cpuidGetId(); 										//��ȡоƬIDΨһ��ţ�����ȡ���MAC��ַ
			FigData->MACaddr[4] = rand()%255; //ͨ���������ȡmac��ַ
			FigData->MACaddr[5] = rand()%255; //ͨ���������ȡmac��ַ
			//FigData->DstIpaddr = 0xc0a80100|11;	//192.168.1.11
			FigData->LocalPort = 0;					//godin Ƶ����Ϣ�˿ںţ��鲥�˿ں�
			FigData->DstPort 	 = 0;					//godin Ƶ����ϢĿ�ĵ�ַ�˿ں�
			FigData->MSorStep           = 0; 												//������
			FigData->ClockDomain 				= DEFAULT_DOMAIN_NUMBER;		//Ĭ��ʱ����0
			FigData->WorkMode		 				=	8;												//NTP
			FigData->ip_mode	   				= 0;												//Ĭ���鲥
			FigData->AnnounceInterval 	= DEFAULT_ANNOUNCE_INTERVAL;				//1
			FigData->AnnounceOutTime		= DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT; //6
			FigData->SyncInterval				=	DEFAULT_SYNC_INTERVAL; 						//0
			FigData->PDelayInterval			= DEFAULT_PDELAYREQ_INTERVAL; 			//1
			FigData->DelayInterval			= DEFAULT_DELAYREQ_INTERVAL; 				//0
			FigData->UTCoffset          = 0;
			FigData->clockClass 				= 48; 							//32
			FigData->priority1 					= 127; 								//128
			FigData->priority2 					= 127; 								//128
			memcpy((int8_t *)readflash,(int8_t *)FigData,16);
			readflash[4]	=	((FigData->LocalPort)<<16)|FigData->DstPort;
			readflash[5]	=	((FigData->WorkMode)<<24)|((FigData->clockClass)<<16)|((FigData->UTCoffset)<<8);
			readflash[6]	=	((FigData->MACaddr[0])<<24)|((FigData->MACaddr[1])<<16)|((FigData->MACaddr[2])<<8)|(FigData->MACaddr[3]);
			readflash[7]	=	((FigData->MACaddr[4])<<24)|((FigData->MACaddr[5])<<16)|((FigData->MSorStep)<<8)|(FigData->ClockDomain);
			readflash[8]	=	((FigData->AnnounceInterval)<<24)|((FigData->AnnounceOutTime)<<16)|((FigData->SyncInterval)<<8)|(FigData->PDelayInterval);
			readflash[9]	=	((FigData->DelayInterval)<<24)|((FigData->priority1)<<16)|((FigData->priority2)<<8)|(FigData->tmp2);
			for(i=0;i<10;i++)
				printf("FLASH_Write[%02d]: %08x\n",i,readflash[i]);
			FLASH_Write(FLASH_SAVE_ADDR1,readflash,10);
			for(i=0;i<1000;i++){}//�ȴ���д����	
		}
	else 
		{ //��ȡ���µ�����
			FigData->IPaddr 	= readflash[0];
			FigData->NETmark 	= readflash[1];
			FigData->GWaddr 	= readflash[2];
			FigData->DstIpaddr= readflash[3];
			FigData->LocalPort	= (uint16_t)((readflash[4]&0xffff0000)>>16); 
			FigData->DstPort	  = (uint16_t)((readflash[4]&0x0000ffff)>>0);
			FigData->WorkMode		= (uint8_t)((readflash[5]&0xff000000)>>24);			//ptp����ģʽ
			FigData->clockClass = (uint8_t)((readflash[5]&0xff0000)>>16);			  //NTP����ģʽ(�����鲥�㲥
			FigData->UTCoffset 	= (uint8_t)((readflash[5]&0xff00)>>8);
			FigData->MACaddr[0] = (uint8_t)((readflash[6]&0xff000000)>>24);
			FigData->MACaddr[1] = (uint8_t)((readflash[6]&0xff0000)>>16);
			FigData->MACaddr[2] = (uint8_t)((readflash[6]&0xff00)>>8);	
			FigData->MACaddr[3] = (uint8_t)(readflash[6]&0xff);	
			FigData->MACaddr[4] = (uint8_t)((readflash[7]&0xff000000)>>24);
			FigData->MACaddr[5] = (uint8_t)((readflash[7]&0xff0000)>>16);
			FigData->MSorStep		= (uint8_t)((readflash[7]&0xff00)>>8);			//PTPһ����������
			FigData->ClockDomain= (uint8_t)(readflash[7]&0xff);				//ʱ����
			FigData->AnnounceInterval		= (uint8_t)((readflash[8]&0xff000000)>>24);
			FigData->AnnounceOutTime 		= (uint8_t)((readflash[8]&0xff0000)>>16);
			FigData->SyncInterval 			= (uint8_t)((readflash[8]&0xff00)>>8);
			FigData->PDelayInterval 		= (uint8_t)(readflash[8] &0xff);	
			FigData->DelayInterval  		= (uint8_t)((readflash[9] &0xff000000)>>24);
			FigData->priority1	 				= (uint8_t)((readflash[9]&0xff0000)>>16);
			FigData->priority2					= (uint8_t)((readflash[9]&0xff00)>>8);	
		}
}


void cpuidGetId(void)
{
	mcuID[0] = *(__IO u32*)(0x1FFF7A10);
	mcuID[1] = *(__IO u32*)(0x1FFF7A14);
	mcuID[2] = *(__IO u32*)(0x1FFF7A18);
	srand((mcuID[0]+mcuID[1])+mcuID[2]); //������������ɵ�����
}

void STM_LEDon(uint16_t LEDx)
{
	GPIO_SetBits(GPIOD,LEDx);
}

void STM_LEDoff(uint16_t LEDx)
{
	GPIO_ResetBits(GPIOD,LEDx);
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
  while (1)
  {}
}
#endif


