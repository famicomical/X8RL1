#ifndef __TICKER_H__
#define __TICKER_H__

#include "osdepend.h"

/* convert tick and other units */
#define USEC_IN_TICK(tick) ((tick)*1000000/ticks_per_sec)
#define TICK_IN_USEC(usec) (ticks_per_sec*(usec)/1000000)
#define TICK_IN_HZ(hz) (ticks_per_sec/(hz))

/* timebase */
extern TICKER tickbase;
/* preset basetime */
#define tick_preset_basetime()  { tickbase = ticker(); }

/* progress time (ticks) from basetine */
#define tick_progress() (ticker()-tickbase)

/* wait time(usec) from basetime , and update basetime */
void tick_wait_usec(int usec);

/* check io bit change time from basetime , and update basetime */
int tick_check_io(DWORD port,BYTE xormask,BYTE andmask,DWORD limit_min_usec,DWORD limit_max_usec);

#endif