/*
	システム依存関数 DJGPP版
	・リアルタイム時間取得
	・割り込み制御
	・Ｉ／Ｏアクセス
*/

#include <stdio.h>
#include <time.h>
#include "osdepend.h"

TICKER ticks_per_sec;

static int cpu_rdtsc(void)
{
	int result;

	__asm__ (
		"movl $1,%%eax     ; "
		"xorl %%ebx,%%ebx  ; "
		"xorl %%ecx,%%ecx  ; "
		"xorl %%edx,%%edx  ; "
		"cpuid             ; "
		"testl $0x10,%%edx ; "
		"setne %%al        ; "
		"andl $1,%%eax     ; "
	:  "=&a" (result)   /* the result has to go in eax */
	:       /* no inputs */
	:  "%ebx", "%ecx", "%edx" /* clobbers ebx ecx edx */
	);
	return result;
}

#if !TICKER_INLINE
TICKER ticker(void)
{
	TICKER result;

	// use RDTSC
	__asm__ __volatile__ (
		"rdtsc"
		: "=A" (result)
	);
	return result;
}
#endif

/* get ticks per sec */
int init_ticker(TICKER fixed_ticker)
{
	clock_t a,b;
	TICKER start,end;

	if (!cpu_rdtsc())
		return 0;

	ticks_per_sec = fixed_ticker;
	if(fixed_ticker == TICKER_AUTO_DITECT)
	{	/* auto ditect */
		a = clock();
		/* wait some time to let everything stabilize */
		do
		{
			b = clock();
		} while (b-a < CLOCKS_PER_SEC/10);
	
		a = clock();
		start = ticker();
		do
		{
			b = clock();
		} while (b-a < CLOCKS_PER_SEC/4);
		end = ticker();
		ticks_per_sec = (end - start)*4;
	}

	return 1;
}

/* IRQ controll */
void disable(void)
{
	__asm__ __volatile__ (
		"cli"
	);
}

void enable(void)
{
	__asm__ __volatile__ (
		"sti"
	);
}
