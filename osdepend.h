#ifndef __OSDEPEND_H__
#define __OSDEPEND_H__

#define TICKER_INLINE 1
#define TICKER_DWORD  0

/*----------------------------------------------------------------------
	TYPES
----------------------------------------------------------------------*/
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;

/*----------------------------------------------------------------------
	TICKER
----------------------------------------------------------------------*/
#if TICKER_DWORD
typedef unsigned long TICKER;
#else
typedef unsigned long long TICKER;
#endif

extern TICKER ticks_per_sec;

#define TICKER_AUTO_DITECT 0
int init_ticker(TICKER fixed_ticker);
#if TICKER_INLINE
static __inline__ TICKER ticker(void)
{
	TICKER result;

	// use RDTSC
	__asm__ __volatile__ (
		"rdtsc"
		: "=A" (result)
	);
	return result;
}
#else
TICKER ticker(void);
#endif

/*----------------------------------------------------------------------
	IRQ controll
----------------------------------------------------------------------*/
void disable(void);
void enable(void);

/*----------------------------------------------------------------------
	I/O access
----------------------------------------------------------------------*/
#define OUTPORTB(p,d) outportb(p,d)
#define INPORTB(p) inportb(p)

#endif
