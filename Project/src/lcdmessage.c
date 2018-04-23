#include "lcdmessage.h"
#include "ptpd.h" 
#include "can.h"
#include "netconf.h"
#include "main.h"

//版本日期 20161108 板卡地址保留8位
extern PtpClock G_ptpClock;
extern RunTimeOpts rtOpts;
extern FigStructData GlobalConfig;
extern uint32_t CAN1_SEND_ID;
extern uint8_t vsmsg;

/***定义PTP配置信息结构体***/
typedef struct 
{		
	u8 SLAVEFLAG;								//11.主从配置	
	u8 TWOSTEPFLAG ;						//12.两步法标志
	u8 DOMAINNUMBER ;						//13.时钟域
	u8 ANNOUNCE_INTERVAL;				//14.Announce报文周期
	u8 ANNOUNCE_RECEIPT_TIMEOUT;//15.Announce接收超时时间
	u8 SYNC_INTERVAL;						//16.Sync报文周期
	u8 PDELAYREQ_INTERVAL;			//17.PDelay报文周期
  u8 DELAYREQ_INTERVAL;				//18.Delay报文周期
	u8 SLAVE_PRIORITY1 ;				//19.优先级1
  u8 SLAVE_PRIORITY2 ;				//20.优先级2
} PTPD_Type;

typedef struct 
{
	u8 	slot_num;				//0.插槽位/网口号		
	u8 	ip[4];    			//1.IP地址					
	u8 	mask[4]; 				//2.子网掩码						
	u8 	gw[4]; 					//3.网关
	u8 	remoteip[4];		//7.对端IP地址 
	u16 localport;			//4.端口号	
	u16 remoteport;			//6.对端端口号
	u8 	ethernet_mode;	//10.网络模式 0代表ETH/ETE；1代表ETH-PTP;2代表UDP-ETE;3代表UDP-PTP；4、NTP；
	u8 	spreadmode;			//8.传输方式		
	u8 	spreadgaps;			//9.广播间隔	
	u8 	mac[6];      		//5.MAC地址		
	PTPD_Type ptpd;			//PTP结构体
} NetDev_Type;


void u8_to_u32(u8* u8buff,u32 *u32buff)
{
	*u32buff = (u8buff[0]<<24)|(u8buff[1]<<16)|(u8buff[2]<<8)|(u8buff[3]);
}

void u32_to_u8(u32 *u32buff,u8* u8buff)
{
	u8buff[0] = ((*u32buff)&0xff000000)>>24;
	u8buff[1] = ((*u32buff)&0xff0000)>>16;
	u8buff[2] = ((*u32buff)&0xff00)>>8;
	u8buff[3] = ((*u32buff)&0xff);
}



void CanRxHandle()
{
	if(((CAN1_RX0_STA&0xff00)>>8)>0)
		{
			int32_t readflash[10] = {0};
			u32 i;
			u8  msg[40]  ={0};
			u32 can_sendid = CAN1_SEND_ID;
			uint16_t CAN_RECIVE_FLAG;
			CAN_RECIVE_FLAG=(CAN1_RX0_STA&0xf00)>>8;
			if(CAN_RECIVE_FLAG==1)
				{
					if(CAN1_RX0_BUF[39] == ((can_sendid&0xff0000)>>16))
						{
							if((CAN1_RX0_BUF[0]!=0x00)&&(CAN1_RX0_BUF[4]!=0x00))
								{
									
									memcpy((int32_t *)readflash,(int32_t *)&CAN1_RX0_BUF,40);
									for(i=0;i<10;i++)
										{
											u8_to_u32(&CAN1_RX0_BUF[i*4],(u32 *)(&readflash[i]));
										}
									FLASH_Read(FLASH_SAVE_ADDR1+16,readflash+4,6);//读出信息到readflash中
									for(i=0;i<10;i++)
										printf("FLASH_Write[%02d]: %08x\n",i,readflash[i]);		
									NVIC_DisableIRQ(CAN1_RX0_IRQn);
									NVIC_DisableIRQ(ETH_IRQn);	
									NVIC_DisableIRQ(USART1_IRQn);	
									NVIC_DisableIRQ(EXTI1_IRQn);
									for(i=0;i<10000;i++){}//等待中断关闭
									FLASH_Write(FLASH_SAVE_ADDR1,readflash,10);
									for(i=0;i<10000000;i++){}//等待烧写结束	
									NVIC_SystemReset();
								}
							else if((CAN1_RX0_BUF[24]|CAN1_RX0_BUF[25]|CAN1_RX0_BUF[26]|CAN1_RX0_BUF[27]|CAN1_RX0_BUF[28]|CAN1_RX0_BUF[29])!=0x00)
								{
									memcpy((int32_t *)readflash,(int32_t *)&CAN1_RX0_BUF,40);
									for(i=0;i<10;i++)
										{
											u8_to_u32(&CAN1_RX0_BUF[i*4],(u32 *)(&readflash[i]));
										}
									readflash[9] &=0x00ffffff; //写FLASH前关闭中断
									FLASH_Read(FLASH_SAVE_ADDR1,readflash,4);//读出信息到readflash中	
									for(i=0;i<10;i++)
										printf("FLASH_Write[%02d]: %08x\n",i,readflash[i]);
									NVIC_DisableIRQ(CAN1_RX0_IRQn);
									NVIC_DisableIRQ(ETH_IRQn);	
									NVIC_DisableIRQ(USART1_IRQn);	
									NVIC_DisableIRQ(EXTI1_IRQn);		
									for(i=0;i<10000;i++){}//等待中断关闭
									FLASH_Write(FLASH_SAVE_ADDR1,readflash,10);
									for(i=0;i<10000000;i++){}//等待烧写结束	
									NVIC_SystemReset();
								}
							
						}
				}	
			else if(CAN_RECIVE_FLAG==2)
				{				
					memcpy((int8_t *)msg,(u32 *)&GlobalConfig,40);
					u32_to_u8(&GlobalConfig.IPaddr,&msg[0]);
					u32_to_u8(&GlobalConfig.NETmark,&msg[4]);
					u32_to_u8(&GlobalConfig.GWaddr,&msg[8]);
					u32_to_u8(&GlobalConfig.DstIpaddr,&msg[12]);
					msg[16] = (GlobalConfig.LocalPort&0xff00)>>8;
					msg[17] = (GlobalConfig.LocalPort&0xff);
					msg[18] = (GlobalConfig.DstPort&0xff00)>>8;
					msg[19] = (GlobalConfig.DstPort&0xff);
					msg[23] = vsmsg;
					for(i=0;i<5;i++)
						{
							CAN1_Send_Msg(&msg[i*8],can_sendid+i,8);
						}
				}	
			CAN1_RX0_STA=0;		
		}
}



