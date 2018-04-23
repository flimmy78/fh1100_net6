#include "share.h"
#include "serial_hand.h"
extern FigStructData GlobalConfig;
extern unsigned int test_fjm;
extern sysTime sTime;
extern Servo Mservo;
extern Filter MofM_filt;
static void Mfilter(Integer32 * nsec_current, Filter * filt);
uint8_t synflags=0;
uint8_t tbflags=0;
void offset_time(sysTime *calcTime)
{
	static  uint8_t b;			
	static  char i = 0;	
	uint32_t offnansec;	
	subTime(&(calcTime->SubTime),&(calcTime->ppsTime),&(calcTime->serailTime));//�ȼ���ʱ�����ֵ
	offnansec	= abs(calcTime->SubTime.nanoseconds);
	if(b <= 7) 					
		{
			if(i <= 10) 		//��̬��δ����
				{
					if(offnansec < 300)
						i++;	
					else if(i>0)
						i--;
				}
			else 						//��̬�Ѿ�����
				{	
					if(calcTime->SubTime.seconds)	
						{
							b+=2;		//�����뼶�����Ĵ��������ȶ�����+2
							calcTime->SubTime.seconds = 0;		 //���Դ˴��벻����
							calcTime->SubTime.nanoseconds = 0; 
						}
					else if((offnansec<500)&&(b>0))
						{
							b--;
						}
					else if(offnansec > 5000	)
						{
							b++; 		//���ִ󲨶������ȶ�����+1
							calcTime->SubTime.nanoseconds = 0; //���Դ˴�żȻ�Ĳ���
						}	
				}
		}
	else  							//���ȶ�����>7,��̬�Ѿ��ƻ�
		{
			i = 0;
			b = 0;
		}
		//printf("offnsec=%d, i=%d, b=%d\n",offnansec,i,b);
	if(!calcTime->SubTime.seconds)//���뼶�ĵ�ʱ�����ֵ����������
		{
			Mfilter(&calcTime->SubTime.nanoseconds,&MofM_filt);	
		}	
}

void abjClock(const TimeInternal subtime)	//ͬ��װ�õ���ʱ��
{
	Integer32 adj;
	TimeInternal timeTmp;
	Integer32  	 offsetNorm;
	static	uint8_t	objnum=0;
//	static int32_t maxadj = ADJ_FREQ_MAX;
//	static int32_t minadj = -ADJ_FREQ_MAX;
	DBGV("updateClock\n");                    
	if (0 != subtime.seconds || abs(subtime.nanoseconds) > MAX_ADJ_OFFSET_NS )//����100ms�Ĵֵ���ʱ��
		{
			if (!sTime.noAdjust) 
				{		
					if(!sTime.noResetClock)
						{
							//printf("subtime.seconds>1 or subtime.nanoseconds >100000000");
							getTime(&timeTmp);												//ȡӲ��ʱ��
							subTime(&timeTmp, &timeTmp, &subtime);		//����ϵͳʱ���벿�ֵ�ƫ��ֵ
							setTime(&timeTmp);												//����Ӳ��ʱ��
							//2017.06.05�ֵ�ʱ����ʱ�伯�������ӿ�Ƶ������
//							sTime.observedDrift = 0;									//�ۼ�ƫ������0
//							MofM_filt.n = 0;													//�����˲�������0
//							MofM_filt.s = 0;
						}
				}
		}
	else	//ϸ��ʱ��
		{
			offsetNorm = subtime.nanoseconds;
			if(abs(offsetNorm) < 300)	//ʱ��ƫ�����С��300
				{				
					if(synflags==0) 				//δͬ��
						{	
							if(GlobalConfig.WorkMode<=7)//PTP
								{
									if(objnum>32)
										{
											synflags    = 1;					//ͬ����־
											GPIO_SetBits(GPIOD,mLED4);//��LED4������������
											printf("��ȷͬ�����...\n");
									  }
									else if(objnum==10)
										{
											Mservo.ai	  = 32;	
											Mservo.ap		= 4;
											printf("��������ȷ�㷨...\n");
										}	
								}
							else //NTP
								{
									if(objnum>5)
									{
										synflags    = 1;					//ͬ����־
										GPIO_SetBits(GPIOD,mLED4);//��LED4������������
										printf("����ͬ�����...\n");
									}
								}
							objnum++;	//�ۼ�
								
						}
					GPIO_SetBits(GPIOD,mLED2);//��LED2������PPS��
				}
			else
				{
					GPIO_ResetBits(GPIOD,mLED2);	//ƫ�300����ͬ����
				}
			sTime.observedDrift += offsetNorm/Mservo.ai;			//�ۼ�ƫ�����㷨
			if (sTime.observedDrift > ADJ_FREQ_MAX)						//��������ֵ��ֻ���㷨�����ü���ֵ	
				sTime.observedDrift = ADJ_FREQ_MAX;
			else if (sTime.observedDrift < -ADJ_FREQ_MAX)
				sTime.observedDrift = -ADJ_FREQ_MAX;
			adj = offsetNorm/Mservo.ap + sTime.observedDrift;	//�㷨
			//myprintf("  adj:%d= off:( %d ) / %d + %d ( %d / %d ) \n",adj,offsetNorm,Mservo.ap,sTime.observedDrift,offsetNorm,Mservo.ai);
			adjFreq(-adj);																		//����Ӳ�������Ĵ���
		}
}



#ifdef NTP_Server
unsigned char databuffer1[44] = {0};
unsigned char databuffer_index = 0;
unsigned char datastart = 0;
unsigned char datagood = 0;
unsigned char dataudpsend = 0;
void *pcb_sendip; 
TimeInternal ntime;
unsigned char flaghunan = 0;;
#endif	//end NTP_Server

static Integer32 order(Integer32 n)
{
    if (n < 0) {
        n = -n;
    }
    else if (n == 0) {
        return 0;
    }
    return floorLog2(n);
}

/* exponencial smoothing */
static void Mfilter(Integer32 * nsec_current, Filter * filt)
{
    Integer32 s, s2;

    /*
			using floatingpoint math
			alpha = 1/2^s
			y[1] = x[0]
			y[n] = alpha * x[n-1] + (1-alpha) * y[n-1]
			
			or equivalent with integer math
			y[1] = x[0]
			y_sum[1] = y[1] * 2^s
			y_sum[n] = y_sum[n-1] + x[n-1] - y[n-1]
			y[n] = y_sum[n] / 2^s
    */
		//printf("filt->n=%d,filt->s=%d\n",filt->n,filt->s);
    filt->n++; /* increment number of samples */
    /* if it is first time, we are running filter, initialize it*/
		//��һ�ε�ʱ���ʼ��y_prev �� y_sum����
    if (filt->n == 1) 
			{
					filt->y_prev = *nsec_current;
					filt->y_sum = *nsec_current;
					filt->s_prev = 0;
			}
    s = filt->s;
    /* speedup filter, if not 2^s > n */
    if ((1<<s) > filt->n)			
			{
					/* lower the filter order */
				s = order(filt->n);
			} 
		else
			{
				/* avoid overflowing of n */
				filt->n = 1<<s;
			}
    /* avoid overflowing of filter. 30 is because using signed 32bit integers */
    s2 = 30 - order(max(filt->y_prev, *nsec_current));
    /* use the lower filter order, higher will overflow */
    s = min(s, s2);
    /* if the order of the filter changed, change also y_sum value */
    if(filt->s_prev > s) 
			{
				filt->y_sum >>= (filt->s_prev - s);
			} 
		else if (filt->s_prev < s) 
			{
				filt->y_sum <<= (s - filt->s_prev);
			}
    /* compute the filter itself */
    filt->y_sum += *nsec_current - filt->y_prev;
    filt->y_prev = filt->y_sum >> s;
    /* save previous order of the filter */
    filt->s_prev = s;
    DBGV("filter: %d -> %d (%d)\n", *nsec_current, filt->y_prev, s);
    /* actualize target value */
    *nsec_current = filt->y_prev;
}



unsigned long  Serial_Htime(tTime *sulocaltime)	//���ڵĴ�ʱ��תUTCʱ��
{
	tTime localtime;
	unsigned long ulTime;
	localtime.usYear = sulocaltime->usYear;
	localtime.ucMon  = sulocaltime->ucMon;	//�˴��µ�ֵΪ1��12
	localtime.ucMday = sulocaltime->ucMday;
	localtime.ucHour = sulocaltime->ucHour;
	if(localtime.ucHour>=8)
		{
		 localtime.ucHour=localtime.ucHour-8;
		}
	else
		{
			localtime.ucHour=localtime.ucHour+(24-8);
			if(localtime.ucMday>1)
				{
					localtime.ucMday=localtime.ucMday-1;
				}
			else
				{
					if(localtime.ucMon==1)
						{
							localtime.ucMon=12;
						}
					else
						{
							localtime.ucMon=localtime.ucMon-1;
						}
					switch(localtime.ucMon)
						{
							case 1:case 3:case 5:
							case 7:case 8:case 10:
									localtime.ucMday=31;
									break;
							case 4:	case 6:
							case 9:case 11:
									localtime.ucMday=30;
									break;
							case 2:
									if(((localtime.usYear+0x7d0)%4 == 0 && (localtime.usYear+0x7d0)%100 != 0) 
											|| (localtime.usYear+0x7d0)%400 == 0)
										localtime.ucMday=29;
									else
										localtime.ucMday=28;
									break;
							case 12:
									localtime.usYear-=1;
									localtime.ucMday=31;
									break;
						}
				 }
		}
	localtime.ucMin = sulocaltime->ucMin;
	localtime.ucSec = sulocaltime->ucSec;
	ulTime = TimeToSeconds(&localtime);		//���ô�ʱ��ת�뺯��
	return ulTime;
}


void ulocaltime(unsigned long ulTime, tTime *psTime)//��ת��ʱ��
{
	unsigned long ulTemp, ulMonths;
	ulTemp = ulTime / 60; 	
	psTime->ucSec = ulTime - (ulTemp * 60);	//��
	ulTime = ulTemp;												
	ulTemp = ulTime / 60;
	psTime->ucMin = ulTime - (ulTemp * 60);	//��
	ulTime = ulTemp;		
	ulTemp = ulTime / 24;
	psTime->ucHour = ulTime - (ulTemp * 24);//Сʱ
	ulTime = ulTemp;
	psTime->ucWday = (ulTime + 4) % 7;			//��
	ulTime += 366 + 365;
	ulTemp = ulTime / ((4 * 365) + 1);		
	if((ulTime - (ulTemp * ((4 * 365) + 1))) > (31 + 28))
		{
			ulTemp++;
			ulMonths = 12;											//=12�µ����	
		}
	else
		{
			ulMonths = 2;												//=2�����	
		}
	psTime->usYear = ((ulTime - ulTemp) / 365) + 1968;	//��
	ulTime -= ((psTime->usYear - 1968) * 365) + ulTemp;	
	for(ulTemp = 0; ulTemp < ulMonths; ulTemp++)
		{
			if(g_psDaysToMonth[ulTemp] > ulTime)
				{
					break;
				}
		}
	psTime->ucMon = ulTemp - 1;							//�£�ע�⣺0-11��1-12��
	psTime->ucMday = ulTime - g_psDaysToMonth[ulTemp - 1] + 1; //��
}


unsigned long TimeToSeconds( tTime *psTime )			//��ʱ��ת��
{
	unsigned long ulTime,tmp,tmp1,leapdaytosec,yeartosec,monthtosec,daytosec,hourtosec,mintosec;
	tmp = psTime->usYear - 1970;
	yeartosec = tmp * 365 *24 * 3600;								//��ת����
	tmp = psTime->ucMon-1;								
	monthtosec = g_psDaysToMonth[tmp] * 24 * 3600;	//��ת����
	tmp = psTime->ucMday;
	daytosec = (tmp-1) * 24 * 3600;									//��ת����
	tmp = psTime->ucHour;
	hourtosec = tmp * 3600;													//ʱת����
	tmp = psTime->ucMin * 60;
	mintosec = tmp;																	//��ת����	
	for(tmp=0,tmp1=1970; tmp1 <= (psTime->usYear); tmp1++)	//��1970�����ڵ����������
		{
			if((tmp1)%4 == 0)
				{
					if((tmp1%100) == 0)
						{
							if((tmp1%400) == 0)
								tmp++;
						}
					else
						tmp++;
				}
		}
	if(psTime->ucMon <= 2)																	//����������꣬���ǻ�δ��3��1��
		{
			if(psTime->usYear%4 == 0)
				{
					if(psTime->usYear %100 == 0)
						{
							if(psTime->usYear == 0)
								{
									tmp--;
								}
						}
					else
						tmp--;
				}
		}
	leapdaytosec = tmp * 24 * 3600;										//��������ת��
	ulTime = yeartosec + monthtosec + daytosec + hourtosec + mintosec + psTime->ucSec + leapdaytosec;
	//myprintf("%d-%d-%d,%d:%d:%d,leaps= %d,ulTime= %d\n", psTime->usYear,psTime->ucMon,psTime->ucMday,psTime->ucHour,psTime->ucMin,psTime->ucSec,leapdaytosec,ulTime);
	return ulTime;		//������
}



//void UARTSend(const unsigned char *pucBuffer, unsigned long ulCount)
//{
//	while(ulCount--)
//		{
//			USART_SendData(USART6,*pucBuffer++);
//			while(USART_GetFlagStatus(USART6, USART_FLAG_TXE) == RESET)
//			{}
//		}
//}

