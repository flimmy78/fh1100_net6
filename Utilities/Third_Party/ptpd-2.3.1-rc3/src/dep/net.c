/*-
 * Copyright (c) 2014	   Wojciech Owczarek,
 *			   George V. Neville-Neil
 * Copyright (c) 2012-2013 George V. Neville-Neil,
 *                         Wojciech Owczarek.
 * Copyright (c) 2011-2012 George V. Neville-Neil,
 *                         Steven Kreuzer, 
 *                         Martin Burnicki, 
 *                         Jan Breuer,
 *                         Gael Mace, 
 *                         Alexandre Van Kempen,
 *                         Inaqui Delgado,
 *                         Rick Ratzel,
 *                         National Instruments.
 * Copyright (c) 2009-2010 George V. Neville-Neil, 
 *                         Steven Kreuzer, 
 *                         Martin Burnicki, 
 *                         Jan Breuer,
 *                         Gael Mace, 
 *                         Alexandre Van Kempen
 *
 * Copyright (c) 2005-2008 Kendall Correll, Aidan Williams
 *
 * All Rights Reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file   net.c
 * @date   Tue Jul 20 16:17:49 2010
 *
 * @brief  Functions to interact with the network sockets and NIC driver.
 *
 *
 */

#include "../ptpd.h"


static Integer32 findIface(const Octet *ifaceName, Octet *uuid, NetPath *netPath)
{
    struct netif * iface;
    iface = netif_default;
    memcpy(uuid, iface->hwaddr, iface->hwaddr_len);
    return iface->ip_addr.addr;
}

static void netQInit(BufQueue *pQ)
{
    pQ->get = 0;
    pQ->put = 0;
    pQ->count = 0;
}

static Boolean netQPut(BufQueue *pQ, void *pbuf)
{
    if (pQ->count >= PBUF_QUEUE_SIZE)
        return FALSE;

    pQ->pbuf[pQ->put] = pbuf;

    pQ->put = (pQ->put + 1) % PBUF_QUEUE_SIZE;

    pQ->count++;

    return TRUE;
}

/**
  * @brief  Get data from the network queue
  * @param  pQ the queue to be used
  * @retval void* pointer to pbuf or NULL
  */
static void *netQGet(BufQueue *pQ)
{
    void *pbuf;
    if (!pQ->count)
        return NULL;

    pbuf = pQ->pbuf[pQ->get];
    pQ->get = (pQ->get + 1) % PBUF_QUEUE_SIZE;
    pQ->count--;
    return pbuf;
}



/**
  * @brief  Free all pbufs in the queue
  * @param  pQ the queue to be used
  * @retval None
  */
static void netQEmpty(BufQueue * pQ)
{
    struct pbuf * p;
    int cnt = pQ->count;

    for (;cnt > 0; cnt--)
    {
        p = (struct pbuf*)netQGet(pQ);

        if (p) pbuf_free(p);
    }
}

/**
  * @brief  Delete all waiting packets in Event queue
  * @param  netPath network object
  * @retval None
  */
void netEmptyEventQ(NetPath *netPath)
{
    netQEmpty(&netPath->eventQ);
}
/**
  * @brief  Check if something is in the queue
  * @param  pQ the queue to be used
  * @retval Boolean TRUE if success
  */
static Boolean netQCheck(BufQueue *pQ)
{
    if (!pQ->count)
        return FALSE;

    return TRUE;
}

/**
 * shutdown the multicast (both General and Peer)
 *
 * @param netPath 
 * 
 * @return TRUE if successful
 */
//static Boolean
//netShutdownMulticast(NetPath * netPath)
//{
//	/* Close General Multicast */
//	netShutdownMulticastIPv4(netPath, netPath->multicastAddr);
//	netPath->multicastAddr = 0;

//	/* Close Peer Multicast */
//	netShutdownMulticastIPv4(netPath, netPath->peerMulticastAddr);
//	netPath->peerMulticastAddr = 0;
//	
//	return TRUE;
//}

/*
 * For future use: Check if IPv4 address is multiast -
 * If last 4 bits of an address are 0xE (1110), it's multicast
 */
/*
static Boolean
isIpMulticast(struct in_addr in)
{
        if((ntohl(in.s_addr) >> 28) == 0x0E )
	    return TRUE;
	return FALSE;
}
*/

/* shut down the UDP stuff */
Boolean netShutdown(NetPath * netPath)
{
	struct ip_addr multicastAaddr;
	struct ip_addr interfaceAddr;
	DBG("network Shutdown\n");
	
	interfaceAddr.addr = findIface(NULL,NULL, netPath);
	
	/* leave multicast group */ 
  multicastAaddr.addr = netPath->multicastAddr;//退出组播
  igmp_leavegroup(&interfaceAddr, &multicastAaddr);
	
	multicastAaddr.addr = netPath->peerMulticastAddr;
	igmp_leavegroup(&interfaceAddr, &multicastAaddr);
	
	/* Disconnect and close the Event UDP interface */
	if (netPath->eventPcb)
	{
			udp_disconnect(netPath->eventPcb);
			udp_remove(netPath->eventPcb);
			netPath->eventPcb = NULL;
	}
	/* Disconnect and close the General UDP interface */
	if (netPath->generalPcb)
	{
			udp_disconnect(netPath->generalPcb);
			udp_remove(netPath->generalPcb);
			netPath->generalPcb = NULL;
	}
	
	/* Clear the network addresses. */
	netPath->unicastAddr = 0;//单播地址设为0
	netPath->multicastAddr = 0;
	netPath->peerMulticastAddr = 0;
	return TRUE;
}



/* Try getting hwAddrSize bytes of ifaceName hardware address,
   and place them in hwAddr. Return 1 on success, 0 when no suitable
   hw address available, -1 on failure.
 */
//static int
//getHwAddress (char* ifaceName, unsigned char* hwAddr, int hwAddrSize)
//{

//}

//static Boolean getInterfaceInfo(char* ifaceName, InterfaceInfo* ifaceInfo)
//{

//}


Boolean
hostLookup(const char* hostname, Integer32* addr)
{
	if (hostname[0]) {
//		/* Attempt a DNS lookup first. */
//		struct hostent *host;
//#ifdef HAVE_GETHOSTBYNAME2
//		host = gethostbyname2(hostname, AF_INET);
//#else
//		host = getipnodebyname(hostname, AF_INET, AI_DEFAULT, &errno);
//#endif /* HAVE_GETHOSTBYNAME2 */

//                if (host != NULL) {
//			if (host->h_length != 4) {
//				PERROR("unicast host resolved to non ipv4"
//				       "address");
//				return FALSE;
//			}
//			*addr = 
//				*(uint32_t *)host->h_addr_list[0];
//			return TRUE;
//		} else {
//			struct in_addr netAddr;
//			/* Maybe it's a dotted quad. */
//			if (!inet_aton(hostname, &netAddr)) {
//				ERROR("failed to encode unicast address: %s\n",
//				      hostname);
//				return FALSE;
//				*addr = netAddr.s_addr;
//				return TRUE;
//			}
            
	}

return FALSE;

}


/**
  * @brief  Processing an incoming message on the Event port.
  * @param  arg the user argument
  * @param  pcb the tcp_pcb that has received the data
  * @param  p the packet buffer
  * @param  addr the addres of sender
  * @param  port the port number of sender
  * @retval None
  */
static void netRecvEventCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                 struct ip_addr *addr, u16_t port)
{
    NetPath *netPath = (NetPath *)arg;

    /* Place the incoming message on the Event Port QUEUE. */
    if (!netQPut(&netPath->eventQ, p))
    {
        pbuf_free(p);
        p = NULL;
        ERROR("netRecvEventCallback: queue full\n");
        return;
    }
}

/**
  * @brief  Processing an incoming message on the General port.
  * @param  arg the user argument
  * @param  pcb the tcp_pcb that has received the data
  * @param  p the packet buffer
  * @param  addr the addres of sender
  * @param  port the port number of sender
  * @retval None
  */
static void netRecvGeneralCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                   struct ip_addr *addr, u16_t port)
{
    NetPath *netPath = (NetPath *)arg;
    /* Place the incoming message on the Event Port QUEUE. */
    if (!netQPut(&netPath->generalQ, p))
    {
        pbuf_free(p);
        p = NULL;
        ERROR("netRecvGeneralCallback: queue full\n");
        return;
    }
}
/**
 * Init all network transports
 *
 * @param netPath 
 * @param rtOpts 
 * @param ptpClock 
 * 
 * @return TRUE if successful
 */
Boolean netInit(NetPath * netPath, RunTimeOpts * rtOpts, PtpClock * ptpClock)
{
	 struct ip_addr interfaceAddr;

    struct in_addr netAddr;
    char addrStr[NET_ADDRESS_LENGTH];

    DBG("netInit\n");

    /* find a network interface */
		//通过网络接口找到网络地址
		//interfaceID存储网络接口MAC地址
    interfaceAddr.addr = findIface(rtOpts->ifaceName, ptpClock->interfaceID, netPath);
//		printf("interfaceAddr.addr=0x%04X\n",interfaceAddr.addr);
    if (!(interfaceAddr.addr)) //该接口未设置ip地址
    {
        goto fail01;
    }

    /* Open lwIP raw udp interfaces for the event port. */
    netPath->eventPcb = udp_new();

    if (NULL == netPath->eventPcb)
    {
        ERROR("netInit: Failed to open Event UDP PCB\n");
        goto fail02;
    }

    /* Open lwIP raw udp interfaces for the general port. */
    netPath->generalPcb = udp_new();

    if (NULL == netPath->generalPcb)
    {
        ERROR("netInit: Failed to open General UDP PCB\n");
        goto fail03;
    }

    /* Initialize the buffer queues. */
    netQInit(&netPath->eventQ);

    netQInit(&netPath->generalQ);

    /* Configure network (broadcast/unicast) addresses. */
    netPath->unicastAddr = 0; /* disable unicast */

    /*Init General multicast IP address*/
    memcpy(addrStr, DEFAULT_PTP_DOMAIN_ADDRESS, NET_ADDRESS_LENGTH);

    if (!inet_aton(addrStr, &netAddr))
    {
        ERROR("netInit: failed to encode multi-cast address: %s\n", addrStr);
        goto fail04;
    }

    netPath->multicastAddr = netAddr.s_addr;

    /* join multicast group (for receiving) on specified interface */
    igmp_joingroup(IP_ADDR_ANY, (struct ip_addr *)&netAddr);


    /*Init Peer multicast IP address*/
    memcpy(addrStr, PEER_PTP_DOMAIN_ADDRESS, NET_ADDRESS_LENGTH);

    if (!inet_aton(addrStr, &netAddr))
    {
        ERROR("netInit: failed to encode peer multi-cast address: %s\n", addrStr);
        goto fail04;
    }

    netPath->peerMulticastAddr = netAddr.s_addr;

    /* join peer multicast group (for receiving) on specified interface */
    igmp_joingroup(IP_ADDR_ANY, (struct ip_addr *)&netAddr);


    /* multicast send only on specified interface */
    netPath->eventPcb->multicast_ip.addr = netPath->multicastAddr;
    netPath->generalPcb->multicast_ip.addr = netPath->multicastAddr;

    /* Establish the appropriate UDP bindings/connections for events. */
    udp_recv(netPath->eventPcb, netRecvEventCallback, netPath);
    udp_bind(netPath->eventPcb, IP_ADDR_ANY, PTP_EVENT_PORT);
    /*  udp_connect(netPath->eventPcb, &netAddr, PTP_EVENT_PORT); */

    /* Establish the appropriate UDP bindings/connections for general. */
    udp_recv(netPath->generalPcb, netRecvGeneralCallback, netPath);
    udp_bind(netPath->generalPcb, IP_ADDR_ANY, PTP_GENERAL_PORT);
    /*  udp_connect(netPath->generalPcb, &netAddr, PTP_GENERAL_PORT); */

    /* Return a success code. */
    return TRUE;

    /*
    fail05:
        udp_disconnect(netPath->eventPcb);
        udp_disconnect(netPath->generalPcb);
    */
fail04:
    udp_remove(netPath->generalPcb);
fail03:
    udp_remove(netPath->eventPcb);
fail02:
fail01:
    return FALSE;
}


/*Check if data has been received*/
int 
netSelect(TimeInternal * timeout, NetPath * netPath)
{
	    /* Check the packet queues.  If there is data, return TRUE. */
    if (netQCheck(&netPath->eventQ) || netQCheck(&netPath->generalQ))
		{
       return 1;
		}
    return 0;
}

/** 
 * store received data from network to "buf" , get and store the
 * SO_TIMESTAMP value in "time" for an event message
 *
 * @note Should this function be merged with netRecvGeneral(), below?
 * Jan Breuer: I think that netRecvGeneral should be
 * simplified. Timestamp returned by this function is never
 * used. According to this, netInitTimestamping can be also simplified
 * to initialize timestamping only on eventSock.
 *
 * @param buf 
 * @param time 
 * @param netPath 
 *
 * @return
 */

static ssize_t netRecv(Octet *buf, TimeInternal *time, BufQueue * msgQueue)
{
	ssize_t length;
	int i, j;
	/* get actual buffer */
	struct pbuf * p, *pcopy;

	p = (struct pbuf*)netQGet(msgQueue);	//指向消息的pbuf
	if (!p)
	{
			return 0;
	}
	pcopy = p;														//拷贝

	/* Here, p points to a valid PBUF structure.  Verify that we have
	 * enough space to store the contents. */

	if (p->tot_len > PACKET_SIZE)		//所有的包长度小于PACKET_SIZE==300
	{
		 ERROR("netRecv: received truncated message\n");
		 return 0;
	}

	if (NULL != time)
	{
#if LWIP_PTP											//取出时间戳
			time->seconds = p->time_sec;
			time->nanoseconds = p->time_nsec;
#else
			getTime(time);
#endif
	}

	/* Copy the PBUF payload into the buffer. */
	j = 0;
	length = p->tot_len;

	for (i = 0; i < length; i++)	//把pbuf放入buf
	{
			buf[i] = ((u8_t *)pcopy->payload)[j++];
			if (j == pcopy->len)
			{
					pcopy = pcopy->next;	//处理pbuf的链表(如果有)
					j = 0;
			}
	}

	/* Free up the pbuf (chain). */
	pbuf_free(p);
	pbuf_free(pcopy);
	return length;
}


ssize_t netRecvEvent(NetPath *netPath, Octet *buf, TimeInternal *time)
{
    return netRecv(buf, time, &netPath->eventQ);
}

/** 
 * 
 * store received data from network to "buf" get and store the
 * SO_TIMESTAMP value in "time" for a general message
 * 
 * @param buf 
 * @param time 
 * @param netPath 
 * 
 * @return 
 */

ssize_t netRecvGeneral(NetPath *netPath, Octet *buf, TimeInternal *time)
{
    return netRecv(buf, time, &netPath->generalQ);
}

static ssize_t netSend(const Octet *buf, UInteger16 length, TimeInternal *time, const Integer32 * addr, struct udp_pcb * pcb)
{
	err_t result;
	struct pbuf * p;

	/* Allocate the tx pbuf based on the current size. */
	p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
	if (NULL == p)
	{
			ERROR("netSend: Failed to allocate Tx Buffer\n");
			goto fail01;
	}

	/* Copy the incoming data into the pbuf payload. */
	result = pbuf_take(p, buf, length);

	if (ERR_OK != result)
	{
			ERROR("netSend: Failed to copy data to Pbuf (%d)\n", result);
			goto fail02;
	}

	/* send the buffer. */
	result = udp_sendto(pcb, p, (void *)addr, pcb->local_port);

	if (ERR_OK != result)
	{
			ERROR("netSend: Failed to send data (%d)\n", result);
			goto fail02;
	}

	if (NULL != time)
	{
#if LWIP_PTP
			time->seconds = p->time_sec;
			time->nanoseconds = p->time_nsec;
#else
			/* TODO: use of loopback mode */
			/*
			time->seconds = 0;
			time->nanoseconds = 0;
			*/
			getTime(time);
#endif
			DBGV("netSend: %ds %dns\n", time->seconds, time->nanoseconds);
	} else {
			DBGV("netSend\n");
	}


fail02:
	pbuf_free(p);
fail01:
	return length;
	/*  return (0 == result) ? length : 0; */
}


ssize_t netSendEvent(NetPath *netPath, const Octet *buf, UInteger16 length, TimeInternal *time)
{
    return netSend(buf, length, time, &netPath->multicastAddr, netPath->eventPcb);
}


ssize_t netSendGeneral(NetPath *netPath, const Octet *buf, UInteger16 length)
{
    return netSend(buf, length, NULL, &netPath->multicastAddr, netPath->generalPcb);
}

ssize_t netSendPeerGeneral(NetPath *netPath, const Octet *buf, UInteger16 length)
{
    return netSend(buf, length, NULL, &netPath->peerMulticastAddr, netPath->generalPcb);
}

ssize_t netSendPeerEvent(NetPath *netPath, const Octet *buf, UInteger16 length, TimeInternal* time)
{
    return netSend(buf, length, time, &netPath->peerMulticastAddr, netPath->eventPcb);
}
