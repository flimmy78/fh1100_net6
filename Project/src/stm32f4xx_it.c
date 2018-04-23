/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @author  MCD Application Team
  * @version V1.1.0
  * @date    31-July-2013
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "main.h"
#include "udp_echoclient.h"
#include "stm32f4x7_eth.h"
#include "netconf.h"
#include "ptpd.h"
#include <string.h>
#include "ethernetif.h"
#include "share.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern __IO uint8_t EthLinkStatus;
extern sysTime sTime;
extern char ntpsendf;
u32 TIM4CH1_CAPTURE_VAL1;
u32 TIM4CH1_CAPTURE_VAL2; //处理PPS时定时器所记的数
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)//godin 每隔10ms中断一次的systick
{
  /* Update the LocalTime by adding SYSTEMTICK_PERIOD_MS each SysTick interrupt */
  Time_Update();
}

//接收到pps过后取出时间
void EXTI1_IRQHandler(void)//godin ptp接收时间pps
{	
	
	if(EXTI_GetITStatus(EXTI_Line1) == SET)//PPS_IN脉冲中断
	{
		TimeInternal 	NowTime;
		getTime(&NowTime);		//取MAC-1588硬件时间

		sTime.ppsTime.nanoseconds = NowTime.nanoseconds -880;
		sTime.ppsTime.seconds = NowTime.seconds;
		
		ntpsendf = 1;
		//printf("PPS arrive time = %ds.%dns\n",NowTime.seconds,NowTime.nanoseconds);
		//TIM_Cmd(TIM4,ENABLE );
		//TIM_SetCounter(TIM4,0);
		GPIO_SetBits(GPIOD,mLED3);//点亮LED3
		EXTI_ClearITPendingBit(EXTI_Line1);	
	}
}

/**
  * @brief  This function handles EXTI15_10.
  * @param  None
  * @retval None
  */
void EXTI15_10_IRQHandler(void)
{
//  if(EXTI_GetITStatus(EXTI_Line15) != RESET)
//  {
//    if (EthLinkStatus == 0)
//    {
//      /* Connect to tcp server */ 
//      udp_echoclient_connect();
//    }
//    /* Clear the EXTI line  pending bit */
//    EXTI_ClearITPendingBit(KEY_BUTTON_EXTI_LINE);
//  }
//  if(EXTI_GetITStatus(ETH_LINK_EXTI_LINE) != RESET)
//  {
//    Eth_Link_ITHandler(DP83640_PHY_ADDRESS);
//    /* Clear interrupt pending bit */
//    EXTI_ClearITPendingBit(ETH_LINK_EXTI_LINE);
//  }  
}

	/**
  * @brief  This function handles ETH interrupt request.
  * @param  None
  * @retval None
  */
void ETH_IRQHandler(void)
{
	if (ETH_GetDMAFlagStatus(ETH_DMA_FLAG_R) == SET) 
  {
  	while(ETH_GetRxPktSize() != 0) //从DMA中获取帧长度
			{		
				LwIP_Pkt_Handle();
			}
  	ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
  	ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS);
  }
}



/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f4xx.s).                                               */
/******************************************************************************/
/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
