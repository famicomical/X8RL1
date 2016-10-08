#ifndef __X1_TAP__
#define __X1_TAP__

/* .TAP file format */
#define TAPE_INDEX   0x45504154   /* index (for LSB first model) */
#define TAPE_PROTECT 0x10         /* write protect               */
#define TAPE_FORMAT_SAMPLING 0x01 /* normal sampling format      */
typedef struct X1TapeHeaderRecoard
{
  unsigned long magic;       /* 00H:識別インデックス "TAPE"            */
                             /*     X1EMUの場合、周波数値              */
  char name[17];             /* 04H:テープの名前(asciiz)               */
  unsigned char reserve[5];  /* 15H:リザーブ                           */
  unsigned char protect;     /* 1AH:ライトプロテクトノッチ             */
                             /*     (00H=書き込み可、10H=書き込み禁止）*/
  unsigned char format;      /* 1BH:フォーマットの種類 ※未対応        */
                             /*    （01H=定速サンプリング方法）        */
  unsigned long frequency;   /* 1CH:サンプリング周波数(Ｈｚ単位）      */
  unsigned long datasize;    /* 20H:テープデータのサイズ（ビット単位） */
  unsigned long position;    /* 24H:テープの位置（ビット単位）         */
}X1TAPE_HEADER;

typedef struct X1TapeFileHandle
{
	FILE *handle;		/* file handle */
	long hdr_size;
	X1TAPE_HEADER hdr;	/* header      */
}X1TAPFILE;

/* read macros */
#define x1tap_get_type(fp)      (fp->hdr_size==sizeof(X1TAPFILE))
#define x1tap_get_name(fp)      (fp->hdr.name)
#define x1tap_get_protect(fp)   (fp->hdr.protect)
#define x1tap_get_format(fp)    (fp->hdr.format)
#define x1tap_get_frequency(fp) (fp->hdr.frequency)
#define x1tap_get_datasize(fp)  (fp->hdr.datasize)
#define x1tap_get_position(fp)  (fp->hdr.position)

/* write macros */
#define x1tap_set_name(fp,val)      strncpy(fp->hdr.name,val,sizeof(tp->hdr.name))
#define x1tap_set_protect(fp,val)   { fp->hdr.protect = val?0x10:0; }
#define x1tap_set_frequency(fp,val) { fp->hdr.frequency = val; }
#define x1tap_set_datasize(fp,val)  { fp->hdr.datasize = val; }
#define x1tap_set_position(fp,val)  { fp->hdr.position = val; }

X1TAPFILE *x1tap_open(char *filename);
X1TAPFILE *x1tap_create(char *filename,int type);
long x1tap_filelength(X1TAPFILE *fp);
int x1tap_seek(X1TAPFILE *fp,long pos);
long x1tap_tell(X1TAPFILE *fp);
int x1tap_read(X1TAPFILE *fp,void *buf,unsigned int count);
int x1tap_write(X1TAPFILE *fp,void *buf,unsigned int count);
int x1tap_close(X1TAPFILE *fp);

#endif
