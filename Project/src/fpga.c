#include "share.h"
#include "ptpd.h"
#include "main.h"
//使用串口发送
extern PtpClock G_ptpClock;
extern RunTimeOpts rtOpts;
extern uint8_t synflags;
extern FigStructData GlobalConfig;
extern uint8_t tbflags;
extern void ptpHandleap(RunTimeOpts *rtOpts, PtpClock *ptpClock);
uint8_t myonetwoflag=0;
//static const unsigned char table[] =
//{
//    0x00,0x31,0x62,0x53,0xc4,0xf5,0xa6,0x97,0xb9,0x88,0xdb,0xea,0x7d,0x4c,0x1f,0x2e,
//    0x43,0x72,0x21,0x10,0x87,0xb6,0xe5,0xd4,0xfa,0xcb,0x98,0xa9,0x3e,0x0f,0x5c,0x6d,
//    0x86,0xb7,0xe4,0xd5,0x42,0x73,0x20,0x11,0x3f,0x0e,0x5d,0x6c,0xfb,0xca,0x99,0xa8,
//    0xc5,0xf4,0xa7,0x96,0x01,0x30,0x63,0x52,0x7c,0x4d,0x1e,0x2f,0xb8,0x89,0xda,0xeb,
//    0x3d,0x0c,0x5f,0x6e,0xf9,0xc8,0x9b,0xaa,0x84,0xb5,0xe6,0xd7,0x40,0x71,0x22,0x13,
//    0x7e,0x4f,0x1c,0x2d,0xba,0x8b,0xd8,0xe9,0xc7,0xf6,0xa5,0x94,0x03,0x32,0x61,0x50,
//    0xbb,0x8a,0xd9,0xe8,0x7f,0x4e,0x1d,0x2c,0x02,0x33,0x60,0x51,0xc6,0xf7,0xa4,0x95,
//    0xf8,0xc9,0x9a,0xab,0x3c,0x0d,0x5e,0x6f,0x41,0x70,0x23,0x12,0x85,0xb4,0xe7,0xd6,
//    0x7a,0x4b,0x18,0x29,0xbe,0x8f,0xdc,0xed,0xc3,0xf2,0xa1,0x90,0x07,0x36,0x65,0x54,
//    0x39,0x08,0x5b,0x6a,0xfd,0xcc,0x9f,0xae,0x80,0xb1,0xe2,0xd3,0x44,0x75,0x26,0x17,
//    0xfc,0xcd,0x9e,0xaf,0x38,0x09,0x5a,0x6b,0x45,0x74,0x27,0x16,0x81,0xb0,0xe3,0xd2,
//    0xbf,0x8e,0xdd,0xec,0x7b,0x4a,0x19,0x28,0x06,0x37,0x64,0x55,0xc2,0xf3,0xa0,0x91,
//    0x47,0x76,0x25,0x14,0x83,0xb2,0xe1,0xd0,0xfe,0xcf,0x9c,0xad,0x3a,0x0b,0x58,0x69,
//    0x04,0x35,0x66,0x57,0xc0,0xf1,0xa2,0x93,0xbd,0x8c,0xdf,0xee,0x79,0x48,0x1b,0x2a,
//    0xc1,0xf0,0xa3,0x92,0x05,0x34,0x67,0x56,0x78,0x49,0x1a,0x2b,0xbc,0x8d,0xde,0xef,
//    0x82,0xb3,0xe0,0xd1,0x46,0x77,0x24,0x15,0x3b,0x0a,0x59,0x68,0xff,0xce,0x9d,0xac
//};

unsigned char cal_crc_table(unsigned char *ptr, unsigned char len) 
{
    unsigned char  crc = 0x00;
    //unsigned char  old = 0x00;
    while (len--)
    {
			  //old=crc;
        crc = crc ^ *ptr++;
			//myprintf(" car%02x=%02x^%02x(%d)\n",crc,old,*ptr,len);
    }
		//printf("\n");
    return (crc);
}


static void UART6Send(const unsigned char *pucBuffer, unsigned long ulCount)
{
// Loop while there are more characters to send.
	while(ulCount--)
	{
	// Write the next character to the UART.
  //	printf("%02X ",*pucBuffer);
		USART_SendData(USART6,*pucBuffer++);
		while(USART_GetFlagStatus(USART6, USART_FLAG_TXE) == RESET){}
	}
 	//printf("\n");
}

void ARM_to_FPGA(void)
{
	ARMtoFH1100 ARM_to_1000;
	TimeInternal  Localtime;
	tTime         L_sLocalTime;
	int tmp1;
	//int tmp2;
	unsigned char cksum2;
	ARM_to_1000.synA = 0x90;
	ARM_to_1000.synB = 0xeb;
	ARM_to_1000.messagelen = 0x1a;

	//ptpHandleap(&rtOpts, &G_ptpClock);	
	getTime(&Localtime);
	ulocaltime(Localtime.seconds+3600*8, &L_sLocalTime);//将秒时间转换为年月日时分秒
	
	ARM_to_1000.year1    = 0;
	ARM_to_1000.year2    = (L_sLocalTime.usYear-2000)%100;
	ARM_to_1000.month   = L_sLocalTime.ucMon+1; 
	ARM_to_1000.day     = L_sLocalTime.ucMday; 
	ARM_to_1000.hour    = L_sLocalTime.ucHour; 
	ARM_to_1000.minute  = L_sLocalTime.ucMin; 
	ARM_to_1000.seconds = L_sLocalTime.ucSec;

	
//	IntToHex(L_sLocalTime.usYear,(char *)&ARM_to_FPGAM.year);
//	IntToHex((L_sLocalTime.ucMon+1),(char*)&ARM_to_FPGAM.month); //L_sLocalTime.ucMon
//	IntToHex(L_sLocalTime.ucMday,(char *)&ARM_to_FPGAM.day); //L_sLocalTime.ucMday
//	L_sLocalTime.ucHour = (L_sLocalTime.ucHour+8)%24;//	小时加8 是将UTC时间转换成北京时间
//	IntToHex(L_sLocalTime.ucHour,(char *)&ARM_to_FPGAM.hour);
//	IntToHex(L_sLocalTime.ucMin, (char *)&ARM_to_FPGAM.minute);
//	IntToHex(L_sLocalTime.ucSec, (char *)&ARM_to_FPGAM.seconds);


//	printf("[ %s-",(char *)&ARM_to_FPGAM.year);
//	printf(" %s-", (char *)&ARM_to_FPGAM.month);
//	printf(" %s ] ",(char *)&ARM_to_FPGAM.day);
//	printf(" %s:",(char *)&ARM_to_FPGAM.hour);
//	printf(" %s:",(char *)&ARM_to_FPGAM.minute);
//	printf(" %s\n",(char *)&ARM_to_FPGAM.seconds);
	
		//if((!ptpClock->timePropertiesDS.leap59 && !ptpClock->timePropertiesDS.leap61) &&
	
	//if(leapwring)
		//ARM_to_FPGAM.leap = LI_ALARM; //未同步
	if(G_ptpClock.timePropertiesDS.leap61)
		ARM_to_1000.leap = 1;
	else if(G_ptpClock.timePropertiesDS.leap59)
		ARM_to_1000.leap = 2;
	else
		ARM_to_1000.leap = LI_NOWARNING;//正常时间


	if(tbflags == 1)
		ARM_to_1000.sync_state = 0x41;
	else
		ARM_to_1000.sync_state = 0x56;
	
	ARM_to_1000.sync_mode 			= GlobalConfig.WorkMode;
	ARM_to_1000.Running_state 	= G_ptpClock.portState;
	ARM_to_1000.bestmac_addr[0] = G_ptpClock.grandmasterIdentity[0];
	ARM_to_1000.bestmac_addr[1] = G_ptpClock.grandmasterIdentity[1];
	ARM_to_1000.bestmac_addr[2] = G_ptpClock.grandmasterIdentity[2];
	ARM_to_1000.bestmac_addr[3] = G_ptpClock.grandmasterIdentity[5];
	ARM_to_1000.bestmac_addr[4] = G_ptpClock.grandmasterIdentity[6];
	ARM_to_1000.bestmac_addr[5] = G_ptpClock.grandmasterIdentity[7];
	if(G_ptpClock.delayMechanism == E2E)
		tmp1 =  G_ptpClock.meanPathDelay.nanoseconds;	
	else if (G_ptpClock.delayMechanism == P2P)
		tmp1 = G_ptpClock.peerMeanPathDelay.nanoseconds;
	printf("%d：%d 闰秒:%d \n",ARM_to_1000.minute,ARM_to_1000.seconds,ARM_to_1000.leap);
	
	
	ARM_to_1000.path_delay[0] = tmp1>>24;
	ARM_to_1000.path_delay[1] = (tmp1&0xffffff)>>16;
	ARM_to_1000.path_delay[2] = (tmp1&0xffff)>>8;
	ARM_to_1000.path_delay[3] = tmp1&0xff;
	//ARM_to_FPGAM.correctionField = G_ptpClock.lastSyncCorrectionField.nanoseconds;
	 ARM_to_1000.onetwostep = G_ptpClock.twoStepFlag;
  //ARM_to_1000.correctionField =0x0;
	//tmp2=G_ptpClock.lastSyncCorrectionField.nanoseconds;
	ARM_to_1000.correctionField[0]  =0x0;
	ARM_to_1000.correctionField[1]  =0x0;
	ARM_to_1000.correctionField[2]  =0x0;
	ARM_to_1000.correctionField[3]  =0x0;
	cksum2=cal_crc_table((uint8_t *)&ARM_to_1000.year1,25) ;
	//printf("ck:%x\n",cksum2);
	ARM_to_1000.chksum=cksum2;				 //校验和
	//ARM_to_1000.chksum=0xAA;				 //校验和
	//UART6Send((uint8_t *)&ARM_to_1000,sizeof(ARMtoFPGA_PTP));//
	
	UART6Send((uint8_t *)&ARM_to_1000,30);//
}

unsigned int checksum_8(unsigned int cksum, void *pBuffer, unsigned int size)  
{  
    char num = 0;  
    unsigned char *p = (unsigned char *)pBuffer;  
   
    if ((NULL == pBuffer) || (0 == size))  
    {  
        return cksum;  
    }  
     
    while (size > 1)  
    {  
        cksum += (((unsigned short)p[num] << 8 & 0xff00) | (unsigned short)p[num + 1]) & 0x00FF;  
        size  -= 2;  
        num   += 2;  
    }  
     
    if (size > 0)  
    {  
        cksum += ((unsigned short)p[num] << 8) & 0xFFFF;  
        num += 1;  
    }  
   
    while (cksum >> 16)  
    {  
        cksum = (cksum & 0xFFFF) + (cksum >> 16);  
    }  
     
    return ~cksum;  
}  
