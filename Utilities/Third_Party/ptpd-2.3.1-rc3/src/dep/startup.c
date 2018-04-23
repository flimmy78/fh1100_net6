/*-
 * Copyright (c) 2014      Wojciech Owczarek,
 * Copyright (c) 2012-2013 George V. Neville-Neil,
 *                         Wojciech Owczarek
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
 * @file   startup.c
 * @date   Wed Jun 23 09:33:27 2010
 * 
 * @brief  Code to handle daemon startup, including command line args
 * 
 * The function in this file are called when the daemon starts up
 * and include the getopt() command line argument parsing.
 */

#include "../ptpd.h"
#include "share.h"
/*
 * valgrind 3.5.0 currently reports no errors (last check: 20110512)
 * valgrind 3.4.1 lacks an adjtimex handler
 *
 * to run:   sudo valgrind --show-reachable=yes --leak-check=full --track-origins=yes -- ./ptpd2 -c ...
 */

/*
  to test daemon locking and startup sequence itself, try:

  function s()  { set -o pipefail ;  eval "$@" |  sed 's/^/\t/' ; echo $?;  }
  sudo killall ptpd2
  s ./ptpd2
  s sudo ./ptpd2
  s sudo ./ptpd2 -t -g
  s sudo ./ptpd2 -t -g -b eth0
  s sudo ./ptpd2 -t -g -b eth0
  ps -ef | grep ptpd2
*/

/*
 * Synchronous signal processing:
 * original idea: http://www.openbsd.org/cgi-bin/cvsweb/src/usr.sbin/ntpd/ntpd.c?rev=1.68;content-type=text%2Fplain
 */
//volatile sig_atomic_t	 sigint_received  = 0;
//volatile sig_atomic_t	 sigterm_received = 0;
//volatile sig_atomic_t	 sighup_received  = 0;
//volatile sig_atomic_t	 sigusr1_received = 0;
//volatile sig_atomic_t	 sigusr2_received = 0;

/* Pointer to the current lock file */
//FILE* G_lockFilePointer;

/*
 * exit the program cleanly
 */
void
do_signal_close(PtpClock * ptpClock)
{
//	ptpdShutdown(ptpClock);

//	NOTIFY("Shutdown on close signal\n");
//	exit(0);
}

void SaveDrift(RunTimeOpts *rtOpts,PtpClock *ptpClock)
{
	if(ptpClock->portState == PTP_PASSIVE || ptpClock->portState == PTP_MASTER ||
     ptpClock->clockQuality.clockClass < 128) {
      DBGV("We're not slave - not saving drift\n");
      return;
   }
}

void ptpdShutdown(RunTimeOpts *rtOpts,PtpClock * ptpClock)
{

	netShutdown(&ptpClock->netPath); //关闭组播，清楚连接

	free(ptpClock->foreign);
	
	ptpClock->foreign = NULL;
	/* free management messages, they can have dynamic memory allocated */
	if(ptpClock->msgTmpHeader.messageType == MANAGEMENT)
	{
		freeManagementTLV(&ptpClock->msgTmp.manage);
	}
	freeManagementTLV(&ptpClock->outgoingManageTmp);

#ifdef PTPD_SNMP
	snmpShutdown();
#endif /* PTPD_SNMP */

	SaveDrift(rtOpts, ptpClock);

	memset(rtOpts,0,sizeof(RunTimeOpts));
	memset(ptpClock,0,sizeof(PtpClock));
}

int ptpdStartup(Integer16 * ret,PtpClock *ptpClock,RunTimeOpts * rtOpts)
{
    /* no negative or zero attenuation */
	if (rtOpts->servo.ap < 1)
			rtOpts->servo.ap = 1;

	if (rtOpts->servo.ai < 1)
			rtOpts->servo.ai = 1;

  DBG("event POWER UP\n");
	
	//PtpClock的结构体大小
	DBG("allocated %d bytes for protocol engine data\n", (int)sizeof(PtpClock));

	ptpClock->foreign = (ForeignMasterRecord *)calloc(rtOpts->max_foreign_records,sizeof(ForeignMasterRecord));
	if (!ptpClock->foreign) {
		PERROR("failed to allocate memory for foreign master data");
		*ret = 2;
		return 0;
	} else {
		//分配的大小外来主时钟存放数组的大小
		DBG("allocated %d bytes for foreign master data\n",(int)(rtOpts->max_foreign_records * sizeof(ForeignMasterRecord)));
	}

	/* Init to 0 net buffer */
	memset(ptpClock->msgIbuf, 0, PACKET_SIZE);
	memset(ptpClock->msgObuf, 0, PACKET_SIZE);

	/* Init user_description */
	//PTPDv2
	memset(ptpClock->user_description, 0, sizeof(ptpClock->user_description));
	memcpy(ptpClock->user_description, &USER_DESCRIPTION, sizeof(USER_DESCRIPTION));
	
	/* Init outgoing management message */ 
	ptpClock->outgoingManageTmp.tlv = NULL; //管理报文为空
	
	*ret = 0;
	return 1;
}

