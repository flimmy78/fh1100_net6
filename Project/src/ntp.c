#include "ntp.h"
#include "main.h"
#include "share.h"
extern FigStructData GlobalConfig;

extern unsigned char leap59; 
extern unsigned char leap61;
extern unsigned char leapwarning;
extern unsigned char leapflag;
extern unsigned char synflags;//已经同步标记
char ntpsendf = 0;

unsigned int NTPFRAC(unsigned int x)  
{	
	return (4294 * (x) +  ((1981 * (x)) >> 11) + ((2911*(x))>>28));	//更精确
}

void getNtpTime( ntp_time* t_ntpTime)
{	
	TimeInternal t_psRxTime;
	getTime(&t_psRxTime);
	t_ntpTime->seconds = t_psRxTime.seconds + JAN_1970;
	t_ntpTime->fraction = NTPFRAC( t_psRxTime.nanoseconds /1000);
}


#ifdef NTP_Client
extern unsigned int test_fjm;
struct udp_pcb *ntpc_pcb;
ntp_msg send_pack;//客服端发包的结构体变量
ntp_msg revNtpData;
TimeData send_ntp_inf;
ntp_time T1_Time;//NTP客服端发送时间T1
ntp_time Org_Time;//客服端发送源时间
unsigned int SendNum = 0;
ntp_time NNTime;

static void client_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
               struct ip_addr *addr, u16_t port)
 {   
	ntp_time rx_time;
	
	getNtpTime(&rx_time);//收到NTP服务器返回的数据包记录时间
	printf("client recv %d!!\n",rx_time.seconds%60);
  if(p->len != NTP_PCK_LEN)
  {
    pbuf_free(p);
	 	return;
  }

	MEMCPY(&revNtpData, p->payload, NTP_PCK_LEN);
	pbuf_free(p);
	
	if( ((revNtpData.status & LI_ALARM) == LI_ALARM )||
		(revNtpData.stratum ==0 )||(revNtpData.stratum > NTP_MAXSTRATUM))
		{
			//return;
		} 	
	memset(&send_ntp_inf,0,sizeof(send_ntp_inf)); 
#if 0//godin test must be use local temp time;t1时间应该用本地保存的时间而不是用接收到的服务器回复的orgtime
  if(test_fjm == Test_LOC_FERID)
		{
			send_ntp_inf.T1secondsFrom1900 = T1_temp.seconds;
			send_ntp_inf.T1fraction 			 = T1_temp.fraction;
		}
	else
		{
			send_ntp_inf.T1secondsFrom1900 = (unsigned int)ntohl(revNtpData.orgtime.seconds) ;	
			send_ntp_inf.T1fraction        = (unsigned int)ntohl(revNtpData.orgtime.fraction);
		}
#endif	//end 0
	
  send_ntp_inf.T1secondsFrom1900  = T1_Time.seconds;
	send_ntp_inf.T1fraction 			  = T1_Time.fraction;
	send_ntp_inf.T2secondsFrom1900 	= (unsigned int)ntohl(revNtpData.rxtime.seconds);
	send_ntp_inf.T2fraction 				= (unsigned int)ntohl(revNtpData.rxtime.fraction);
	send_ntp_inf.T3secondsFrom1900 	= (unsigned int)ntohl(revNtpData.txtime.seconds) ;
	send_ntp_inf.T3fraction 				= (unsigned int)ntohl(revNtpData.txtime.fraction);
	send_ntp_inf.T4secondsFrom1900 	= rx_time.seconds;
	send_ntp_inf.T4fraction 				= rx_time.fraction;
	send_ntp_inf.fjm 								= revNtpData.refid;
	send_ntp_inf.CFL_leapflag 			= revNtpData.status;//这里把LI ,VN,MODE的信息都带了,
  SendNum++;
	send_ntp_inf.frame_head 	= 0xeb;
  send_ntp_inf.frame_head1 	= 0x90;
	send_ntp_inf.frame_head2	= 0xeb;
  send_ntp_inf.frame_head3	= 0x90;
  send_ntp_inf.data_type 		= 0x01;
	send_ntp_inf.updateNum 		= SendNum;
	send_ntp_inf.frame_end0 	= 0xef;
	send_ntp_inf.frame_end1		= 0x9f;
	send_ntp_inf.frame_end2		= 0xef;
	send_ntp_inf.frame_end3 	= 0x9f;
	if(synflags)                                       
	{
		UARTSend((unsigned char *)&send_ntp_inf,sizeof(send_ntp_inf));//发送到PC
	}	
}

void Ntp_ClientInit(unsigned int ip)
{
	struct ip_addr ipaddr;
	
  ipaddr.addr = htonl(ip);//主机字节序转换为网络字节序(由于串口传来的IP地址是低字节在前)	
	ntpc_pcb = udp_new();
	udp_bind(ntpc_pcb, IP_ADDR_ANY,NTP_PORT);//
	udp_connect(ntpc_pcb, &ipaddr, NTP_PORT);// 联接对端
	udp_recv(ntpc_pcb,client_recv, NULL);		//client_recv接收并处理服务器返回的数据包
	printf("ntp client init\n");
}

void NTPClientLoop(void)//NTP客服端循环发送到服务器
{	
	struct pbuf *psend = NULL;
	ntp_time NtpgetTime;
	memset(&send_pack,0,NTP_PCK_LEN);
	getNtpTime(&NtpgetTime);//避免偏差
	
	if(ntpsendf == 1 || abs(NtpgetTime.seconds - NNTime.seconds)>=1)//1s 避免偏差
	{
		ntpsendf = 0;
		NNTime.seconds 		= NtpgetTime.seconds;

		send_pack.status 	= DEFAULT_STATUS;
		send_pack.stratum = DEFAULT_STRATUM;//0
		send_pack.poll 		= DEFAULT_POLL;	
		send_pack.precision 	= DEFAULT_PREC;
		send_pack.rootdelay 	= DEFAULT_RTDELAY;
		send_pack.dispersion 	= DEFAULT_DISPER;
		send_pack.refid 	= test_fjm;
		
		psend = pbuf_alloc(PBUF_TRANSPORT, NTP_PCK_LEN, PBUF_RAM);
		if(psend == NULL)
		{
			pbuf_free(psend);
			return;
		}		
		getNtpTime(&Org_Time);	          
		T1_Time.seconds  = Org_Time.seconds;
		T1_Time.fraction = Org_Time.fraction;

		if(send_pack.refid != Test_LOC_FERID)//如果不带TSSM 就将T1的时间用当前发出的时间
		{
			send_pack.txtime.seconds = htonl(Org_Time.seconds);
			send_pack.txtime.fraction= htonl(Org_Time.fraction);
		}

		MEMCPY(psend->payload,&send_pack, NTP_PCK_LEN);
		udp_send(ntpc_pcb,psend);				
		pbuf_free(psend);
		
		printf("ntp Client sent %d\n",Org_Time.seconds%60);
	}
}

#endif //end NTP_Client



#ifdef NTP_Server
struct udp_pcb 	*ntps_pcb;
extern unsigned char leap61;
extern unsigned char leap59;
extern unsigned char leapwring;

//NTP服务器接收Ntp数据包处理函数
void server_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
    ip_addr_t *addr, u16_t port)
{
		struct pbuf *reply_pbuf = NULL;
		ntp_time rx_time, tx_time;
		ntp_msg query, reply;
		ip_addr_t Dstaddr;
		 
	  if(!synflags) 				//未同步
		{
			pbuf_free(p);
			return;
		}
		//当服务器收到消息时获取一次时间
		getNtpTime(&rx_time);
		if(p->len != NTP_PCK_LEN)//判断收到的数据长度是否符合
		{
			pbuf_free(p);
			return;
		}
		memset(&reply,0,NTP_PCK_LEN);//清空回复数据
		MEMCPY(&query,p->payload,NTP_PCK_LEN);//
		
		pbuf_free(p);
		
		if(leapwring)
		{
			reply.status  = LI_ALARM; //未同步
		}
		if(leap61)
		{
			reply.status = LI_PLUSSEC;
		}
		else if(leap59)
		{
			reply.status = LI_MINUSSEC;
		}
		else
			reply.status = LI_NOWARNING;//正常时间
		
		reply.status |= (query.status & VERSIONMASK);//将收到的query.statut字段赋给reply //版本号
	
		//send_pack.status = DEFAULT_STATUS;客服端发送的是0xdb未同步
		switch(query.status & MODEMASK)//判断发送query的主机所在状态
		{	
			case ACT_MOD:	
				reply.status |= PAS_MOD; 
				break;
			case CLIENT_MOD:
			case BDC_MOD: 
				if(GlobalConfig.ip_mode == 2) //0(单播) 1(组播) 2(广播)//godin 支持广播模式
					reply.status |=	BDC_MOD;//广播模式
				else
					reply.status |= SERVER_MOD;//客服端为3,服务器回送字段为4
				break;
			default:
				return;
		}

		//填充数据
		reply.stratum 	 = LOC_STRATUM;//时钟层 1
		reply.poll 			 = query.poll; //连续消息间的最大间隔
		reply.precision  = LOC_PRECISION;//本地时钟精度
		reply.rootdelay  = LOC_RTDELAY;
		reply.dispersion = LOC_DISPER;
		reply.refid 		 = LOC_FERID;		 //

		if(query.refid == Test_LOC_FERID)//如果请求包的refid是Test_LOC_FERID，回相同的
		{
			reply.refid = Test_LOC_FERID;
		}
		//T2
		reply.orgtime.seconds		= query.txtime.seconds; //NTP发送时间T2
		reply.orgtime.fraction	= query.txtime.fraction;//
		//T3
		reply.rxtime.seconds 		= htonl(rx_time.seconds) ;//query的接收时间T3
		reply.rxtime.fraction 	= htonl(rx_time.fraction);//
		
		reply_pbuf = pbuf_alloc(PBUF_TRANSPORT,NTP_PCK_LEN,PBUF_RAM);//申请一个reply_pbuf用于发送reply
		if(reply_pbuf == NULL) 
		{
			return;	
		}
		
		getNtpTime(&tx_time);//回复时间
		//T1填写的是T3
		reply.reftime.seconds		= htonl(tx_time.seconds);//回送数据包时间
    reply.reftime.fraction	= htonl(tx_time.fraction);
		//T4
		reply.txtime.seconds		= htonl(tx_time.seconds);//回送数据包时间代替
		reply.txtime.fraction		= htonl(tx_time.fraction);//
		
		MEMCPY(reply_pbuf->payload, &reply, NTP_PCK_LEN);
		
		if(GlobalConfig.ip_mode == 2)
		{
			Dstaddr.addr = htonl(0xffffffff); //广播
			udp_sendto(pcb, reply_pbuf, &Dstaddr, port);
		}
		else
			udp_sendto(pcb, reply_pbuf, addr, port);//谁发给我回送给谁
		
		pbuf_free(reply_pbuf);
		//printf("ntp server send time %d\n",tx_time.seconds%60);
}

void NTP_ServerInit(void)
{
	ntps_pcb = udp_new();
	udp_bind(ntps_pcb,IP_ADDR_ANY,NTP_PORT);
	udp_recv(ntps_pcb, server_recv, NULL);
	printf("ntp Server init\n");
}	
#endif	//end NTP_Server


//初始化NTP服务器
void NTP_Init(void)
{
#ifdef NTP_Server   //godin ntp服务器开启
	NTP_ServerInit();
#elif defined(NTP_Client)
	Ntp_ClientInit(GlobalConfig.DstIpaddr);
#endif
	printf("---------NTP init finish---------\r\n");
}

