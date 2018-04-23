/*-
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
 * @file   timer.c
 * @date   Wed Jun 23 09:41:26 2010
 * 
 * @brief  The timers which run the state machine.
 * 
 * Timers in the PTP daemon are run off of the signal system.  
 */

#include "../ptpd.h"

//#define US_TIMER_INTERVAL (62500)
volatile unsigned int elapsed;

/*
 * original code calls sigalarm every fixed 1ms. This highly pollutes the debug_log, and causes more interrupted instructions
 * This was later modified to have a fixed granularity of 1s.
 *
 * Currently this has a configured granularity, and timerStart() guarantees that clocks expire ASAP when the granularity is too small.
 * Timers must now be explicitelly canceled with timerStop (instead of timerStart(0.0))
 */

void 
catch_alarm(int sig)
{
	elapsed += sig;
	/* be sure to NOT call DBG in asynchronous handlers! */
}

void 
initTimer(void)
{
	DBG("initTimer elapsed is 0 !\n");
	elapsed = 0;
}

void 
timerUpdate(IntervalTimer * itimer)
{
	int i, delta;
	/*
	 * latch how many ticks we got since we were last called											//锁存ticks间隔
	 * remember that catch_alarm() is totally asynchronous to this timerUpdate()	//记录函数catch_alarm()异步于this timerUpdate()函数
	 */
	delta = elapsed;//间隔时间=elapsed
	elapsed = 0;

	if (delta <= 0)
		return;

	/*
	 * if time actually passed, then decrease every timer left
	 * the one(s) that went to zero or negative are:
	 *  a) rearmed at the original time (ignoring the time that may have passed ahead)
	 *  b) have their expiration latched until timerExpired() is called
	 */
	for (i = 0; i < TIMER_ARRAY_SIZE; ++i) {
		if ((itimer[i].interval) > 0 && ((itimer[i].left) -= delta) 
		    <= 0) {
			itimer[i].left = itimer[i].interval;
			itimer[i].expire = TRUE;
			DBG2("TimerUpdate:    Timer %u has now expired.   (Re-armed again with interval %d, left %d)\n", i, itimer[i].interval, itimer[i].left );
		}
	}

}

void 
timerStop(UInteger16 index, IntervalTimer * itimer)													//配置定时器停止工作
{
	if (index >= TIMER_ARRAY_SIZE)
		return;

	itimer[index].interval = 0;
	DBG2("timerStop:      Stopping timer %d.   (New interval: %d; New left: %d)\n", index, itimer[index].left , itimer[index].interval);
}

void 
timerStart(UInteger16 index, UInteger32 interval, IntervalTimer * itimer)		//配置定时器开始工作
{

	if (index >= TIMER_ARRAY_SIZE)																						//定时器号不对
		return;
	itimer[index].expire = FALSE;																							//定时器超时标志复位
	
	itimer[index].left = interval;																						//设置下溢值
	
	itimer[index].interval = itimer[index].left;															//赋计数器下溢的初值=interval

	DBG2("timerStart:     Set timer %d to %f.  New interval: %d; new left: %d\n", index, interval, itimer[index].left , itimer[index].interval);
}



/*
 * This function arms the timer with a uniform range, as requested by page 105 of the standard (for sending delayReqs.)
 * actual time will be U(0, interval * 2.0);
 *
 * PTPv1 algorithm was:
 *    ptpClock->R = getRand(&ptpClock->random_seed) % (PTP_DELAY_REQ_INTERVAL - 2) + 2;
 *    R is the number of Syncs to be received, before sending a new request
 * 
 */ 
void timerStart_random(UInteger16 index, UInteger32 interval, IntervalTimer * itimer)
{
	float new_value;

	new_value = getRand(interval * 2) ;
	DBG2(" timerStart_random: requested %.2f, got %.2f\n", interval, new_value);
	
	timerStart(index, new_value, itimer);
}


Boolean 
timerExpired(UInteger16 index, IntervalTimer * itimer)
{
	timerUpdate(itimer);

	if (index >= TIMER_ARRAY_SIZE)			//参数错误
		return FALSE;
	if (!itimer[index].expire)					//定时器未到时
		return FALSE;

	itimer[index].expire = FALSE;				//重新开始计时

	DBG2("timerExpired:   Timer %d expired, taking actions.   current interval: %d; current left: %d\n", index, itimer[index].left , itimer[index].interval);
	return TRUE;
}



Boolean 
timerStopped(UInteger16 index, IntervalTimer * itimer)
{
	timerUpdate(itimer);

	if (index >= TIMER_ARRAY_SIZE)
		return FALSE;

	if (itimer[index].interval == 0) {
		return TRUE;
	DBG2("timerStopped:   Timer %d is stopped\n", index);
	}

	return FALSE;

}

Boolean 
timerRunning(UInteger16 index, IntervalTimer * itimer)
{
	timerUpdate(itimer);

	if (index >= TIMER_ARRAY_SIZE)
		return FALSE;

	if ((itimer[index].interval != 0) &&
	    (itimer[index].expire == FALSE)) {
		return TRUE;
	DBG2("timerRunning:   Timer %d is running\n", index);
	}

	return FALSE;

}

