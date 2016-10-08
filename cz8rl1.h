#ifndef __CZ8RL1_H__
#define __CZ8RL1_H__

/* X1->CZ8RL1 command & CZ8RL1->X1 status */
#define CZ8RL1_EJECT  0x00
#define CZ8RL1_STOP   0x01
#define CZ8RL1_PLAY   0x02
#define CZ8RL1_FF     0x03
#define CZ8RL1_REW    0x04
#define CZ8RL1_AFF    0x05
#define CZ8RL1_AREW   0x06
#define CZ8RL1_REC    0x0A

/* X1->CZ8RL1 command (bit1..6 = Don't cate) */
#define CZ8RL1_STATUS 0x80
#define CZ8RL1_SENSOR 0x81

/* CZ8RL1->X1 status */
#define CZ8RL1_BREAK  0xff
/* 0x80～0x87 sensor (answer of CZ8RL1_SENSOR) */
/*             bit0   :tape end   (0=ON)       */
/*             bit1   :tape set   (1=ON)       */
/*             bit2   :writeprtect(0=ON)       */

/* function return code */
#define CZ8RL1_STS_OK           0x100 /* エラー無し                             */
#define CZ8RL1_STS_AUTOSTOP     0x101 /* 自動停止                               */
#define CZ8RL1_STS_BUFFULL      0x102 /* バッファフル                           */
#define CZ8RL1_STS_BREAK        0x103 /* カセット停止（ＢＵＳＹ検出)            */
#define CZ8RL1_STS_DOAPSS       0x104 /* ＡＰＳＳ実行中                         */
/* function error code */
#define CZ8RL1_ERR_NOTFOUND  -1 /* CZ8RL1無し(BUSY=H , STATUS=H)          */
#define CZ8RL1_ERR_BSYACK    -2 /* BUSYが規定時間内にHIGHに応答しなかった */
#define CZ8RL1_ERR_DOAPSS    -3 /* ＡＰＳＳ実行中                         */
#define CZ8RL1_ERR_NOHEADER  -4 /* STATUSのヘッダー信号無し               */
#define CZ8RL1_ERR_BADHEAD   -5 /* STATUSのヘッダーに長さが規定外         */
#define CZ8RL1_ERR_BADBIT    -6 /* データビットの長さが規定外             */

#define CZ8RL1_ERR_SPEED1   -10/* ＣＰＵ速度が追いつかない               */
#define CZ8RL1_ERR_SPEED2   -11/* ＣＰＵ速度が追いつかない               */

/* command output to CZ8RL1            */
/* in     : command code               */
/* return : status code or error value */
int cz8rl1_write(BYTE value);
#define cz8rl1_get_status() cz8rl1_write(0x80)
#define cz8rl1_get_sensor() cz8rl1_write(0x81)

/* sampling CZ8RL1 read data */
int CZ8RL1_read_data(
	BYTE *buf,			/* buffer pointer                  */
	unsigned int *pbufsize,	/* buffer size / sampling size     */
	unsigned int *numeddge,	/*             / num of eddges     */
	int busybreak,		/* break when busy high            */
	int rate,			/* samplingrate                    */
	double *jitterl,	/* jitter fall  % / jitter avg.    */
	double *jitterh,	/* jitter raise % / jitter avg.    */
	int idlebreak);		/* auto ditect stop time           */

int cz8rl1_write_data(BYTE *buf,unsigned int *pbufsize,unsigned int rate,int busybreak);

/* initialize CZ8RL1 I/O port */
int CZ8RL1_init(
	WORD wport_addr,	/* 書き込みポートのアドレス							*/
	BYTE wdata_base,		/* ポートの初期値（含む負極性信号の設定）		*/
	BYTE mask_write_data,	/* WRITE_DATA出力信号ビットのマスク値			*/
	BYTE mask_command,		/* COMMAND出力信号ビットのマスク値				*/
	BYTE mask_strobe,		/* STROBE出力信号ビットのマスク値				*/
	WORD rport_addr,	/* 読み込みポートのアドレス							*/
	BYTE rdata_base,		/* ポートの負極性設定マスク						*/
	BYTE mask_busy,			/* BUSY入力信号ビットのマスク値					*/
	BYTE mask_status,		/* STATUS入力信号ビットのマスク値				*/
	BYTE mask_read_data		/* READ_DATA入力信号ビットのマスク値			*/
);
#endif
