/*
	リアルタイム制御関数
	2001.10.1 Tatsuyuki Satoh
*/

#include <stdio.h>
#include <sys/io.h>
#include "realtime.h"
TICKER tickbase;

/* ベースタイムからusecウェイト、ベースタイムを更新 */
void tick_wait_usec(int usec)
{
	TICKER ticks = ticks_per_sec * usec / 1000000;
	TICKER delta;

	do{
		delta = ticker()-tickbase;
	}while(delta < ticks );
	tickbase += ticks;
}

/* 入力ポートのビットが規定時間内に更新されるかどうか */
/* タイムベースは進められる */
int tick_check_io(DWORD port,BYTE xormask,BYTE andmask,DWORD limit_min_usec,DWORD limit_max_usec)
{
	TICKER min = TICK_IN_USEC(limit_min_usec);
	TICKER max = TICK_IN_USEC(limit_max_usec);
	TICKER width;

	do{
		width = ticker()-tickbase;
		if( width >= max)
			return 0;
	}while( ((inb(port)^xormask)&andmask) );
	tickbase += width;
	if( width < min )
		return 0;
	return 1;
}
