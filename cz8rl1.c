/*
	CZ8RL1 送受信ドライバー
	パラレルポートに接続されたX1turbo用データレコーダCZ8RL1と交信する
	リアルタイム制御を行うため、tickerが及び割り込み制御関数が必要
	Ｉ／Ｏを直接たたくので、WindowsNT/2000では不可
	送受信中は割り込み禁止される。
	書き込みポート、読み込みポートは、それぞれ同じＩ／Ｏアドレスの
	中になくてはならない。
*/

#include <stdio.h>
#include <sys/io.h>
#include <string.h>
#include "realtime.h"
#include "x1tape.h"
#include "cz8rl1.h"

/* 8RL1デバッグのため */
#define VERBOSE 0

/* output I/O assign and access macro */
#define cz8rl1_write_port(value)   outb(cz8rl1_wport,value)
static WORD cz8rl1_wport;			/* port address        */
static BYTE cz8rl1_wp_idle;		/* when idle           */
static BYTE cz8rl1_wp_cmdon;		/* when CMD=ON (LOW)   */
static BYTE cz8rl1_wp_stbon;		/* when STB=ON (HIGH)  */
static BYTE cz8rl1_wp_wrdh;		/* when WRITE_DATA = H */
static BYTE cz8rl1_wp_wrdl;		/* when WRITE_DATA = L */
static BYTE cz8rl1_wp_break;

/* macro of read cz8rl1 port and bit selection for external data transceiver */
#define cz8rl1_read_port(andmask) ((inb(cz8rl1_rport)^cz8rl1_rp_base)&(andmask))
static WORD cz8rl1_rport;     /* port address */
static BYTE cz8rl1_rp_base;   /* reverce bit  */
static BYTE cz8rl1_rm_busy;   /* busy         */
static BYTE cz8rl1_rm_status; /* status       */
static BYTE cz8rl1_rm_rdata;  /* read data    */

/*-----------------------------------------------------------------
	CZ8RL1 RAW interface
-----------------------------------------------------------------*/

#define check_low(mask,min,max) tick_check_io(cz8rl1_rport,cz8rl1_rp_base,(mask),(min),(max))
#define check_high(mask,min,max) tick_check_io(cz8rl1_rport,~cz8rl1_rp_base,(mask),(min),(max))

/* command / status time data */
#define TICK_HEAD   1000  /* 1000usec L */
#define TICK_BIT1    750  /*  750usec H */
#define TICK_BIT0    250  /*  250usec H */
#define TICK_BIT     250  /*  250usec L */

static int cz8rl1_is_apss(void)
{
	tick_preset_basetime();
	/* APSS check and break */
	if( cz8rl1_read_port(cz8rl1_rm_status|cz8rl1_rm_busy) == cz8rl1_rm_busy)
	{
		if( !check_high(cz8rl1_rm_status,(long)(0),(long)(TICK_HEAD*2)) )
		{	/* APSS found */
			return 1;
		}
	}
	return 0;
}

static int cz8rl1_apss_break(void)
{
	int dobreak;

	if( !cz8rl1_is_apss())
		return 0;
	/* assert break request */
	cz8rl1_write_port(cz8rl1_wp_break);
	/* wait for break */
	tick_preset_basetime();
	dobreak = check_low(cz8rl1_rm_busy,(long)(0),(long)(TICK_HEAD*1000));
	/* deassert break request */
	cz8rl1_write_port(cz8rl1_wp_idle);
	return dobreak;
}

/* CZ8RL1から１バイト受信 */
/* 割り込み禁止状態で呼び出すこと */
static int cz8rl1_rx(void)
{
	TICKER bitwidth;
	int value = 0x00;
	int bitmask;

	tick_preset_basetime();

	/* wait for busy on at 2sec */
	if( !check_high(cz8rl1_rm_busy,0,2000000) )
		return CZ8RL1_ERR_BSYACK;
	tick_preset_basetime();
	tick_wait_usec(1000);
	cz8rl1_write_port(cz8rl1_wp_stbon);
	/* wait for busy off at 2msec */
	if( !check_low(cz8rl1_rm_busy,0,2000) )
	{
		cz8rl1_write_port(cz8rl1_wp_idle);
		return CZ8RL1_STS_DOAPSS;
	}
	/* wait for status on 2sec */
	if( !check_low(cz8rl1_rm_status,0,2000000) )
	{
		cz8rl1_write_port(cz8rl1_wp_idle);
		return CZ8RL1_ERR_NOHEADER;
	}
	/* check header 1000us +- 25% */
	if( !check_high(cz8rl1_rm_status,(long)(TICK_HEAD*0.75),(long)(TICK_HEAD*1.25)) )
	{
		cz8rl1_write_port(cz8rl1_wp_idle);
		return CZ8RL1_ERR_BADHEAD;
	}
	/* get data */
	for(bitmask=0x80;bitmask;bitmask>>=1)
	{
		/* space */
		while( (cz8rl1_read_port(cz8rl1_rm_status)))
			if( USEC_IN_TICK(tick_progress()) > 2000)
			{
				cz8rl1_write_port(cz8rl1_wp_idle);
				return CZ8RL1_ERR_BADBIT;
			}
		/* make */
		do
		{
			bitwidth = USEC_IN_TICK(tick_progress());
			if( bitwidth > 2000)
			{
				cz8rl1_write_port(cz8rl1_wp_idle);
				return CZ8RL1_ERR_BADBIT;
			}
		}while(!(cz8rl1_read_port(cz8rl1_rm_status)));
		/* */
		tick_wait_usec(bitwidth);
		if( bitwidth > 750)
			value |= bitmask;
	}
	cz8rl1_write_port(cz8rl1_wp_idle);

#if VERBOSE
	printf("CZ8Rl1 RX CODE=%02x (%s)\n",status,str);
#endif
	return value;
}

/* CZ8RL1に１バイト送信 */
/* 割り込み禁止状態で呼び出すこと */
static void cz8rl1_tx(BYTE value)
{
	BYTE bitmask;

	tick_preset_basetime();
	/* header */
	cz8rl1_write_port(cz8rl1_wp_cmdon);
	tick_wait_usec(TICK_HEAD);
	cz8rl1_write_port(cz8rl1_wp_idle);
	/* data */
	for( bitmask = 0x80 ; bitmask ; bitmask>>=1 )
	{
		/* space */
		tick_wait_usec((value & bitmask) ? TICK_BIT1 : TICK_BIT0);
		/* make */
		cz8rl1_write_port(cz8rl1_wp_cmdon);
		tick_wait_usec(TICK_BIT);
		cz8rl1_write_port(cz8rl1_wp_idle);
	}
}

/* CMTにコマンド転送＋ステータスリード */
/*
 in
	value = controll command
			bit7    : 0
			bit6..0 : CMT command
  return
	<0    : error code
	0-255 : status command
		bit7    : 0
		bit6..1 : Don't care
		bit0    : 0=status / 1=sensor
 */
int cz8rl1_write(BYTE value)
{
	TICKER base,bitwait;
	BYTE bitmask;
	int status;

	disable();
	/* APSS check and break */
	if( cz8rl1_is_apss() )
	{
		if( !cz8rl1_apss_break())
		{
			enable();
			return CZ8RL1_ERR_DOAPSS;
		}
		status = cz8rl1_rx();
		enable();
		/* error  ? */
		if( status < 0 )
			return status;
		disable();
	}
	cz8rl1_tx(value);
	status = cz8rl1_rx();
	enable();

	tick_wait_usec(TICK_IN_USEC(1000));
#if VERBOSE
	printf("CZ8RL1 TX CODE=%02x\n",value);
#endif
	return status;
}

/* CZ8RL1 -> バッファ 吸い出し */
int CZ8RL1_read_data(
	BYTE *buf,			/* buffer pointer                  */
	unsigned int *pbufsize,	/* buffer size / sampling size     */
	unsigned int *numeddge,	/*             / num of eddges     */
	int busybreak,		/* break when busy high            */
	int rate,			/* samplingrate                    */
	double *jitterl,	/* jitter fall  % / jitter avg.    */
	double *jitterh,	/* jitter raise % / jitter avg.    */
	int idlebreak)		/* auto ditect stop time           */
{
	/* sampling works */
	BYTE jit_flag[2];
	TICKER  tick_step = TICK_IN_HZ(rate);
	TICKER tick_jitter[2];
	TICKER ticknow;
	unsigned int idle_limit,idle_left;
	BYTE *ptr ,*eptr;
	BYTE nowport , nxtport , portmask;
	BYTE level , mask , mask2;
	TICKER jitter_sum[2];
	unsigned int sampled_eddge;
	int err;

	/* clear buffer & buffer pointers */
	memset(buf,0,*pbufsize);
	ptr  = buf;
	eptr = buf + (*pbufsize);
	/* jitter ticks */
	tick_jitter[0] =  (TICKER)((*jitterl)*tick_step/100);
	jit_flag[0]    =  jitterl ? 1 : 0;
	tick_jitter[1] =  (TICKER)((*jitterh)*tick_step/100);
	jit_flag[1]    =  jitterh ? 1 : 0;
	/* auto stop timer */
	idle_left = idle_limit = idlebreak * rate;
	/* read port signal */
	portmask = cz8rl1_rm_rdata;
	if(busybreak)	/* use busy break? */
		portmask |= cz8rl1_rm_busy;
	/* report */
	sampled_eddge = 0;
	jitter_sum[0] = jitter_sum[1] = 0;
	err = 0;
	/* start level = low */
	level = 0;
	mask2 = 0;
	nxtport = 0;

	/* start sampling ! */
	disable();
	tick_preset_basetime();
	do
	{
		/* sampling READ DATA & timming */
		nowport = cz8rl1_read_port(portmask);
		ticknow = tick_progress();
		/* bit sampling check */
		if( ticknow >= tick_step)
		{
			tickbase += tick_step;
			ticknow -= tick_step;
			/* set bit */
			*ptr |= (mask&mask2);
			/* shift bit point */
			if( !(mask >>=1) )
			{	/* address increment */
				mask = 0x80;
				if( ++ptr==eptr)
					break;
			}
			else
			{
				/* auto stop timer check */
				if( --idle_left == 0)
					break;
			}
			/* speed error check! */
			if( tick_progress() >= tick_step)
			{
				err = CZ8RL1_ERR_SPEED1;
				break;
			}
		}
		/* eddge check */
		if( nowport != nxtport)
		{
			/* get jitter sum & adjust jitter */
			jitter_sum[level] += ticknow;
			if(jit_flag[level])
				tickbase += ticknow - tick_jitter[level];
			sampled_eddge++;			/* eddge count */
			/* reverce sampling level */
			level ^= 1;					/* level      */
			nxtport ^= cz8rl1_rm_rdata;	/* port value */
			mask2 ^= 0xff;				/* data mask  */
			/* preset auto stop timer */
			idle_left = idle_limit;
			/* speed error check! */
			if( tick_progress() >= tick_step)
			{
				err = CZ8RL1_ERR_SPEED2;
				break;
			}
		}
	}while( !(nowport&cz8rl1_rm_busy) );
	enable();

	/* make return code */
	if(!err)
	{
		if( idle_left == 0 )
			err = CZ8RL1_STS_AUTOSTOP;
		else if( ptr==eptr)
			err = CZ8RL1_STS_BUFFULL;
		else
			err = CZ8RL1_STS_BREAK;
	}

	/* return sampled length */
	*pbufsize = (ptr - buf);

	/* return sampled jitter avg. */
	if(sampled_eddge==0)
		*numeddge = *jitterl = *jitterh = 0;
	else
	{
		*numeddge = sampled_eddge;
		*jitterl = (double)(jitter_sum[1]*2/sampled_eddge) / tick_step;
		*jitterh = (double)(jitter_sum[0]*2/sampled_eddge) / tick_step;
	}
	return err;
}

/* CZ8RL1 <- バッファ吐き出し */
int cz8rl1_write_data(BYTE *buf,unsigned int *pbufsize,unsigned int rate,int busybreak)
{
	unsigned int memsize = *pbufsize;
	unsigned int bitlength ,writepos;
	TICKER tick_step;
	BYTE cdata,nport;

	bitlength = (*pbufsize)*8;
	tick_step = 1000000 / rate;

	disable();
	tick_preset_basetime();

	for(writepos=0;writepos<bitlength;writepos++)
	{
		/* BUSY break check */
		if( busybreak && cz8rl1_read_port(cz8rl1_rm_busy) )
			break;
		if(writepos%8 == 0)
		{
			cdata = buf[writepos/8];
		}
		else
			cdata <<=1;
		nport = (cdata&0x80) ? cz8rl1_wp_wrdh : cz8rl1_wp_wrdl;
		/* wait for next bit & update */
		tick_wait_usec(tick_step);
		cz8rl1_write_port(nport);
	}
	cz8rl1_write_port(cz8rl1_wp_idle);
	enable();

	if( writepos < bitlength )
		return CZ8RL1_STS_BREAK;
	return CZ8RL1_STS_OK;
}

/* CZ8RL1 I/O初期化 */
int CZ8RL1_init(
	WORD wport_addr,
	BYTE wdata_base,
	BYTE mask_write_data,
	BYTE mask_command,
	BYTE mask_strobe,
	WORD rport_addr,
	BYTE rdata_base,
	BYTE mask_busy,
	BYTE mask_status,
	BYTE mask_read_data
)
{
#if VERBOSE
	printf("CZ8RL1 initialize waiting\n");
#endif
	/* write value */
	cz8rl1_wport    = wport_addr;
	cz8rl1_wp_idle  = wdata_base ^ mask_command; /* CMD = HIGH */
	cz8rl1_wp_cmdon = wdata_base;
	cz8rl1_wp_stbon = wdata_base ^ mask_command ^ mask_strobe;
	cz8rl1_wp_wrdh  = wdata_base ^ mask_command ^ mask_write_data;
	cz8rl1_wp_wrdl  = cz8rl1_wp_idle;
	cz8rl1_wp_break = wdata_base ^ mask_strobe;
	/* read mask */
	cz8rl1_rport     = rport_addr;
	cz8rl1_rp_base   = rdata_base,
	cz8rl1_rm_busy   = mask_busy;
	cz8rl1_rm_status = mask_status;
	cz8rl1_rm_rdata  = mask_read_data;

	/* output port initialize */
	cz8rl1_write_port(cz8rl1_wp_idle);
	
	/* wait for 100ms */
	tick_preset_basetime();
	cz8rl1_write_port(cz8rl1_wp_idle);
	tick_wait_usec(100000);

#if 0
	/* exist check */
	if( cz8rl1_read_port(cz8rl1_rm_status|cz8rl1_rm_busy) == cz8rl1_rm_status|cz8rl1_rm_busy)
		return CZ8RL1_ERR_NOTFOUND;
#endif

#if VERBOSE
	printf("CZ8RL1 initialize finish!\n");
#endif
	return 1;
}