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
	subTime(&(calcTime->SubTime),&(calcTime->ppsTime),&(calcTime->serailTime));//先计算时间调整值
	offnansec	= abs(calcTime->SubTime.nanoseconds);
	if(b <= 7) 					
		{
			if(i <= 10) 		//稳态还未建立
				{
					if(offnansec < 300)
						i++;	
					else if(i>0)
						i--;
				}
			else 						//稳态已经建立
				{	
					if(calcTime->SubTime.seconds)	
						{
							b+=2;		//出现秒级波动的次数，不稳定计数+2
							calcTime->SubTime.seconds = 0;		 //忽略此次秒不连续
							calcTime->SubTime.nanoseconds = 0; 
						}
					else if((offnansec<500)&&(b>0))
						{
							b--;
						}
					else if(offnansec > 5000	)
						{
							b++; 		//出现大波动，不稳定计数+1
							calcTime->SubTime.nanoseconds = 0; //忽略此次偶然的波动
						}	
				}
		}
	else  							//不稳定计数>7,稳态已经破坏
		{
			i = 0;
			b = 0;
		}
		//printf("offnsec=%d, i=%d, b=%d\n",offnansec,i,b);
	if(!calcTime->SubTime.seconds)//非秒级的的时间调整值，进过滤器
		{
			Mfilter(&calcTime->SubTime.nanoseconds,&MofM_filt);	
		}	
}

void abjClock(const TimeInternal subtime)	//同步装置调整时间
{
	Integer32 adj;
	TimeInternal timeTmp;
	Integer32  	 offsetNorm;
	static	uint8_t	objnum=0;
//	static int32_t maxadj = ADJ_FREQ_MAX;
//	static int32_t minadj = -ADJ_FREQ_MAX;
	DBGV("updateClock\n");                    
	if (0 != subtime.seconds || abs(subtime.nanoseconds) > MAX_ADJ_OFFSET_NS )//大于100ms的粗调大时间
		{
			if (!sTime.noAdjust) 
				{		
					if(!sTime.noResetClock)
						{
							//printf("subtime.seconds>1 or subtime.nanoseconds >100000000");
							getTime(&timeTmp);												//取硬件时间
							subTime(&timeTmp, &timeTmp, &subtime);		//加上系统时间秒部分的偏移值
							setTime(&timeTmp);												//设置硬件时间
							//2017.06.05粗调时间清时间集参数，加快频率收敛
//							sTime.observedDrift = 0;									//累计偏移器清0
//							MofM_filt.n = 0;													//数字滤波配置清0
//							MofM_filt.s = 0;
						}
				}
		}
	else	//细调时间
		{
			offsetNorm = subtime.nanoseconds;
			if(abs(offsetNorm) < 300)	//时间偏差绝对小于300
				{				
					if(synflags==0) 				//未同步
						{	
							if(GlobalConfig.WorkMode<=7)//PTP
								{
									if(objnum>32)
										{
											synflags    = 1;					//同步标志
											GPIO_SetBits(GPIOD,mLED4);//亮LED4，点亮锁定灯
											printf("精确同步完成...\n");
									  }
									else if(objnum==10)
										{
											Mservo.ai	  = 32;	
											Mservo.ap		= 4;
											printf("调整到精确算法...\n");
										}	
								}
							else //NTP
								{
									if(objnum>5)
									{
										synflags    = 1;					//同步标志
										GPIO_SetBits(GPIOD,mLED4);//亮LED4，点亮锁定灯
										printf("快速同步完成...\n");
									}
								}
							objnum++;	//累加
								
						}
					GPIO_SetBits(GPIOD,mLED2);//亮LED2，点亮PPS灯
				}
			else
				{
					GPIO_ResetBits(GPIOD,mLED2);	//偏差超300，灭同步灯
				}
			sTime.observedDrift += offsetNorm/Mservo.ai;			//累计偏移器算法
			if (sTime.observedDrift > ADJ_FREQ_MAX)						//超过极限值后，只在算法中引用极限值	
				sTime.observedDrift = ADJ_FREQ_MAX;
			else if (sTime.observedDrift < -ADJ_FREQ_MAX)
				sTime.observedDrift = -ADJ_FREQ_MAX;
			adj = offsetNorm/Mservo.ap + sTime.observedDrift;	//算法
			//myprintf("  adj:%d= off:( %d ) / %d + %d ( %d / %d ) \n",adj,offsetNorm,Mservo.ap,sTime.observedDrift,offsetNorm,Mservo.ai);
			adjFreq(-adj);																		//调整硬件加数寄存器
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
		//第一次的时候初始化y_prev 和 y_sum参数
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



unsigned long  Serial_Htime(tTime *sulocaltime)	//串口的大时间转UTC时间
{
	tTime localtime;
	unsigned long ulTime;
	localtime.usYear = sulocaltime->usYear;
	localtime.ucMon  = sulocaltime->ucMon;	//此处月的值为1－12
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
	ulTime = TimeToSeconds(&localtime);		//调用大时间转秒函数
	return ulTime;
}


void ulocaltime(unsigned long ulTime, tTime *psTime)//秒转大时间
{
	unsigned long ulTemp, ulMonths;
	ulTemp = ulTime / 60; 	
	psTime->ucSec = ulTime - (ulTemp * 60);	//秒
	ulTime = ulTemp;												
	ulTemp = ulTime / 60;
	psTime->ucMin = ulTime - (ulTemp * 60);	//分
	ulTime = ulTemp;		
	ulTemp = ulTime / 24;
	psTime->ucHour = ulTime - (ulTemp * 24);//小时
	ulTime = ulTemp;
	psTime->ucWday = (ulTime + 4) % 7;			//周
	ulTime += 366 + 365;
	ulTemp = ulTime / ((4 * 365) + 1);		
	if((ulTime - (ulTemp * ((4 * 365) + 1))) > (31 + 28))
		{
			ulTemp++;
			ulMonths = 12;											//=12月的情况	
		}
	else
		{
			ulMonths = 2;												//=2的情况	
		}
	psTime->usYear = ((ulTime - ulTemp) / 365) + 1968;	//年
	ulTime -= ((psTime->usYear - 1968) * 365) + ulTemp;	
	for(ulTemp = 0; ulTemp < ulMonths; ulTemp++)
		{
			if(g_psDaysToMonth[ulTemp] > ulTime)
				{
					break;
				}
		}
	psTime->ucMon = ulTemp - 1;							//月，注意：0-11对1-12月
	psTime->ucMday = ulTime - g_psDaysToMonth[ulTemp - 1] + 1; //日
}


unsigned long TimeToSeconds( tTime *psTime )			//大时间转秒
{
	unsigned long ulTime,tmp,tmp1,leapdaytosec,yeartosec,monthtosec,daytosec,hourtosec,mintosec;
	tmp = psTime->usYear - 1970;
	yeartosec = tmp * 365 *24 * 3600;								//年转总秒
	tmp = psTime->ucMon-1;								
	monthtosec = g_psDaysToMonth[tmp] * 24 * 3600;	//月转总秒
	tmp = psTime->ucMday;
	daytosec = (tmp-1) * 24 * 3600;									//天转总秒
	tmp = psTime->ucHour;
	hourtosec = tmp * 3600;													//时转总秒
	tmp = psTime->ucMin * 60;
	mintosec = tmp;																	//分转总秒	
	for(tmp=0,tmp1=1970; tmp1 <= (psTime->usYear); tmp1++)	//求1970到现在的瑞年的天数
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
	if(psTime->ucMon <= 2)																	//如果本年闰年，但是还未到3月1日
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
	leapdaytosec = tmp * 24 * 3600;										//闰年天数转秒
	ulTime = yeartosec + monthtosec + daytosec + hourtosec + mintosec + psTime->ucSec + leapdaytosec;
	//myprintf("%d-%d-%d,%d:%d:%d,leaps= %d,ulTime= %d\n", psTime->usYear,psTime->ucMon,psTime->ucMday,psTime->ucHour,psTime->ucMin,psTime->ucSec,leapdaytosec,ulTime);
	return ulTime;		//总秒数
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

