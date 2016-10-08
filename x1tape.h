#ifndef __X1_TAP__
#define __X1_TAP__

/* .TAP file format */
#define TAPE_INDEX   0x45504154   /* index (for LSB first model) */
#define TAPE_PROTECT 0x10         /* write protect               */
#define TAPE_FORMAT_SAMPLING 0x01 /* normal sampling format      */
typedef struct X1TapeHeaderRecoard
{
  unsigned long magic;       /* 00H:���ʃC���f�b�N�X "TAPE"            */
                             /*     X1EMU�̏ꍇ�A���g���l              */
  char name[17];             /* 04H:�e�[�v�̖��O(asciiz)               */
  unsigned char reserve[5];  /* 15H:���U�[�u                           */
  unsigned char protect;     /* 1AH:���C�g�v���e�N�g�m�b�`             */
                             /*     (00H=�������݉A10H=�������݋֎~�j*/
  unsigned char format;      /* 1BH:�t�H�[�}�b�g�̎�� �����Ή�        */
                             /*    �i01H=�葬�T���v�����O���@�j        */
  unsigned long frequency;   /* 1CH:�T���v�����O���g��(�g���P�ʁj      */
  unsigned long datasize;    /* 20H:�e�[�v�f�[�^�̃T�C�Y�i�r�b�g�P�ʁj */
  unsigned long position;    /* 24H:�e�[�v�̈ʒu�i�r�b�g�P�ʁj         */
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
