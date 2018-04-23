#include "ptpd.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/udp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "ethernetif.h"
#include "main.h"
#include "netconf.h"
#include <stdio.h>
#include "share.h"


extern FigStructData GlobalConfig;
struct netif gnetif;
extern struct ip_addr ipaddr1;
extern struct ip_addr netmask1;
extern struct ip_addr gw1;

uint32_t TCPTimer = 0;
uint32_t ARPTimer = 0;
uint32_t IPaddress = 0;

#ifdef USE_DHCP
uint32_t DHCPfineTimer = 0;
uint32_t DHCPcoarseTimer = 0;
__IO uint8_t DHCP_state;
#endif

extern __IO uint32_t  EthStatus;		//网口状态
extern struct ADDressInfo addrInfo;
void LwIP_DHCP_Process_Handle(void);



void LwIP_Init(void)
{
  struct ip_addr ipaddr;
  struct ip_addr netmask;
  struct ip_addr gw;
	uint8_t Raad_MACddr[6];
	
  mem_init();									//内存块初始化
  memp_init();								//内存管理表初始化
	Set_MAC_Address(GlobalConfig.MACaddr);
#ifdef USE_DHCP 							//DHCP自动分配地址
  ipaddr.addr = 0;
  netmask.addr = 0;
  gw.addr = 0;
#else													//不自动分，从参数配置读IP信息
	ipaddr.addr 	= htonl(GlobalConfig.IPaddr);
	netmask.addr 	= htonl(GlobalConfig.NETmark);
	gw.addr 			= htonl(GlobalConfig.GWaddr);
	printf("IP地址  :%d.%d.%d.%d\r\n",(uint32_t)((ipaddr.addr&0x000000ff)>>0),(uint32_t)((ipaddr.addr&0x0000ff00)>>8),
																								(uint32_t)((ipaddr.addr&0x00ff0000)>>16),(uint32_t)((ipaddr.addr&0xff000000)>>24));
	printf("子网掩码:%d.%d.%d.%d\r\n",(uint32_t)((netmask.addr&0x000000ff)>>0),(uint32_t)((netmask.addr&0x0000ff00)>>8),
																								(uint32_t)((netmask.addr&0x00ff0000)>>16),(uint32_t)((netmask.addr&0xff000000)>>24));
	printf("网关地址:%d.%d.%d.%d\r\n",(uint32_t)((netmask.addr&0x000000ff)>>0),(uint32_t)((netmask.addr&0x0000ff00)>>8),
																								(uint32_t)((netmask.addr&0x00ff0000)>>16),(uint32_t)((netmask.addr&0xff000000)>>24));
#endif  

  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);	//初始化变量网口gnetif
	//其中比较重要的两个参数是 ethernetif_init和 ethernet_input，
	//前者是用户自己定义的底层接口初始化函数， ethernet_input 函数是向 IP 层递交数据包的函数
  netif_set_default(&gnetif);												//指定gnetif为Lwip的网络接口
  netif_set_up(&gnetif);														//使能网络接口
	ETH_GetMACAddress(ETH_MAC_Address0,Raad_MACddr);				//尝试从网口读出MAC地址
	printf("MAC地址 :%02X.%02X.%02X.%02X.%02X.%02X\n",Raad_MACddr[0],Raad_MACddr[1],Raad_MACddr[2],Raad_MACddr[3],Raad_MACddr[4],Raad_MACddr[5]);
}


/**
* @brief  Called when a frame is received
* @param  None
* @retval None
*/
void LwIP_Pkt_Handle(void)	//网口中断调用函数
{
  ethernetif_input(&gnetif);
}

/**
* @brief  LwIP periodic tasks
* @param  localtime the current LocalTime value
* @retval None
*/
void LwIP_Periodic_Handle(__IO uint32_t localtime)	//Lwip周期调用的函数
{
#if LWIP_TCP
  if (localtime - TCPTimer >= TCP_TMR_INTERVAL)		//每250ms调用一次tcp_tmr()函数
		{
			TCPTimer =  localtime;
			tcp_tmr();
		}
#endif
  if ((localtime - ARPTimer) >= ARP_TMR_INTERVAL)	//ARP每5s周期性调用一次
		{
			ARPTimer =  localtime;
			etharp_tmr();
		}
#ifdef USE_DHCP							//如果使用DHCP的话
  if (localtime - DHCPfineTimer >= DHCP_FINE_TIMER_MSECS)	
		{
			DHCPfineTimer =  localtime;
			dhcp_fine_tmr();				//每500ms调用一次dhcp_fine_tmr()
			if ((DHCP_state != DHCP_ADDRESS_ASSIGNED) && 
					(DHCP_state != DHCP_TIMEOUT) &&
						(DHCP_state != DHCP_LINK_DOWN))
				{
					LwIP_DHCP_Process_Handle();
				}
		}
  //每60s执行一次DHCP粗糙处理
  if (localtime - DHCPcoarseTimer >= DHCP_COARSE_TIMER_MSECS)
		{
			DHCPcoarseTimer =  localtime;
			dhcp_coarse_tmr();
		}
#endif
}

#ifdef USE_DHCP
/**
* @brief  LwIP_DHCP_Process_Handle
* @param  None
* @retval None
*/
void LwIP_DHCP_Process_Handle()	//DHCP处理函数
{
  struct ip_addr ipaddr;
  struct ip_addr netmask;
  struct ip_addr gw;
	uint8_t	MAX_DHCP_TRIES	= 4;
  uint8_t iptab[4] = {0};
  uint8_t iptxt[20];
  switch (DHCP_state)
  {
		case DHCP_START:											//开启DHCP状态
				{
					DHCP_state = DHCP_WAIT_ADDRESS;	//状态切换到DHCP_WAIT_ADDRESS
					dhcp_start(&gnetif);
					IPaddress = 0;
				}
				break;
		case DHCP_WAIT_ADDRESS:
				IPaddress = gnetif.ip_addr.addr;	//读取新IP地址
				if (IPaddress!=0) 								//正确获取到IP地址的时候
					{
						DHCP_state = DHCP_ADDRESS_ASSIGNED;		//DHCP成功
						dhcp_stop(&gnetif);
					}
				else
					{
						if (gnetif.dhcp->tries > MAX_DHCP_TRIES)	//通过DHCP服务获取IP地址失败,且超过最大尝试次数4
							{
								DHCP_state = DHCP_TIMEOUT;
								dhcp_stop(&gnetif);										//DHCP超时失败,使用静态IP地址
								IP4_ADDR(&ipaddr, IP_ADDR0 ,IP_ADDR1 , IP_ADDR2 , IP_ADDR3 );
								IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
								IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
								netif_set_addr(&gnetif, &ipaddr , &netmask, &gw);
							}
					}
				break;
		default: 
				break;
  }
}
#endif


