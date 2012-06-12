/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/iochunk.c 1.17 2005/11/10 19:50:30 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions for file input and output with compression.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>

#include "io.h"
#include "iopriv.h"
#include "errors.h"


/*+ The minimum chunk size to output (use same size as WRITE_BUFFER_SIZE).. +*/
#define MIN_CHUNK_SIZE (IO_BUFFER_SIZE/4)


/* Local functions */

static int parse_chunk_head(io_chunk *context,const char *buffer,int n);
static int parse_chunk_trailer(io_chunk *context,const char *buffer,int n);

static void set_error(const char *msg);


/*++++++++++++++++++++++++++++++++++++++
  Initialise the chunked-encoding context information.

  io_chunk *io_init_chunk_encode Returns a new chunked encoding context.

  io_chunk **new_context The new chunk context information to create.
  ++++++++++++++++++++++++++++++++++++++*/

io_chunk *io_init_chunk_encode(void)
{
 io_chunk *context=(io_chunk*)calloc((size_t)1,sizeof(io_chunk));

 context->chunk_buffer=create_io_buffer(MIN_CHUNK_SIZE);

 return(context);
}


/*++++++++++++++++++++++++++++++++++++++
  Initialise the chunked-decoding context information.

  io_chunk *io_init_chunk_decode Returns a new chunked decoding context.

  io_chunk **new_context The new chunk context information to create.
  ++++++++++++++++++++++++++++++++++++++*/

io_chunk *io_init_chunk_decode(void)
{
 io_chunk *context=(io_chunk*)calloc((size_t)1,sizeof(io_chunk));

 context->doing_head=3;         /* already skipped the first CRLF */

 return(context);
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from an input buffer and perform chunked encoding to an output buffer.

  int io_chunk_encode Returns 0 normally, 1 if finished or negative for error.

  io_buffer *in The input buffer, or NULL to flush the internal buffer.

  io_chunk *context The chunk context information.

  io_buffer *out The output buffer.
  ++++++++++++++++++++++++++++++++++++++*/

int io_chunk_encode(io_buffer *in,io_chunk *context,io_buffer *out)
{
 size_t chunk_size;

 chunk_size=context->chunk_buffer->length;

 if(in)
    chunk_size+=in->length;

 /* If insufficient data buffer internally */

 if(in && chunk_size<MIN_CHUNK_SIZE)
   {
    memcpy(context->chunk_buffer->data+context->chunk_buffer->length,in->data,in->length);
    context->chunk_buffer->length+=in->length;
    in->length=0;

    return(0);
   }

 /* Don't output a chunk smaller than the minimum. */

 if((out->size-out->length)<(chunk_size+16))
    chunk_size=(out->size-out->length)-16;

 if(in && chunk_size<MIN_CHUNK_SIZE)
    return(0);

 out->length+=sprintf(out->data+out->length,"%x\r\n",(unsigned)chunk_size);

 /* Output all of the internal buffer if possible. */

 if(context->chunk_buffer->length>0 && context->chunk_buffer->length<=chunk_size)
   {
    memcpy(out->data+out->length,context->chunk_buffer->data,context->chunk_buffer->length);
    out->length+=context->chunk_buffer->length;
    chunk_size-=context->chunk_buffer->length;
    context->chunk_buffer->length=0;
   }

 /* If there is more to output then use the input buffer */

 if(in && chunk_size>0 && context->chunk_buffer->length==0)
   {
    memcpy(out->data+out->length,in->data,chunk_size);
    out->length+=chunk_size;

    if(chunk_size<in->length)
       memmove(in->data,in->data+chunk_size,in->length-chunk_size);

    in->length-=chunk_size;
   }

 out->data[out->length++]='\r';
 out->data[out->length++]='\n';

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Read some data from an input buffer and perform chunked decoding to an output buffer.

  int io_chunk_decode Returns 0 normally, 1 if finished or negative for error.

  io_buffer *in The input buffer.

  io_chunk *context The chunk context information.

  io_buffer *out The output buffer.
  ++++++++++++++++++++++++++++++++++++++*/

int io_chunk_decode(io_buffer *in,io_chunk *context,io_buffer *out)
{
 size_t nused=0;

 /* If there is no input data then we have finished early. */

 if(in->length==0)
    return(1);

 do
   {
    /* If parsing a header then do that */

    if(context->doing_head)
      {
       int nb=parse_chunk_head(context,in->data+nused,in->length-nused);

       if(nb<0)
          return(-1);
       else if(nb==(in->length-nused))
         {
          in->length=0;
          return(0);
         }
       else
          nused+=nb;
      }

    /* else if parsing a trailer then do that */

    else if(context->doing_trailer)
      {
       int nb=parse_chunk_trailer(context,in->data+nused,in->length-nused);

       if(nb<0)
          return(-1);
       else if(nb==(in->length-nused))
         {
          in->length=0;
          return(0);
         }
       else
          nused+=nb;
      }

    /* else if parsing a chunk do that */

    else if(context->doing_chunk)
      {
       int chunk_size=in->length-nused;

       if(chunk_size>context->remain)
          chunk_size=context->remain;

       if((out->size-out->length)<chunk_size)
          chunk_size=out->size-out->length;

       if(chunk_size>0)
         {
          memcpy(out->data+out->length,in->data+nused,chunk_size);
          out->length+=chunk_size;

          nused+=chunk_size;

          context->remain-=chunk_size;

          if(context->remain==0)
            {
             context->doing_chunk=0;
             context->doing_head=1;
            }
         }
      }

    /* else must be finished */

    else
       return(1);
   }
 while(nused<in->length && out->length<out->size);

 if(nused<in->length)
    memmove(in->data,in->data+nused,in->length-nused);

 in->length-=nused;

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Finalise the chunked-encoding data stream and output all remaining bytes.

  int io_finish_chunk_encode Returns 0 on completion, 1 if there is more data, negative if error.

  io_chunk *context The chunk context information..

  io_buffer *out The final output buffer to fill with tail data.
  ++++++++++++++++++++++++++++++++++++++*/

int io_finish_chunk_encode(io_chunk *context,io_buffer *out)
{
 if(context->chunk_buffer->length)
    io_chunk_encode(NULL,context,out);

 if(context->chunk_buffer->length || (out->size-out->length)<5)
    return(1);

 out->data[out->length++]='0';
 out->data[out->length++]='\r';
 out->data[out->length++]='\n';
 out->data[out->length++]='\r';
 out->data[out->length++]='\n';

 destroy_io_buffer(context->chunk_buffer);

 free(context);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Finalise the chunked-encoding data stream and output all remaining bytes.

  int io_finish_chunk_decode Returns 0 on completion, 1 if there is more data, negative if error.

  io_chunk *context The chunk context information..

  io_buffer *out The final output buffer to fill with tail data.
  ++++++++++++++++++++++++++++++++++++++*/

int io_finish_chunk_decode(io_chunk *context,/*@unused@*/ io_buffer *out)
{
 free(context);

 return(0);
}


/*+ For conversion from hex string to integer. +*/
static const unsigned char unhexstring[64]={ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                             0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                             0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0x60-0x6f "`abcdefghijklmno" */


/*++++++++++++++++++++++++++++++++++++++
  Parse a chunk header.

  int parse_chunk_head Returns the amount of the buffer consumed (not all if head finished).

  io_chunk *context The chunk context information.

  const char *buffer The buffer of new data.

  int n The amount of new data.
  ++++++++++++++++++++++++++++++++++++++*/

static int parse_chunk_head(io_chunk *context,const char *buffer,int n)
{
 const unsigned char *p0=(unsigned char*)buffer,*p=(unsigned char*)buffer;

 while(n>0)
   {
    switch(context->doing_head)
      {
      case 1:                   /* CR from previous chunk */
       if(*p!='\r')
         {set_error("not chunked encoding; not CR");return(-1);}
       context->doing_head++;n--;p++;
       break;
      case 2:                   /* LF from previous chunk */
       if(*p!='\n')
         {set_error("not chunked encoding; not LF");return(-1);}
       context->doing_head++;n--;p++;
       break;
      case 3:                   /* length byte 1 */
       if(isxdigit(*p))
         {context->doing_head++;context->remain=unhexstring[*p-48];}
       else if(*p==' ')
          ;
       else
         {set_error("not chunked encoding; not hex digit");return(-1);}
       n--;p++;
       break;
      case 4:                   /* length byte 2..16 or ';' or CR */
       if(isxdigit(*p))
          context->remain=(context->remain<<4)+unhexstring[*p-48];
       else if(*p==';')
          context->doing_head=5;
       else if(*p=='\r')
          context->doing_head=6;
       else if(*p==' ')
          ;
       else
         {set_error("not chunked encoding; not hex digit, ';' or CR");return(-1);}
       n--;p++;
       break;
      case 5:                   /* chunk extension */
       if(*p=='\r')
          context->doing_head=6;
       n--;p++;
       break;
      case 6:                   /* LF */
       if(*p=='\n')
          context->doing_head=0;
       else
         {set_error("not chunked encoding; not LF");return(-1);}
       n--;p++;
       break;
      case 0:                   /* finished */
       n=0;
      }
   }

 if(context->doing_head==0)
   {
    if(context->remain)
       context->doing_chunk=1;
    else
       context->doing_trailer=1;
   }

 return(p-p0);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse a chunk trailer.

  int parse_chunk_trailer Returns the amount of the buffer consumed (not all if trailer finished).

  io_chunk *context The chunk context information.

  const char *buffer The buffer of new data.

  int n The amount of new data.
  ++++++++++++++++++++++++++++++++++++++*/

static int parse_chunk_trailer(io_chunk *context,const char *buffer,int n)
{
 const unsigned char *p0=(unsigned char*)buffer,*p=(unsigned char*)buffer;

 while(n>0)
   {
    switch(context->doing_trailer)
      {
      case 1:                   /* trailer text first char */
       if(*p=='\r')
         {
          context->doing_trailer=4;
          break;
         }
       context->doing_trailer++;n--;p++;
       break;
      case 2:                   /* trailer text other chars or CR */
       if(*p=='\r')
          context->doing_trailer++;
       n--;p++;
       break;
      case 3:                   /* trailer text LF */
       if(*p!='\n')
         {set_error("not chunked encoding; not LF");return(-1);}
       context->doing_trailer=1;
       n--;p++;
       break;
      case 4:                   /* final CR */
       context->doing_trailer++;n--;p++;
       break;
      case 5:                   /* final LF */
       if(*p!='\n')
         {set_error("not chunked encoding; not LF");return(-1);}
       context->doing_trailer=0;n--;p++;
       break;
      case 0:                   /* finished */
       n=0;
      }
   }

 return(p-p0);
}


/*++++++++++++++++++++++++++++++++++++++
  Set the error status when there is a chunk decoding error.

  const char *msg The error message.
  ++++++++++++++++++++++++++++++++++++++*/

static void set_error(const char *msg)
{
 errno=ERRNO_USE_IO_ERRNO;

 if(io_strerror)
    free(io_strerror);
 io_strerror=(char*)malloc(16+strlen(msg)+1);

 sprintf(io_strerror,"IO(chunked): %s",msg);

 io_errno=-1;
}
