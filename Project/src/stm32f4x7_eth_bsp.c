#include "lwip/opt.h"
#include "stm32f4x7_eth.h"
#include "stm32f4x7_eth_bsp.h"
#include "main.h"
#include "netif.h"
#include "netconf.h"
#include "lwip/dhcp.h"

#define RMII_MODE  // User have to provide the 50 MHz clock by soldering a 50 MHz
                     // oscillator (ref SM7745HEV-50.0M or equivalent) on the U3
                     // footprint located under CN3 and also removing jumper on JP5.
                     // This oscillator is not provided with the board. 
                     // For more details, please refer to STM3240G-EVAL evaluation
                     // board User manual (UM1461).

//#define MII_MODE
#define PHY_CLOCK_MCO

/* Uncomment the define below to clock the PHY from external 25MHz crystal (only for MII mode) */
//#ifdef 	MII_MODE
// #define PHY_CLOCK_MCO
//#endif

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
ETH_InitTypeDef ETH_InitStructure;
__IO uint32_t  EthStatus = 0;
__IO uint8_t EthLinkStatus = 0;
extern struct netif gnetif;
#ifdef USE_DHCP
extern __IO uint8_t DHCP_state;
#endif /* LWIP_DHCP */


/**
  * @brief  Configures RCC
  * @param  None
  * @retval None
  */
void RCC_Configuration(void)
{
	RCC_ClocksTypeDef RCC_Clocks;

	/* Configure Systick clock source as HCLK */
  SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);

  /* SystTick configuration: an interrupt every 10ms */
  RCC_GetClocksFreq(&RCC_Clocks);	
  SysTick_Config(RCC_Clocks.HCLK_Frequency / 100);

  /* Set Systick interrupt priority to 0*/
  NVIC_SetPriority (SysTick_IRQn, 0);
  	
	/*Enable USART2 and TIM2 clock*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2| RCC_APB1Periph_TIM2, ENABLE);
	
	/* Enable ETHERNET clock  */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_ETH_MAC | RCC_AHB1Periph_ETH_MAC_Tx |
                        RCC_AHB1Periph_ETH_MAC_Rx, ENABLE);
	
	
	/* Enable GPIOs clocks */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB |
                         RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOI |
                         RCC_AHB1Periph_GPIOG | RCC_AHB1Periph_GPIOH |
                         RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOD , ENABLE);
	/*Enable SYSCFG clock*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	
	/* Enable ADC1 clocks */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
}

void NVIC_Configuration(void)
{
  NVIC_InitTypeDef   NVIC_InitStructure;
  NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组 2; 
	
  NVIC_InitStructure.NVIC_IRQChannel = ETH_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure); 
	
	NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;    
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;            
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	if((PTP_MS_MODE==0)||(PTP_MS_MODE==1))
	{
		NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;//优先级稍低
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStructure);
		
		NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn; //优先级最高
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStructure);
		myprintf("Usart1,PPS1,Init\n");
	}
}  

void GPIO_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE);

	//PE用作地址码
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 |
								  GPIO_Pin_2 | GPIO_Pin_3 |
								  GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOE, &GPIO_InitStructure);	

	/*Configure PD for LED*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12|
								 GPIO_Pin_13	| GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/*Configure PPS_IN (PD0)*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	
}

void UART1_Configuration(void)
{
//	USART_ClockInitTypeDef USART_ClockInitStruct;
	USART_InitTypeDef USART_InitStruct;
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	
	USART_DeInit(USART1);//串口复位
	//管脚映射
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource9,GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource10,GPIO_AF_USART1);
	
		//PA9->Tx PA10->Rx
	/* Configure USART Tx as alternate function  */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 ;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Configure USART Rx as alternate function  */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	USART_InitStruct.USART_BaudRate = 9600;
  USART_InitStruct.USART_WordLength = USART_WordLength_8b;//字长8位
  USART_InitStruct.USART_StopBits = USART_StopBits_1;//1位停止位
  USART_InitStruct.USART_Parity = USART_Parity_Even ;//偶校验位
  USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;//打开发送Tx和接收Rx
  USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //无流控
	
	USART_Init(USART1,&USART_InitStruct);
	USART_ITConfig(USART1,USART_IT_RXNE, ENABLE);
	USART_Cmd(USART1,ENABLE);//开启串口
}

void UART6_Configuration(void)
{
//	USART_ClockInitTypeDef USART_ClockInitStruct;
	USART_InitTypeDef USART_InitStruct;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART6, ENABLE);
	
	USART_DeInit(USART6);
	//管脚映射
	GPIO_PinAFConfig(GPIOC,GPIO_PinSource6,GPIO_AF_USART6);
	GPIO_PinAFConfig(GPIOC,GPIO_PinSource7,GPIO_AF_USART6);
	//PC6->Tx PC7->Rx
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 ;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* Configure USART Rx as alternate function  */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

//#ifdef Oscil_Device  //录波器串口波特率9600
	USART_InitStruct.USART_BaudRate = 9600;
//#else
//	USART_InitStruct.USART_BaudRate = 115200;
//#endif

  USART_InitStruct.USART_WordLength = USART_WordLength_8b;//字长8位
  USART_InitStruct.USART_StopBits = USART_StopBits_1;//1位停止位
  USART_InitStruct.USART_Parity = USART_Parity_No ;//无奇偶校验位
  USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;//打开发送Tx和接收Rx
  USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //无流控
	
	USART_Init(USART6,&USART_InitStruct);
	USART_ITConfig(USART6,USART_IT_RXNE, ENABLE);
	USART_Cmd(USART6,ENABLE);//开启串口
}

//配置EXTI的工作方式
void EXTI_Configuration(void)
{
	EXTI_InitTypeDef EXTI_InitStruct;
	
	/*Enable SYSCFG clock*/
	//系统配置控制器
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	
	EXTI_ClearITPendingBit(EXTI_Line1);//清除中断挂起位
	//Configure GPIO for EXTI1 line
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOD, EXTI_PinSource1);//做主时钟时PPS输入中断
	EXTI_InitStruct.EXTI_Line = EXTI_Line1;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;//EXTI_Mode_Event事件
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;//EXTI_Trigger_Falling下降沿,
	//EXTI_Trigger_Rising上升沿,EXTI_Trigger_Rising_Falling都触发
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	
	EXTI_Init(&EXTI_InitStruct);
}

/**
  * @brief  Configure timers and SysTick
  * @param  None
  * @retval None
  */
//static void TIM_Configuration(void)
//{
//  RCC_ClocksTypeDef RCC_Clocks;
//  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
//  TIM_OCInitTypeDef TIM_OCInitStructure;
//  TIM_ICInitTypeDef TIM_ICInitStructure;

////  RCC_GetClocksFreq(&RCC_Clocks);

//  /* SystTick configuration: an interrupt every 10ms */
////  SysTick_Config(RCC_Clocks.SYSCLK_Frequency / 100);
//  /* Update the SysTick IRQ priority should be higher than the Ethernet IRQ */
//  /* The Localtime should be updated during the Ethernet packets processing */
// // NVIC_SetPriority (SysTick_IRQn, 1);  


//  /* This PTP trigger signal is connected to the TIM2 ITR1 input selectable
//   * by software. The connection is enabled through bit 29 in the AFIO_MAPR
//   * register.
//   */
//  //GPIO_PinRemapConfig(GPIO_Remap_TIM2ITR1_PTP_SOF, DISABLE);
// // TIM2->OR |= TIM_OR_ITR1_RMP;
// TIM_RemapConfig( TIM2,  TIM2_ETH_PTP);
//  //TIM2->OR &= (uint16_t)~TIM_OR_ITR1_RMP;

//  /* TIM2 configuration: One Pulse mode */
//  /* length of the pulse in us */
//  TIM_TimeBaseStructure.TIM_Period = 10000;
//  /* prescaler of SysClk - 1 */
//  TIM_TimeBaseStructure.TIM_Prescaler = RCC_Clocks.SYSCLK_Frequency / 1000 - 1;
//  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
//  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

//  TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

//  TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
//  TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
//  /* Because of TIM_OCFast_Enable this value must be greater then 0,
//   * but then it does not generate any delay */
//  TIM_OCInitStructure.TIM_Pulse = 100;
//  TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;


//  /* Configure TIM2 channel 1 as output */
//  TIM_OC1Init(TIM2, &TIM_OCInitStructure);
//  TIM_OC1FastConfig(TIM2, TIM_OCFast_Enable);

//  TIM_ICStructInit(&TIM_ICInitStructure);

//  /* One Pulse Mode selection */
//  TIM_SelectOnePulseMode(TIM2, TIM_OPMode_Single);

//  /* Input Trigger selection */
//  TIM_SelectInputTrigger(TIM2, TIM_TS_ITR1);

//  /* Slave Mode selection: Trigger Mode */
//  TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_Trigger); 
//}

//static void WWDG_Configuration(void)
//{
//	NVIC_InitTypeDef NVIC_InitStructure;
//	RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, ENABLE);//窗口看门狗时钟
//	WWDG_SetPrescaler(WWDG_Prescaler_8);//
//	//我们的值要大于0x40，不然以运行就复位
//	
//	WWDG_SetWindowValue(0x41); //窗口看门狗的最大值是0x7f 递减,当0x40 到0x3f的时候会产生中断并复位
//	 /* Enable WWDG and set counter value to 0x7F,  0x7f-0x41=64 --WWDG timeout = ~8 ms * 64 = 512 ms */
//	WWDG_Enable(0x7f); //看门狗的最初递减值我们这里设为最大值
//	/* Clear EWI flag */
//	WWDG_ClearFlag(); 
//	
//	NVIC_InitStructure.NVIC_IRQChannel = WWDG_IRQn;
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
//	NVIC_Init(&NVIC_InitStructure);
//	
//	 /* Enable EW interrupt */
//	WWDG_EnableIT();
//}
/**
  * @brief  ETH_BSP_Config
  * @param  None
  * @retval None
  */

/**
  * @brief  Configures the Ethernet Interface
  * @param  None
  * @retval None
  */
void ETH_MACDMA_Config(void)
{
  /* Enable ETHERNET clock  */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_ETH_MAC | RCC_AHB1Periph_ETH_MAC_Tx |
                        RCC_AHB1Periph_ETH_MAC_Rx | RCC_AHB1Periph_ETH_MAC_PTP, ENABLE);// | RCC_AHB1Periph_ETH_MAC_PTP
                        
  /* Reset ETHERNET on AHB Bus */
  ETH_DeInit();

  /* Software reset */
  ETH_SoftwareReset();

  /* Wait for software reset */
  while (ETH_GetSoftwareResetStatus() == SET); //如果软件停在这里，一定是网卡没有配置好

  /* ETHERNET Configuration --------------------------------------------------*/
  /* Call ETH_StructInit if you don't like to configure all ETH_InitStructure parameter */
  ETH_StructInit(&ETH_InitStructure);

  /* Fill ETH_InitStructure parametrs */
  /*------------------------   MAC   -----------------------------------*/
  ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;											//开启网络自适应功能
//  ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Disable; 
//  ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
//  ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;   

  ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;													//关闭回送模式
  ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;								//关闭重传功能
  ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;					//关闭自动去除PDA/CRC功能
  ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;															//关闭接收所有的帧	
  ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;		//允许接收所有广播帧
  ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;										//关闭混合模式的地址过滤 
  ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_None;						//对于组播地址使用--->	无地址过滤  
	
//#if defined(NTP_Client) || defined(NTP_Server) /*开启组播地址完全过滤*/
//	ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
//#endif

  ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;						//对单播地址使用完美地址过滤 	
#ifdef CHECKSUM_BY_HARDWARE																																//定义CHECKSUM_BY_HARDWARE,使用硬件帧校验
  ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;											//开启ipv4和TCP/UDP/ICMP的帧校验和卸载  
#endif
	

  /*------------------------   DMA   -----------------------------------*/  

  /* When we use the Checksum offload feature, we need to enable the Store and Forward mode:
  the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum, 
  if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
	//当我们使用帧校验和卸载功能的时候，一定要使能存储转发模式,存储转发模式中要保证整个帧存储在FIFO中,
	//这样MAC能插入/识别出帧校验值,当帧校验正确的时候DMA就可以处理帧,否则就丢弃掉该帧
	
  ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable;	//开启丢弃TCP/IP错误帧
  ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;									//开启接收数据的存储转发模式  
  ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;								//开启发送数据的存储转发模式  

  ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;									//禁止转发错误帧	
  ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;//不转发过小的好帧
  ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Disable;									//开启的DMA处理第二帧功能，应该关闭？
  ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;									//开启DMA传输的地址对齐功能(LWIP中需开启，很重要)
  ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;																		//开启固定突发功能
  ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;												//DMA发送的最大突发长度为32个节拍 
  ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;												//DMA接收的最大突发长度为32个节拍	
  ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;							//DMA仲裁优先比率：Rx请求优先于Tx,优先比例为2：1

  EthStatus = ETH_Init(&ETH_InitStructure, DP83640_PHY_ADDRESS);															//配置ETH
	ETH_DMAITConfig(ETH_DMA_IT_NIS | ETH_DMA_IT_R, ENABLE);																			//使能以太网收中断	
}

/**
  * @brief  Configures the different GPIO ports.
  * @param  None
  * @retval None
  */
void ETH_GPIO_Config(void)
{
	volatile uint32_t i;
  GPIO_InitTypeDef GPIO_InitStructure;

  /* Enable GPIOs clocks */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOE, ENABLE);

  /* Enable SYSCFG clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	
  SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_ETH);

 /* Configure PB11 and PB12 PB13 */
  //PB11->TX_EN,PB12->TXD0,PB13->TXD1,PB5->PPS_OUT//godin pb5初始化后，开启ptp功能，自动出pps
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 ;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_ETH);	
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource12, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_ETH);
	
  /* Configure PC1, PC4 and PC5 */
  //PC1->MDC,PC4->RXD0,PC5->RXD1
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource4, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource5, GPIO_AF_ETH);
	
	//PA6 ETH_RST_N
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOA, GPIO_Pin_6);
	for (i = 0; i < 1000; i++);
	GPIO_SetBits(GPIOA,GPIO_Pin_6);//取消PHY复位管脚
	for (i = 0; i < 1000; i++);
}

/**
  * @brief  Configure the PHY to generate an interrupt on change of link status.
  * @param PHYAddress: external PHY address  
  * @retval None
  */
uint32_t Eth_Link_PHYITConfig(uint16_t PHYAddress)
{
  uint16_t tmpreg = 0;

  /* Read MICR register */
  tmpreg = ETH_ReadPHYRegister(PHYAddress, PHY_MICR);

  /* Enable output interrupt events to signal via the INT pin */
  tmpreg |= (uint16_t)(PHY_MICR_INT_EN | PHY_MICR_INT_OE);
  if(!(ETH_WritePHYRegister(PHYAddress, PHY_MICR, tmpreg)))
  {
    /* Return ERROR in case of write timeout */
    return ETH_ERROR;
  }

  /* Read MISR register */
  tmpreg = ETH_ReadPHYRegister(PHYAddress, PHY_MISR);

  /* Enable Interrupt on change of link status */
  tmpreg |= (uint16_t)PHY_MISR_LINK_INT_EN;
  if(!(ETH_WritePHYRegister(PHYAddress, PHY_MISR, tmpreg)))
  {
    /* Return ERROR in case of write timeout */
    return ETH_ERROR;
  }
  /* Return SUCCESS */
  return ETH_SUCCESS;   
}

/**
  * @brief  EXTI configuration for Ethernet link status.
  * @param PHYAddress: external PHY address  
  * @retval None
  */
void Eth_Link_EXTIConfig(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  EXTI_InitTypeDef EXTI_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  /* Enable the INT (PB14) Clock */
  RCC_AHB1PeriphClockCmd(ETH_LINK_GPIO_CLK, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

  /* Configure INT pin as input */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Pin = ETH_LINK_PIN;
  GPIO_Init(ETH_LINK_GPIO_PORT, &GPIO_InitStructure);

  /* Connect EXTI Line to INT Pin */
  SYSCFG_EXTILineConfig(ETH_LINK_EXTI_PORT_SOURCE, ETH_LINK_EXTI_PIN_SOURCE);

  /* Configure EXTI line */
  EXTI_InitStructure.EXTI_Line = ETH_LINK_EXTI_LINE;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStructure);

  /* Enable and set the EXTI interrupt to priority 1*/
  NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief  This function handles Ethernet link status.
  * @param  None
  * @retval None
  */
void Eth_Link_ITHandler(uint16_t PHYAddress)
{
  /* Check whether the link interrupt has occurred or not */
//  if(((ETH_ReadPHYRegister(PHYAddress, PHY_MISR)) & PHY_LINK_STATUS) != 0)
//  {
//    if((ETH_ReadPHYRegister(PHYAddress, PHY_SR) & 1))
//    {
//      netif_set_link_up(&gnetif);
//    }
//    else
//    {
//      EthLinkStatus = 1;
//      netif_set_link_down(&gnetif);
//    }
//  }
}

/**
  * @brief  Link callback function, this function is called on change of link status.
  * @param  The network interface
  * @retval None
  */
void ETH_link_callback(struct netif *netif)
{
  __IO uint32_t timeout = 0;
 uint32_t tmpreg,RegValue;
  struct ip_addr ipaddr;
  struct ip_addr netmask;
  struct ip_addr gw;
#ifndef USE_DHCP
	#define IP_ADDR0   192
	#define IP_ADDR1   168
	#define IP_ADDR2   1
	#define IP_ADDR3   18

	/*NETMASK*/
	#define NETMASK_ADDR0   255
	#define NETMASK_ADDR1   255
	#define NETMASK_ADDR2   255
	#define NETMASK_ADDR3   0

	/*Gateway Address*/
	#define GW_ADDR0   192
	#define GW_ADDR1   168
	#define GW_ADDR2   1
	#define GW_ADDR3   1
#endif /* USE_DHCP */



  if(netif_is_link_up(netif))
  {
    /* Restart the autonegotiation */
    if(ETH_InitStructure.ETH_AutoNegotiation != ETH_AutoNegotiation_Disable)
    {
      /* Reset Timeout counter */
      timeout = 0;

      /* Enable auto-negotiation */
      ETH_WritePHYRegister(DP83640_PHY_ADDRESS, PHY_BCR, PHY_AutoNegotiation);

      /* Wait until the auto-negotiation will be completed */
      do
      {
        timeout++;
      } while (!(ETH_ReadPHYRegister(DP83640_PHY_ADDRESS, PHY_BSR) & PHY_AutoNego_Complete) && (timeout < (uint32_t)PHY_READ_TO));  

      /* Reset Timeout counter */
      timeout = 0;

      /* Read the result of the auto-negotiation */
      RegValue = ETH_ReadPHYRegister(DP83640_PHY_ADDRESS, PHY_SR);
    
      /* Configure the MAC with the Duplex Mode fixed by the auto-negotiation process */
      if((RegValue & PHY_DUPLEX_STATUS) != (uint32_t)RESET)
      {
        /* Set Ethernet duplex mode to Full-duplex following the auto-negotiation */
        ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;  
      }
      else
      {
        /* Set Ethernet duplex mode to Half-duplex following the auto-negotiation */
        ETH_InitStructure.ETH_Mode = ETH_Mode_HalfDuplex;           
      }
      /* Configure the MAC with the speed fixed by the auto-negotiation process */
      if(RegValue & PHY_SPEED_STATUS)
      {
        /* Set Ethernet speed to 10M following the auto-negotiation */    
        ETH_InitStructure.ETH_Speed = ETH_Speed_10M; 
      }
      else
      {
        /* Set Ethernet speed to 100M following the auto-negotiation */ 
        ETH_InitStructure.ETH_Speed = ETH_Speed_100M;      
      }

      /*------------------------ ETHERNET MACCR Re-Configuration --------------------*/
      /* Get the ETHERNET MACCR value */  
      tmpreg = ETH->MACCR;

      /* Set the FES bit according to ETH_Speed value */ 
      /* Set the DM bit according to ETH_Mode value */ 
      tmpreg |= (uint32_t)(ETH_InitStructure.ETH_Speed | ETH_InitStructure.ETH_Mode);

      /* Write to ETHERNET MACCR */
      ETH->MACCR = (uint32_t)tmpreg;

      _eth_delay_(ETH_REG_WRITE_DELAY);
      tmpreg = ETH->MACCR;
      ETH->MACCR = tmpreg;
    }

    /* Restart MAC interface */
    ETH_Start();

#ifdef USE_DHCP
    ipaddr.addr = 0;
    netmask.addr = 0;
    gw.addr = 0;
    DHCP_state = DHCP_START;
#else
    IP4_ADDR(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
    IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
    IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
#endif /* USE_DHCP */
    netif_set_addr(&gnetif, &ipaddr , &netmask, &gw);
    /* When the netif is fully configured this function must be called.*/
    netif_set_up(&gnetif);    
#ifdef USE_LCD
    /* Set the LCD Text Color */
    LCD_SetTextColor(Green);

    /* Display message on the LCD */
    LCD_DisplayStringLine(Line5, (uint8_t*)"  Network Cable is  ");
    LCD_DisplayStringLine(Line6, (uint8_t*)"    now connected   ");

    /* Set the LCD Text Color */
    LCD_SetTextColor(White);

  #ifndef USE_DHCP
    /* Display static IP address */
    iptab[0] = IP_ADDR3;
    iptab[1] = IP_ADDR2;
    iptab[2] = IP_ADDR1;
    iptab[3] = IP_ADDR0;
    sprintf((char*)iptxt, "  %d.%d.%d.%d", iptab[3], iptab[2], iptab[1], iptab[0]);
    LCD_DisplayStringLine(Line8, (uint8_t*)"  Static IP address   ");
    LCD_DisplayStringLine(Line9, iptxt);

    /* Clear LCD */
    LCD_ClearLine(Line5);
    LCD_ClearLine(Line6);
  #endif /* USE_DHCP */
#endif /* USE_LCD */
    EthLinkStatus = 0;
  }
  else
  {
    ETH_Stop();
#ifdef USE_DHCP
    DHCP_state = DHCP_LINK_DOWN;
    dhcp_stop(netif);
#endif /* USE_DHCP */

    /*  When the netif link is down this function must be called.*/
    netif_set_down(&gnetif);
#ifdef USE_LCD
    /* Set the LCD Text Color */
    LCD_SetTextColor(Red);

    /* Display message on the LCD */
    LCD_DisplayStringLine(Line5, (uint8_t*)"  Network Cable is  ");
    LCD_DisplayStringLine(Line6, (uint8_t*)"     unplugged   ");

    /* Set the LCD Text Color */
    LCD_SetTextColor(White);
#endif /* USE_LCD */
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
