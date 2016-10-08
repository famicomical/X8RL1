/*
	X1EMU / Xmillennium T_TUNE
	テープアクセスライブラリ

	LittleEndian Model ONLY!
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "x1tape.h"

int fsize(FILE *fp){
    int prev=ftell(fp);
    fseek(fp, 0L, SEEK_END);
    int sz=ftell(fp);
    fseek(fp,prev,SEEK_SET); //go back to where we were
    return sz;
}

X1TAPFILE *x1tap_open(char *filename)
{
	FILE *handle;
	X1TAPFILE *tp = NULL;
	int err = 0;
	unsigned long freq;
	X1TAPE_HEADER header;

	/* open file */
	handle = fopen(filename,"rb");
	if( handle == NULL)
		return NULL;
	/* allocate memory */
	tp = (X1TAPFILE *)malloc(sizeof(X1TAPFILE));
	if( tp == NULL)
	{
		fclose(handle);
		return NULL;
	}
	memset(tp,0,sizeof(X1TAPFILE));
	tp->handle = handle;
	/* read top 4 byte */
	if( fread(&tp->hdr.magic,1,sizeof(long),handle) == 4)
	{
		if( tp->hdr.magic != TAPE_INDEX)
		{	/* no MAGIC CODE , try to X1EMU format */
			/* sampling rate 1KHz to 200KHz ? */
			if( (tp->hdr.magic) >= 1000 && (tp->hdr.magic) <= 200000 )
			{	/* OK! , header convert */
				strncpy(tp->hdr.name,"X1EMU TAPE",sizeof(tp->hdr.name));
				tp->hdr.format = 0x01;
				tp->hdr.frequency = tp->hdr.magic;
				tp->hdr.datasize  = fsize(handle)*8;
				if( tp->hdr.datasize  != 0xffffffff)
					tp->hdr_size = 4;
			}
		}
		else
		{	/* found magic , Xmillenium t-tune format */
			if( fread(&tp->hdr.name,1,sizeof(X1TAPE_HEADER)-sizeof(long),handle) == sizeof(X1TAPE_HEADER)-sizeof(long))
				tp->hdr_size = sizeof(X1TAPE_HEADER);
		}
	}
	if( tp->hdr_size == 0 )
	{
		fclose(handle);
		free(tp);
		return NULL;
	}
	return tp;
}

X1TAPFILE *x1tap_create(char *filename,int type)
{
	FILE *handle;
	X1TAPFILE *tp = NULL;
	X1TAPE_HEADER header;

	/* open file */
	handle = fopen(filename,"wb");
	if( handle == NULL)
		return NULL;
	/* allocate memory */
	tp = (X1TAPFILE *)malloc(sizeof(X1TAPFILE));
	if( tp == NULL)
	{
		fclose(handle);
		return NULL;
	}
	memset(tp,0,sizeof(X1TAPFILE));
	tp->handle = handle;

	/* build header */
	if(type)
	{
		tp->hdr_size = sizeof(X1TAPE_HEADER);
		tp->hdr.magic = TAPE_INDEX;
		strncpy(tp->hdr.name,"XMIL T-TUNE",sizeof(tp->hdr.name));
		tp->hdr.format = 0x01;
		tp->hdr.frequency = 8000; /* 8KHz */
	}
	else
	{
		tp->hdr_size = 4;
		tp->hdr.magic = 8000;
		strncpy(tp->hdr.name,"X1EMU TAPE",sizeof(tp->hdr.name));
		tp->hdr.format = 0x01;
		tp->hdr.frequency = tp->hdr.magic;
	}
	/* write header */
	if(fwrite(&tp->hdr,1,tp->hdr_size,handle) != tp->hdr_size)
	{
		fclose(handle);
		free(tp);
		tp =NULL;
	}
	return tp;
}

long x1tap_filelength(X1TAPFILE *fp)
{
	long len = fsize(fp->handle);
	return len < fp->hdr_size ? -1 : len-fp->hdr_size;
}

int x1tap_seek(X1TAPFILE *fp,long pos)
{
	return fseek(fp->handle,pos+fp->hdr_size,SEEK_SET);
}

long x1tap_tell(X1TAPFILE *fp)
{
	long pos = ftell(fp->handle);
	return pos < fp->hdr_size ? -1 : pos-fp->hdr_size;
}

int x1tap_read(X1TAPFILE *fp,void *buf,unsigned int count)
{
	return fread(buf,1,count,fp->handle);
}

int x1tap_write(X1TAPFILE *fp,void *buf,unsigned int count)
{
	return fwrite(buf,1,count,fp->handle);
}

int x1tap_close(X1TAPFILE *fp)
{
	int ret = 0;
	long pos = 0;

	/* update datasize if filesize over */
	if( fp->hdr.datasize < x1tap_filelength(fp)*8 )
		fp->hdr.datasize = x1tap_filelength(fp)*8;
	/* X1EMU convert */
	if( fp->hdr_size == 4)
		fp->hdr.magic = fp->hdr.frequency;
	/* write header */
	if( (fseek(fp->handle,0,SEEK_SET) != 0) ||
		(fwrite(&fp->hdr,1,fp->hdr_size,fp->handle) != fp->hdr_size) )
		ret = -1;
	fclose(fp->handle);
	free(fp);
	return ret;
}
