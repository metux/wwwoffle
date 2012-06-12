/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/iozlib.c 1.23 2006/07/21 17:35:48 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Functions for file input and output with compression.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>

#if USE_ZLIB
#include <zlib.h>
#endif

#include "io.h"
#include "iopriv.h"
#include "errors.h"


#if USE_ZLIB

/*+ The gzip header required to be output for compression +*/
static const unsigned char gzip_head[10]={0x1f,0x8b,8,0,0,0,0,0,0,0xff};


/* Local functions */

static void guess_init_zlib_uncompress(io_zlib *context,io_buffer *in);

static int parse_gzip_head(io_zlib *context,const char *buffer,int n);
static int parse_gzip_tail(io_zlib *context,const char *buffer,int n);

static void set_zerror(const char *msg);


/*--------------------------------------------------------------------------------

  The use of the deflate Transfer-Encoding is not recommended because there are
  two ways that it is implemented in web servers and clients.

  The HTTP/1.1 specification, RFC 2616 says:

       deflate
            The "zlib" format defined in RFC 1950 in combination with
            the "deflate" compression mechanism described in RFC 1951.

  This requires that there is the 2 byte zlib header and the 4 byte Adler
  checksum, like gzip where the deflated data is enclosed in a gzip specific
  header and trailer (RFC 1952).

  In practice however the most common (only?) implementation that is found is
  that the zlib header and trailer (RFC 1950) are missing and just the deflated
  data (RFC 1951) is sent.

  To handle the data from the majority of the web servers WWWOFFLE must attempt
  to guess the actual format from the data.  The gzip format (RFC 1952) can be
  detected by unique values in the first three bytes.  The zlib format (RFC
  1950) can be detected by the internal checksum of the first two bytes.  The
  deflate format (RFC 1951) cannot be detected so is the fallback option.

  It is not unknown for servers to send data and say that it uses deflate and
  actually it looks like gzip but cannot be uncompressed.

  WWWOFFLE itself will not send data to a client using the deflate transfer
  encoding unless the client only asks for deflate and not gzip.  In this case
  WWWOFFLE will send deflate (RFC 1951) and not zlib (RFC 1950) like HTTP/1.1
  says that it should.

--------------------------------------------------------------------------------*/


/*++++++++++++++++++++++++++++++++++++++
  Initialise the compression context information.

  io_zlib *io_init_zlib_compress Returns a new compression context.

  int type Set to 1 for zlib/deflate or 2 for gzip.
  ++++++++++++++++++++++++++++++++++++++*/

io_zlib *io_init_zlib_compress(int type)
{
 io_zlib *context=(io_zlib*)calloc((size_t)1,sizeof(io_zlib));

 context->type=1950+type; /* use the RFC number 0=zlib, 1=deflate, 2=gzip */

 if(context->type==1950) /* zlib */
    io_errno=deflateInit(&context->stream,Z_DEFAULT_COMPRESSION);
 else
    io_errno=deflateInit2(&context->stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,MAX_MEM_LEVEL,Z_DEFAULT_STRATEGY);

 if(io_errno!=Z_OK)
   {
    set_zerror(context->stream.msg);
    free(context);
    return(NULL);
   }

 if(context->type==1952) /* gzip */
   {
    context->insert_head=1;

    context->crc=crc32(0L,Z_NULL,0);
   }

 return(context);
}


/*++++++++++++++++++++++++++++++++++++++
  Initialise the uncompression context information.

  io_zlib *io_init_zlib_uncompress Returns a new uncompression context.

  int type Set to 1 for zlib/deflate or 2 for gzip.
  ++++++++++++++++++++++++++++++++++++++*/

io_zlib *io_init_zlib_uncompress(int type)
{
 io_zlib *context=(io_zlib*)calloc((size_t)1,sizeof(io_zlib));

 context->type=1950+type; /* use the RFC number 0=zlib, 1=deflate, 2=gzip */

 /* The initialisation is actually done in guess_init_zlib_uncompress() because
    the specified compression method may be wrong. */

 return(context);
}


/*++++++++++++++++++++++++++++++++++++++
  Initialise the uncompression context information based on guessing the real data format.

  io_zlib *context The context to initialise.

  io_buffer *in The first few bytes of data.
  ++++++++++++++++++++++++++++++++++++++*/

static void guess_init_zlib_uncompress(io_zlib *context,io_buffer *in)
{
 unsigned char *bytes=(unsigned char*)in->data;
 int type=1951;

 /* Guess the format of the data */

 if(in->length>=12 && bytes[0]==0x1f && bytes[1]==0x8b && bytes[2]==0x08) /* gzip */
   {
    if(context->type==1951 &&
       bytes[3]==0 && bytes[4]==0 && bytes[5]==0 &&
       bytes[6]==0 && bytes[7]==0 && bytes[8]==0 && /* bytes[9]==? && */
       (((((unsigned)bytes[10])<<8)+(unsigned)bytes[11])%31==0)
       && (bytes[10]&0x0f)==8) /* PHP v4 is broken */
       type=1950+1952;
    else
       type=1952;
   }
 else if(in->length>=2 && ((((unsigned)bytes[0])<<8)+(unsigned)bytes[1])%31==0
         && (bytes[0]&0x0f)==8) /* zlib */
    type=1950;

 if(type!=context->type)
   {
    if(type==(1950+1952))
       PrintMessage(Debug,"Compressed data was expected to be RFC %d but looks like RFC 1950 + RFC 1952.",context->type);
    else
       PrintMessage(Debug,"Compressed data was expected to be RFC %d but looks like RFC %d.",context->type,type);

    if(StderrLevel==ExtraDebug)
      {
       char hexdata[16*3+1];
       int i;

       for(i=0;i<16 && i<in->length;i++)
          sprintf(&hexdata[i*3]," %02x",(unsigned int)bytes[i]);

       PrintMessage(ExtraDebug,"Compressed data was %s.",hexdata+1);
      }

    if(type==(1950+1952))
      {
       memmove(in->data,in->data+10,in->length-10);
       in->length-=10;
       type=1950;
      }
   }

 /* Now do what should have been done in io_init_zlib_uncompress() */

 context->type=type;

 if(context->type==1950) /* zlib */
    io_errno=inflateInit(&context->stream);
 else
    io_errno=inflateInit2(&context->stream,-MAX_WBITS);

 if(io_errno!=Z_OK)
   {
    set_zerror(context->stream.msg);
    return;
   }

 if(context->type==1952) /* gzip */
   {
    context->crc=crc32(0L,Z_NULL,0);

    context->doing_head=1;
    context->doing_body=0;
   }
 else
   {
    context->doing_head=0;
    context->doing_body=1;
   }

 context->doing_tail=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from an input buffer and compress it to an output buffer.

  int io_zlib_compress Returns 0 normally, 1 if finished or negative for error.

  io_buffer *in The input buffer.

  io_zlib *context The zlib context information.

  io_buffer *out The output buffer.
  ++++++++++++++++++++++++++++++++++++++*/

int io_zlib_compress(io_buffer *in,io_zlib *context,io_buffer *out)
{
 if(context->insert_head)
   {
    if((out->size-out->length)<sizeof(gzip_head))
       return(0);

    memcpy(out->data+out->length,gzip_head,sizeof(gzip_head));

    out->length+=sizeof(gzip_head);

    context->insert_head=0;
   }

 context->stream.avail_in=in->length;
 context->stream.next_in=(unsigned char*)in->data;

 context->stream.next_out=(unsigned char*)out->data+out->length;
 context->stream.avail_out=out->size-out->length;

 io_errno=deflate(&context->stream,Z_NO_FLUSH);

 if(io_errno!=Z_STREAM_END && io_errno!=Z_OK)
   {
    set_zerror(context->stream.msg);
    return(-1);
   }

 if(context->stream.avail_in!=in->length)
   {
    if(context->type==1952) /* gzip */
       context->crc=crc32(context->crc,(unsigned char*)in->data,in->length-context->stream.avail_in);

    if(context->stream.avail_in>0)
       memmove(in->data,in->data+(in->length-context->stream.avail_in),context->stream.avail_in);

    in->length=context->stream.avail_in;
   }

 out->length=out->size-context->stream.avail_out;

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Read some data from an input buffer and decompress it to an output buffer.

  int io_zlib_uncompress Returns 0 normally, 1 if finished or negative for error.

  io_buffer *in The input buffer.

  io_zlib *context The zlib context information.

  io_buffer *out The output buffer.
  ++++++++++++++++++++++++++++++++++++++*/

int io_zlib_uncompress(io_buffer *in,io_zlib *context,io_buffer *out)
{
 size_t nused=0;

 /* If zlib is not initialised then gather bytes and try to guess. */

 if(!context->stream.state)
   {
    if(in->length==0)
      return(1);

    if(in->length<=12 && in->length!=context->lastlen)
      {
       context->lastlen=in->length;
       return(0);
      }

    guess_init_zlib_uncompress(context,in);

    if(!context->stream.state)
      {io_errno=-1;set_zerror("cannot initialise zlib uncompression");return(-1);}
   }

 /* Process the head, tail or body. */

 if(context->doing_head) /* gzip */
   {
    int nb=parse_gzip_head(context,in->data,in->length);

    if(nb<0) /* error */
       return(-1);
    else if(nb==in->length) /* read all bytes, need more */
      {
       in->length=0;
       return(0);
      }
    else /* bytes remaining, uncompress them */
       nused=nb;
   }
 else if(context->doing_tail) /* gzip */
   {
    int nb=parse_gzip_tail(context,in->data,in->length);

    if(nb<0) /* error */
       return(-1);
    else if(nb==in->length) /* read all bytes, need more */
      {
       in->length=0;
       return(0);
      }
    else /* bytes remaining, junk */
      {
       in->length=0;
       return(1);
      }
   }
 else if(!context->doing_body)
    return(1);

 context->stream.next_out=(unsigned char*)out->data+out->length;
 context->stream.avail_out=out->size-out->length;

 context->stream.next_in=(unsigned char*)in->data+nused;
 context->stream.avail_in=in->length-nused;

 io_errno=inflate(&context->stream,Z_SYNC_FLUSH);

 if(io_errno!=Z_STREAM_END && io_errno!=Z_OK)
   {
    set_zerror(context->stream.msg);
    return(-1);
   }

 if(io_errno==Z_STREAM_END)
   {
    context->doing_body=0;
    if(context->type==1952) /* gzip */
       context->doing_tail=1;
   }

 if(context->type==1952) /* gzip */
    context->crc=crc32(context->crc,(unsigned char*)out->data+out->length,out->size-context->stream.avail_out);

 if(context->stream.avail_in>0)
    memmove(in->data,in->data+(in->length-context->stream.avail_in),context->stream.avail_in);

 in->length=context->stream.avail_in;

 out->length+=out->size-context->stream.avail_out;

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Finalise the compression data stream and output all remaining bytes.

  int io_finish_zlib_compress Returns 0 on completion, 1 if there is more data, negative if error.

  io_zlib *context The zlib context information.

  io_buffer *out The final output buffer to fill with tail data.
  ++++++++++++++++++++++++++++++++++++++*/

int io_finish_zlib_compress(io_zlib *context,io_buffer *out)
{
 /* Finish deflating the buffer and writing it. */

 context->stream.avail_in=0;
 context->stream.next_in=(unsigned char*)"";

 context->stream.next_out=(unsigned char*)out->data+out->length;
 context->stream.avail_out=out->size-out->length;

 io_errno=deflate(&context->stream,Z_FINISH);

 out->length=out->size-context->stream.avail_out;

 if(context->stream.avail_out==0)
    return(1);

 if(context->type==1952) /* gzip */
   {
    int i;
    char *gz_tail;

    if((out->size-out->length)<8)
       return(1);

    gz_tail=out->data+out->length;

    for(i=0;i<4;i++)
      {
       gz_tail[i]=context->crc&0xff;
       context->crc>>=8;
       gz_tail[i+4]=context->stream.total_in&0xff;
       context->stream.total_in>>=8;
      }

    out->length+=8;
   }

 io_errno=deflateEnd(&context->stream);

 if(io_errno!=Z_OK)
   {
    set_zerror(context->stream.msg);
    free(context);
    return(-1);
   }

 free(context);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Finalise the uncompression data stream and output all remaining bytes.

  int io_finish_zlib_uncompress Returns 0 on completion, 1 if there is more data, negative if error.

  io_zlib *context The zlib context information.

  io_buffer *out The final output buffer to fill with tail data.
  ++++++++++++++++++++++++++++++++++++++*/

int io_finish_zlib_uncompress(io_zlib *context,/*@unused@*/ io_buffer *out)
{
 io_errno=inflateEnd(&context->stream);

 if(io_errno!=Z_OK)
   {
    set_zerror(context->stream.msg);
    free(context);
    return(-1);
   }

 free(context);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse a gzip header.

  int parse_gzip_head Returns the amount of the buffer consumed (not all if head finished).

  io_zlib *context The zlib context information.

  const char *buffer The buffer of new data.

  int n The amount of new data.
  ++++++++++++++++++++++++++++++++++++++*/

static int parse_gzip_head(io_zlib *context,const char *buffer,int n)
{
 const unsigned char *p0=(unsigned char*)buffer,*p=(unsigned char*)buffer;

 if(n==0)
   {io_errno=-1;set_zerror("truncated gzip header");return(-1);}

 while(n>0)
   {
    switch(context->doing_head)
      {
      case 1:                   /* magic byte 1 */
       if(*p!=0x1f)
         {io_errno=-1;set_zerror("not gzip format");return(-1);}
       context->doing_head++;n--;p++;
       break;
      case 2:                   /* magic byte 2 */
       if(*p!=0x8b)
         {io_errno=-1;set_zerror("not gzip format");return(-1);}
       context->doing_head++;n--;p++;
       break;
      case 3:                   /* method byte */
       if(*p!=Z_DEFLATED)
         {io_errno=-1;set_zerror("not gzip format");return(-1);}
       context->doing_head++;n--;p++;
       break;
      case 4:                   /* flag byte */
       context->head_flag=*p;
       context->doing_head++;n--;p++;
       break;
      case 5:                   /* time byte 0 */
      case 6:                   /* time byte 1 */
      case 7:                   /* time byte 2 */
      case 8:                   /* time byte 3 */
      case 9:                   /* xflags */
      case 10:                  /* os flag */
       context->doing_head++;n--;p++;
       break;
      case 11:                  /* extra field length byte 1 */
       if(!(context->head_flag&0x04))
         {context->doing_head=14;break;}
       context->head_extra_len=*p;
       context->doing_head++;n--;p++;
       break;
      case 12:                  /* extra field length byte 2 */
       context->head_extra_len+=*p<<8;
       context->doing_head++;n--;p++;
       break;
      case 13:                  /* extra field bytes */
       if(context->head_extra_len==0)
          context->doing_head++;
       context->head_extra_len--;
       n--;p++;
       break;
      case 14:                  /* orig name bytes */
       if(!(context->head_flag&0x08))
         {context->doing_head++;break;}
       if(*p==0)
          context->doing_head++;
       n--;p++;
       break;
      case 15:                  /* comment bytes */
       if(!(context->head_flag&0x10))
         {context->doing_head++;break;}
       if(*p==0)
          context->doing_head++;
       n--;p++;
       break;
      case 16:                  /* head crc byte 1 */
       if(!(context->head_flag&0x02))
         {context->doing_head=0;break;}
       context->doing_head++;n--;p++;
       break;
      case 17:                  /* head crc byte 2 */
       context->doing_head=0;n--;p++;
       break;
      case 0:                   /* finished */
       n=0;
      }
   }

 if(context->doing_head==0)
    context->doing_body=1;

 return(p-p0);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse a gzip tail and check the checksum.

  int parse_gzip_tail Returns the amount of the buffer consumed (not all if tail finished).

  io_zlib *context The zlib context information.

  const char *buffer The buffer of new data.

  int n The amount of new data.
  ++++++++++++++++++++++++++++++++++++++*/

static int parse_gzip_tail(io_zlib *context,const char *buffer,int n)
{
 const unsigned char *p0=(unsigned char*)buffer,*p=(unsigned char*)buffer;

 if(n==0)
   {io_errno=-1;set_zerror("truncated gzip tail");return(-1);}

 while(n>0)
   {
    switch(context->doing_tail)
      {
      case 1:                   /* crc byte 1 */
       context->tail_crc=(unsigned long)*p;
       context->doing_tail++;n--;p++;
       break;
      case 2:                   /* crc byte 2 */
       context->tail_crc+=(unsigned long)*p<<8;
       context->doing_tail++;n--;p++;
       break;
      case 3:                   /* crc byte 3 */
       context->tail_crc+=(unsigned long)*p<<16;
       context->doing_tail++;n--;p++;
       break;
      case 4:                   /* crc byte 4 */
       context->tail_crc+=(unsigned long)*p<<24;
       context->doing_tail++;n--;p++;
       if(context->tail_crc!=context->crc)
         {io_errno=-1;set_zerror("gzip crc error");return(-1);}
       break;
      case 5:                   /* length byte 1 */
       context->tail_len=(unsigned long)*p;
       context->doing_tail++;n--;p++;
       break;
      case 6:                   /* length byte 2 */
       context->tail_len+=(unsigned long)*p<<8;
       context->doing_tail++;n--;p++;
       break;
      case 7:                   /* length byte 3 */
       context->tail_len+=(unsigned long)*p<<16;
       context->doing_tail++;n--;p++;
       break;
      case 8:                   /* length byte 4 */
       context->tail_len+=(unsigned long)*p<<24;
       context->doing_tail=0;n--;p++;
       if(context->tail_len!=context->stream.total_out)
         {io_errno=-1;set_zerror("gzip length error");return(-1);}
       break;
      case 0:                   /* finished */
       n=0;
      }
   }

 return(p-p0);
}


/*++++++++++++++++++++++++++++++++++++++
  Set the error status when there is a compression error.

  const char *msg The error message.
  ++++++++++++++++++++++++++++++++++++++*/

static void set_zerror(const char *msg)
{
 if(!msg)
    msg="Unknown error";

 errno=ERRNO_USE_IO_ERRNO;

 if(io_strerror)
    free(io_strerror);
 io_strerror=(char*)malloc(16+strlen(msg)+1);

 sprintf(io_strerror,"IO(zlib): %s",msg);
}

#endif /* USE_ZLIB */
