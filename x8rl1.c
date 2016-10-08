/*
	CZ8RL1 コントローラ

	・パラレルポートに接続されたX1turbo用データレコーダCZ8RL1
	　を制御する。
	・イメージファイルとＣＭＴとの送受信を行う。

	リアルタイム制御を行うため、tickerが及び割り込み制御関数が必要
	基本的に送受信／サンプリング動作中は割り込みが禁止される。

	このプログラムにおける結線図

	PCパラレル(DB25)   結線図    CZ8RL1(DIN7)
	DB5     7:------|R100Ω|---- WRITE_DATA(1)
	DB7     9:------|R100Ω|---- STROBE(2)

	ACK    10:---------------+-- BUSY(3)
	DB3     4:------|R10KΩ|-+

	SELECT 13:------|PullUp|-+-- READ DATA(4)
	DB1     6:------|R10KΩ|-+

	PE     12:------|PullUp|---- STATUS DATA(5)
	DB2     5:------|R10KΩ|-+

	GND 18-25:------------------ GND (6)
	DB6     8:------|R100Ω|---- COMMAND DATA(7)

	FG   CASE:------------------ FG(CASE)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include "getch.h"
#include "realtime.h"
#include "x1tape.h"
#include "cz8rl1.h"

/*-----------------------------------------------------------------
	各種パラメーター
-----------------------------------------------------------------*/
static char filename[128] ;	/* file name */
static int samplerate;		/* sampling rate */
static int maxtime;			/* maximum sampling time */
static int idlebreak;		/* auto ditect / idle after write */
static int idletime;		/* idle time of auto ditect / after write */
static double jitterh;			/* raise eddge jitter rate */
static double jitterl;			/* fall eddge jitter rate */
static int cz8rl1_controll;		/* CZ8RL1 full logic controll */

/*-----------------------------------------------------------------
	CZ8RL1を制御する
-----------------------------------------------------------------*/

static const char *cz8rl1_cmd_str[] = {
	"EJECT","STOP","PLAY","FF",	/* 0-3 */
	"REW"  ,"AFF" ,"AREW",""  ,	/* 4-7 */
	""     , ""   , "REC"};		/* 8-10*/

/* CZ8RL1にコマンドを送る */
static int cmt_controll(int command)
{
	const char *str ="不明です";
	int status;
	int ret = 0;

	/* 、オプション指定で無視 */
	if( !cz8rl1_controll )
	{
		return 1;
	}

	status = cz8rl1_write(command);

	if( status < 0 )
	{	/* err */
		switch(status){
		case CZ8RL1_ERR_NOTFOUND: str = "CZ8Rl1不在"; break;
		case CZ8RL1_ERR_BSYACK:   str = "BUSYがHIGHに応答しなかった"; break;
		case CZ8RL1_ERR_DOAPSS:   str = "APSS実行中です"; break;
		case CZ8RL1_ERR_NOHEADER: str = "STATUSのヘッダーが来ない";break;
		case CZ8RL1_ERR_BADHEAD:  str = "STATUSのヘッダーに長さが規定外";break;
		case CZ8RL1_ERR_BADBIT:   str = "データビットの長さが規定外";break;
		}
		printf("CZ8RL1送受信エラーです code=%d , %s\n",status,str);
		return 0;
	}
	if( status >= 0x80 && status <= 0x87 )
	{	/* sensor */
		printf("CZ8RL1センサー状態 TAPEEND=%d,WPROTECT=%d,WAPEIN=%d\n"
			,(status>>2)&1,(status>>1)&1,status&1);
		return 1;
	}
	switch(status)
	{
	case CZ8RL1_EJECT:
	case CZ8RL1_STOP:
	case CZ8RL1_PLAY:
	case CZ8RL1_FF:
	case CZ8RL1_REW:
	case CZ8RL1_AFF:
	case CZ8RL1_AREW:
	case CZ8RL1_REC:
		str = cz8rl1_cmd_str[status];
		ret = 1;
		break;
	case CZ8RL1_STS_DOAPSS:
		printf("CZ8RL1ステータス APSS実行中です\n");
		return;
	case 0xff:
		str = "BREAK";
		ret=1;
	}
	if( command == 2 || command == 10)
	{
		if( command != status)
		{
			printf("CZ8RL1制御に失敗しました 要求=%d ステータス=%d\n",command,status);
			ret = 0;
		}
	}
	printf("CZ8RL1ステータス code=%02xh(%s)\n",status,str);
	return ret;
}

/*-----------------------------------------------------------------
	INFORMATION
-----------------------------------------------------------------*/
static void show_tapfile_info(X1TAPFILE *fp)
{
	if( x1tap_get_type(fp) )
	{
		printf("type     :XMilleniumT-tune\n");
		printf("name     :%s\n",x1tap_get_name(fp));
		printf("protect  :%s\n",x1tap_get_protect(fp)? "on" : "off");
		printf("format   :%02x\n",x1tap_get_format(fp));
		printf("frequency:%ld Hz\n",x1tap_get_frequency(fp));
		printf("data size:%ld bits\n",x1tap_get_datasize(fp));
		printf("position :%ld bits\n",x1tap_get_position(fp));
	}
	else
	{
		printf("type     :X1EMU\n");
		printf("frequency:%ld Hz\n",x1tap_get_frequency(fp));
		printf("data size:%08ld bits\n",x1tap_get_datasize(fp));
	}
}

/*-----------------------------------------------------------------
	CZ8RL1 TOOLS
-----------------------------------------------------------------*/

/* CZ8RL1 READサンプリング */
static int cmt_read_tape(void)
{
	X1TAPFILE *fp;
	BYTE *buf;
	unsigned int memsize = samplerate*(maxtime*60)/8;
	int ret;
	unsigned int sampled_eddge;
	double sampled_jitter[2];
	unsigned int sampled_time , sampled_length;
	unsigned int cut_idle_time;

	printf("ＣＺ８ＲＬ１のテープからファイルに記録します\n\n");
	printf("ファイル名　　　　:%s\n",filename[0] ? filename:"調査のみ");
	printf("サンプリングレート:%dHz\n",samplerate);
	printf("最大時間　　　　　:%02d分\n",maxtime);
	printf("アジャストジッター:L=%3.2lf%%/H=%3.2lf%%\n",jitterl,jitterh);
	printf("無音自動停止時間　:%dsec\n",idlebreak);
	printf("自動停止時空白時間:%dsec\n",idletime);
	printf("バッファーサイズ　:%dKB\n",memsize/1024);

	/* buffer allocate */
	buf = (BYTE *)malloc(memsize);
	if( buf == NULL)
	{
		printf("バッファメモリの割当が出来ません\n");
		return -1;
	}
	memset(buf,0,memsize);

	/* file exist check */
	if(filename[0])
	{
		if( (fp = x1tap_open(filename)) != NULL )
		{
			printf("すでに'%s'が存在します\n",filename);
			x1tap_close(fp);
			return -1;
		}
	}

	/* setup params */
	sampled_length = memsize;
	sampled_jitter[0] = jitterl;
	sampled_jitter[1] = jitterh;

	printf("－－－－－－－－－－サンプリング開始－－－－－－－－－－\n");
	if( !cz8rl1_controll)
		printf("カセットを再生状態にしてから、");
	printf("何かキーを押してください\n");
	getch();
	/* PLAY */
	if( !cmt_controll(CZ8RL1_PLAY) )
		return -1;
	printf("終了するまでロックします。ＣＭＴを停止でブレークします。\n");

	fflush(stdout);
	tick_preset_basetime();
	tick_wait_usec(200000);

	/* sampling */
	ret = CZ8RL1_read_data(buf,&sampled_length,&sampled_eddge,
						cz8rl1_controll,samplerate,
						&sampled_jitter[0],&sampled_jitter[1],
						idlebreak);
	printf("－－－－－－－－－－サンプリング終了－－－－－－－－－－\n");
	switch(ret)
	{
	case CZ8RL1_ERR_SPEED1:
	case CZ8RL1_ERR_SPEED2:
		printf("処理速度エラーです、ＰＣの処理速度が遅すぎました\n");
		break;
	case CZ8RL1_STS_AUTOSTOP:
		printf("無音検出で自動停止しました\n");
		/* 自動停止の場合、無音部分を規定秒数残して削除 */
		if( idlebreak > idletime)
		{
			cut_idle_time = samplerate * (idlebreak-idletime); /* 削除する時間 */
			if( sampled_length*8 < cut_idle_time)
				sampled_length = 0;
			else
				sampled_length -= cut_idle_time/8;
		}
		break;
	case CZ8RL1_STS_BUFFULL:
		printf("最大時間経過\n");
		break;
	case CZ8RL1_STS_BREAK:
		printf("カセットが停止しました\n");
		break;
	default:
		printf("不明のリターンコード %d\n",ret);
	}
	/* CMT停止 */
	cmt_controll(CZ8RL1_STOP);
	/* サンプリング時間の計算 */
	sampled_time = sampled_length*8/samplerate;

	printf("サンプリング時間 %02d:%02d\n",sampled_time/60,sampled_time%60);
	printf("サンプリングサイズ %dKB\n",sampled_length/1024);
	printf("総エッジ数 %d\n",sampled_eddge);
	printf("推奨ジッター LOW=%3.2f%% HIGH=%3.2f%%\n",100-sampled_jitter[0]*100,100-sampled_jitter[1]*100);

	if(filename[0])
	{
		/* file write */
		fp = x1tap_create(filename,0);
		if(fp == NULL)
		{
			printf("ファイル'%s'がオープンできません\n",filename);
			return -1;
		}
		printf("ファイル書き込み中\n");
		/* set header */
		x1tap_set_frequency(fp,samplerate);
		if( x1tap_write(fp,buf,sampled_length) < sampled_length)
		{
			x1tap_close(fp);
			printf("ファイル'%s'にデータが書き込めません\n",filename);
			return -1;
		}
		x1tap_close(fp);
	}
	printf("完了！\n");

	return 0;
}

/* CZ8RL1 WRITEサンプリング */
int cmt_write_tape(void)
{
	X1TAPFILE *fp;
	BYTE *buf;
	unsigned int memsize;
	unsigned int rate , sec , bitlength;
	int ret;

	printf("ＣＺ８ＲＬ１のテープからファイルに記録します\n\n");
	if( !cz8rl1_controll)
	{
		printf("ロジックコントロールでないと実行できません\n");
		return 0;
	}

	printf("ファイル名　　　　:%s\n",filename);

	fp = x1tap_open(filename);
	if(fp == NULL)
	{
		printf("ファイル'%s'がオープンできないか、ＴＡＰファイルではありません\n",filename);
		return -1;
	}

	bitlength = fp->hdr.datasize;
	rate   = fp->hdr.frequency;
	sec    = bitlength / rate;
	memsize = (bitlength+7)/8;

	/* show status */
	show_tapfile_info(fp);
	printf("バッファサイズ: %d\n",memsize);
	printf("記録時間      : %2dm%02ds\n",sec/60,sec%60);
	printf("書き込み後空白:%dsec\n",idletime);

	buf = (BYTE *)malloc(memsize);
	if( buf == NULL)
	{
		printf("バッファメモリの割当が出来ません\n");
		x1tap_close(fp);
		return -1;
	}
	memset(buf,0,memsize);

	if( x1tap_read(fp,buf,memsize) <= 0)
	{
		printf("ファイル'%s'からリード出来ません\n",filename);
		x1tap_close(fp);
		return -1;
	}
	x1tap_close(fp);

	printf("－－－－－－－－－－書き出し開始－－－－－－－－－－\n");
	printf("何かキーを押してください\n");
	getch();
	if( !cmt_controll(CZ8RL1_REC) )
		return -1;
	printf("終了するまでロックします。ＣＭＴを停止でブレークします。\n");
	fflush(stdout);
	tick_preset_basetime();
	tick_wait_usec(200000);

	/* start outout */
	ret = cz8rl1_write_data(buf,&memsize,rate,cz8rl1_controll);
	switch(ret)
	{
	case CZ8RL1_STS_OK:
		printf("無音部分を%d秒間書き込んでいます。\n",idletime);
		tick_preset_basetime();
		tick_wait_usec((TICKER)idletime*1000000);
		printf("書き込み終了\n");
		break;
	case CZ8RL1_STS_BREAK:
		printf("ブレークされました\n");
		break;
	}
	/* 停止 */
	cmt_controll(CZ8RL1_STOP);
	return 0;
}


int main( int argc , char *argv[] )
{
	int i;
	int err = 0;
	char *argp;
	int status;
	/* default of option */
	int cmd = 0;
	int cmtcode = -1;
	long double fixed_tick_Mhz = 0;
	TICKER fixed_ticker = 	TICKER_AUTO_DITECT;

	/* port assign */
	int wp = 0x378;	/* OUTポート : LPT1 , データポート */
	int rp = 0x379;	/* INポート  : LPT1 , ステータスポート */
	ioperm(0x377, 3, 1);
	int wd = 0x3f;	/* DB1..DB4  : プルアップ電源供給、すべて正極性 */
	int ww = 0x20;	/* DB5       : WRITE DATA */
	int wc = 0x40;	/* DB6       : COMMAND DATA */
	int ws = 0x80;	/* DB7       : STROBE */
	int rd = 0x00;	/* in極性    : 入力はすべて正極性 */
	int rb = 0x40;	/* bit6(ACK) : BUSY */
	int rs = 0x20;	/* bit5(PE)  : STATUS */
	int rr = 0x10;	/* bit4(SEL)  : READ DATA */

	/* static options default */
	filename[0] = 0x00;
	samplerate=8000;	/* sampling rate */
	maxtime   = 60;		/* max sampling time */
	idlebreak = 15;		/* auto ditect / idle after write */
	idletime  = 0;		/* idle time of auto ditect / after write */
	jitterl = 50;		/* jitter low 50% */
	jitterh = 50;		/* jitter high 50% */
	cz8rl1_controll = 1;/* CZ8RL1 full logic controll on */

	/* command code */
	if( argc<2)
		err = 1;
	else
		cmd = argv[1][0] | 0x20;

	/* option anaryze */
	for(i=2; i<argc && !err; i++){
		argp = argv[i];
		if( *argp =='/' || *argp=='-')
		{	/* option switch */
			argp++;
			switch(*argp++ | 0x20)
			{
			case 's':
				if(sscanf(argp,"%d",&samplerate)<1)
					err=1;
				break;
			case 't':
				if(sscanf(argp,"%d,%d",&maxtime)<1)
					err=1;
				break;
			case 'p': /* I/O port select */
				if(sscanf(argp,"%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
				&wp,&wd,&ww,&wc,&rp,&ws,&rd,&rb,&rs,&rr) == 1)
				{
					rp = wp+0x01;
				}
			case 'j': /* set jitter / adjust rate */
				if(sscanf(argp,"%lf,%lf",&jitterl,&jitterh)<2)
					err=1;
				break;
			case 'k': /* idle break time / write idle time */
				if(sscanf(argp,"%d",&idlebreak)<1)
					err=1;
				break;
			case 'i': /* idle break time / write idle time */
				if(sscanf(argp,"%d",&idletime)<1)
					err=1;
				break;
			case 'n': /* no CMT controll mode */
				cz8rl1_controll = 0;
				break;
			case 'c': /* fixed ticker */
				if(sscanf(argp,"%Lf",&fixed_tick_Mhz)<1 && fixed_tick_Mhz <=0 )
					err=1;
				else
				{
					fixed_ticker = (TICKER)(1000000*fixed_tick_Mhz);
					/* 100MHz - 10GKz */
					if( fixed_ticker < 100000000 || fixed_ticker > 10000000000)
						err = 1;
				}
				break;
			default:
				err = 1;
			}
		}
		else
		{
			if(filename[0] == 0)
				strcpy(filename,argp);
			else
				err=1;
		}
	}
	switch( cmd )
	{
	case 'r':
		if(idletime==0)
			idletime = 3;
		if(!samplerate || !maxtime || idlebreak < idletime)
		 	err = 1;
		break;
	case 'w':
		if(idletime==0)
			idletime = 15;
		if(!filename[0])
		 	err = 1;
		break;
	case 'c':
		if(filename[0])
			if(sscanf(filename,"%x",&cmtcode)<1)
				err=1;
		break;
	case 'e':
		printf("this version is not supported emulation mode\n");
		err = 1;
		break;
	default:
		err = 1;
	}

	if( err == 1)
	{
		printf(" CZ8RL1 Controller Ver.0.1\n");
		printf("  usage : X8RL1.EXE [command] [-option] [filename/code(hex)]\n\n");
		printf("  command:\n");
		printf("    R : read TAPE data from CZ8RL1 to write imagefile\n");
		printf("    W : read imagefile to write TAPE data to CZ8RL1\n");
		printf("    C : controll CZ8RL1\n");
		printf("    E : emulate CZ8RL1 conect to X1 CMT connector\n\n");
		printf("  option:              |cmd|deflt  | note");
		printf("    -c[cpu_speed_MHz]  |all|(auto) | use fixed 'rdttsc' cycles (no auto ditect)\n");
		printf("    -p[wp][,wd,ww,wc,ws|all|(LPT1) | I/O port,bit assign,and active level\n");
		printf("      ,rp,rd,rb,rs,rr]\n");
		printf("    -n                 |all|(ctrl) | ignore CZ8RL1 controll (for simple cable)\n");
		printf("    -s[rate_Hz]        | R | 8000  | sampling and imagefile frequency\n");
		printf("    -t[time_min]       | R | 60    | max sampling time\n");
		printf("    -j[hi(%%)],[lo(%%)]  | R | 50,50 | jitter rate of LO/HI eddge adjuster%%\n");
		printf("                                   | 0 = no adjust when eddge found\n");
		printf("    -k[sec]            | R | 15    | auto stop ditect time\n");
		printf("    -i[sec]            | RW| R3,W15| idle time, (R)after autoditect,(W)after write\n");
		return err;
	}

	/* ticker初期化*/
	if( fixed_ticker == TICKER_AUTO_DITECT)
		printf("基準クロック(rdtsc)計測中");
	if( !init_ticker(fixed_ticker) )
	{
		printf("rdtsc命令が使えないこのＣＰＵはサポートされていません\n");
		return 1;
	}
	printf("周波数は%4dMHzですね\n",ticks_per_sec/1000000);

	/* ポート初期化 */
	printf("ＣＺ８ＲＬ１　Ｉ／Ｆポート初期化 out_address=%04x , in_address =%04x\n",wp,rp);
	if( !CZ8RL1_init(wp,wd,ww,wc,ws,rp,rd,rb,rs,rr) )
	{
		printf("CZ8RL1が見つかりません\n");
		return 1;
	}
	if( !cmt_controll(CZ8RL1_STATUS) || !cmt_controll(CZ8RL1_SENSOR) )
	{
		printf("CZ8RL1と通信できません\n");
		return;
	}

	switch( cmd )
	{
	case 'r':
		cmt_read_tape();
		break;
	case 'w':
		cmt_write_tape();
		break;
	case 'e':
		break;
	case 'c':
		/* CMTコード送信！ */
		if( cmtcode >=0)
		{
			printf("コマンド送信開始 code=%02xh\n",cmtcode);
			cmt_controll(cmtcode);
		}
		break;
	case 'l':
		/* loop */
		cmtcode = 0x80;
		do
		{
			status = cmt_controll(0x80);
			cmtcode ^= 1;
		}while(status!=0);
	}
	return 0;
}
