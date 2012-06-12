/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/io.c,v 2.60 2007-03-25 11:05:46 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Functions for file input and output.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include <sys/types.h>
#include <unistd.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <errno.h>

#include "io.h"
#include "iopriv.h"
#include "errors.h"


/*+ The buffer size for reading lines. +*/
#define LINE_BUFFER_SIZE  256

/*+ The buffer size for write accumulation (use same size as MIN_CHUNK_SIZE). +*/
#define WRITE_BUFFER_SIZE (IO_BUFFER_SIZE/4)


/*+ The number of IO contexts allocated. +*/
static int nio=0;

/*+ The allocated IO contexts. +*/
static /*@only@*/ io_context **io_contexts;


/*+ The IO functions error number. +*/
int io_errno=0;

/*+ The IO functions error message string. +*/
char /*@null@*/ *io_strerror=NULL;


static int io_write_data(int fd,io_context *context,io_buffer *iobuffer);


/*++++++++++++++++++++++++++++++++++++++
  Initialise the IO context used for this file descriptor.

  int fd The file descriptor to initialise.
  ++++++++++++++++++++++++++++++++++++++*/

void init_io(int fd)
{
 if(fd==-1)
    PrintMessage(Fatal,"IO: Function init_io(%d) was called with an invalid argument.",fd);

 /* Allocate some space for new IO contexts */

 if(fd>=nio)
   {
    io_contexts=(io_context**)realloc((void*)io_contexts,(fd+9)*sizeof(io_context**));

    for(;nio<=(fd+8);nio++)
       io_contexts[nio]=NULL;
   }

 /* Allocate the new context */

 if(io_contexts[fd])
    PrintMessage(Fatal,"IO: Function init_io(%d) was called twice without calling finish_io(%d).",fd,fd);

 io_contexts[fd]=(io_context*)calloc((size_t)1,sizeof(io_context));
}


/*++++++++++++++++++++++++++++++++++++++
  Re-initialise a file descriptor (e.g. after seeking on a file).

  int fd The file descriptor to re-initialise.
  ++++++++++++++++++++++++++++++++++++++*/

void reinit_io(int fd)
{
 io_context *context;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

#if USE_ZLIB
 if(context->r_zlib_context || context->w_zlib_context)
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called while zlib compression enabled.",fd);
#endif

 if(context->r_chunk_context || context->w_chunk_context)
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called while chunked encoding enabled.",fd);

#if USE_GNUTLS
 if(context->gnutls_context)
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called while gnutls encryption enabled.",fd);
#endif

 if(context->r_file_data)
   {
    destroy_io_buffer(context->r_file_data);
    context->r_file_data=NULL;
   }

 context->r_raw_bytes=0;
 context->w_raw_bytes=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Configure the timeout in the IO context used for this file descriptor.

  int fd The file descriptor.

  int timeout_r The read timeout or 0 for none or -1 to leave unchanged.

  int timeout_w The write timeout or 0 for none or -1 to leave unchanged.
  ++++++++++++++++++++++++++++++++++++++*/

void configure_io_timeout(int fd,int timeout_r,int timeout_w)
{
 io_context *context;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function configure_io_timeout(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_timeout(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Set the timeouts */

 if(timeout_r>=0)
    context->r_timeout=timeout_r;

 if(timeout_w>=0)
    context->w_timeout=timeout_w;
}


#if USE_ZLIB

/*++++++++++++++++++++++++++++++++++++++
  Configure the compression option for the IO context used for this file descriptor.

  int fd The file descriptor.

  int zlib_r The flag to indicate the new zlib compression method for reading or -1 to leave unchanged.

  int zlib_w The flag to indicate the new zlib compression method for writing or -1 to leave unchanged.
  ++++++++++++++++++++++++++++++++++++++*/

void configure_io_zlib(int fd,int zlib_r,int zlib_w)
{
 io_context *context;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function configure_io_zlib(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_zlib(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the zlib decompression context for reading */

 if(zlib_r>=0)
   {
    if(zlib_r && !context->r_zlib_context)
      {
       context->r_zlib_context=io_init_zlib_uncompress(zlib_r);
       if(!context->r_zlib_context)
          PrintMessage(Fatal,"IO: Could not initialise zlib uncompression; [%!s].");

       if(context->r_chunk_context && !context->r_zlch_data)
          context->r_zlch_data=create_io_buffer(IO_BUFFER_SIZE);

       if(!context->r_file_data)
          context->r_file_data=create_io_buffer(IO_BUFFER_SIZE);
       else
          resize_io_buffer(context->r_file_data,IO_BUFFER_SIZE);
      }
    else if(!zlib_r && context->r_zlib_context)
      {
       context->r_zlib_context=NULL;

       if(context->r_zlch_data)
          destroy_io_buffer(context->r_zlch_data);
       context->r_zlch_data=NULL;
      }
   }

 /* Create the zlib compression context for writing */

 if(zlib_w>=0)
   {
    if(zlib_w && !context->w_zlib_context)
      {
       context->w_zlib_context=io_init_zlib_compress(zlib_w);
       if(!context->w_zlib_context)
          PrintMessage(Fatal,"IO: Could not initialise zlib compression; [%!s].");

       if(context->w_chunk_context && !context->w_zlch_data)
          context->w_zlch_data=create_io_buffer(IO_BUFFER_SIZE);

       if(!context->w_file_data)
          context->w_file_data=create_io_buffer(IO_BUFFER_SIZE);
      }
    else if(!zlib_w && context->w_zlib_context)
      {
       context->w_zlib_context=NULL;

       if(context->w_zlch_data)
          destroy_io_buffer(context->w_zlch_data);
       context->r_zlch_data=NULL;
      }
   }
}

#endif /* USE_ZLIB */


/*++++++++++++++++++++++++++++++++++++++
  Configure the chunked encoding/decoding for the IO context used for this file descriptor.

  int fd The file descriptor.

  int chunked_r The flag to indicate the new chunked encoding method for reading or -1 to leave unchanged.

  int chunked_w The flag to indicate the new chunked encoding method for writing or -1 to leave unchanged.
  ++++++++++++++++++++++++++++++++++++++*/

void configure_io_chunked(int fd,int chunked_r,int chunked_w)
{
 io_context *context;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function configure_io_chunked(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_chunked(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the chunked decoding context for reading */

 if(chunked_r>=0)
   {
    if(chunked_r && !context->r_chunk_context)
      {
       context->r_chunk_context=io_init_chunk_decode();
       if(!context->r_chunk_context)
          PrintMessage(Fatal,"IO: Could not initialise chunked decoding; [%!s].");

#if USE_ZLIB
       if(context->r_zlib_context && !context->r_zlch_data)
          context->r_zlch_data=create_io_buffer(IO_BUFFER_SIZE);
#endif

       if(!context->r_file_data)
          context->r_file_data=create_io_buffer(IO_BUFFER_SIZE);
       else
          resize_io_buffer(context->r_file_data,IO_BUFFER_SIZE);
      }
    else if(!chunked_r && context->r_chunk_context)
      {
       context->r_chunk_context=NULL;

#if USE_ZLIB
       if(context->r_zlch_data)
          destroy_io_buffer(context->r_zlch_data);
       context->r_zlch_data=NULL;
#endif
      }
   }

 /* Create the chunked encoding context for writing */

 if(chunked_w>=0)
   {
    if(chunked_w && !context->w_chunk_context)
      {
       context->w_chunk_context=io_init_chunk_encode();
       if(!context->w_chunk_context)
          PrintMessage(Fatal,"IO: Could not initialise chunked encoding; [%!s].");

#if USE_ZLIB
       if(context->w_zlib_context && !context->w_zlch_data)
          context->w_zlch_data=create_io_buffer(IO_BUFFER_SIZE);
#endif

       if(!context->w_file_data)
          context->w_file_data=create_io_buffer(IO_BUFFER_SIZE+16);
       else
          resize_io_buffer(context->w_file_data,IO_BUFFER_SIZE+16);
      }
    else if(!chunked_w && context->w_chunk_context)
      {
       context->w_chunk_context=NULL;

#if USE_ZLIB
       if(context->w_zlch_data)
          destroy_io_buffer(context->w_zlch_data);
       context->r_zlch_data=NULL;
#endif
      }
   }
}


#if USE_GNUTLS

/*++++++++++++++++++++++++++++++++++++++
  Configure the encryption in the IO context used for this file descriptor.

  int configure_io_gnutls returns 0 if OK or an error code.

  int fd The file descriptor to configure.

  const char *host The name of the server to serve as or NULL for a client.

  int type A flag set to 0 for client connection, 1 for built-in server or 2 for a fake server.
  ++++++++++++++++++++++++++++++++++++++*/

int configure_io_gnutls(int fd,const char *host,int type)
{
 io_context *context;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function configure_io_gnutls(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_gnutls(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Initialise the GNUTLS context information. */

 context->gnutls_context=io_init_gnutls(fd,host,type);

 if(!context->gnutls_context)
    return(1);

 return(0);
}

#endif


/*++++++++++++++++++++++++++++++++++++++
  Read data from a file descriptor instead of read().

  ssize_t read_data Returns the number of bytes read or 0 for end of file.

  int fd The file descriptor.

  char *buffer The buffer to put the data into.

  size_t n The maximum number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t read_data(int fd,char *buffer,size_t n)
{
 io_context *context;
 int err=0,nr=0;
 io_buffer iobuffer;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function read_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function read_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the output buffer */

 iobuffer.data=buffer;
 iobuffer.size=n;
 iobuffer.length=0;

 /* Read in new data */

#if USE_ZLIB
 if(!context->r_zlib_context)
   {
#endif
    if(!context->r_chunk_context)
      {
       if(context->r_file_data && context->r_file_data->length)
         {
          if(iobuffer.size>context->r_file_data->length)
            {
             memcpy(iobuffer.data,context->r_file_data->data,context->r_file_data->length);
             iobuffer.length=context->r_file_data->length;
             context->r_file_data->length=0;
            }
          else
            {
             memcpy(iobuffer.data,context->r_file_data->data,iobuffer.size);
             iobuffer.length+=iobuffer.size;
             memmove(context->r_file_data->data,context->r_file_data->data+iobuffer.size,context->r_file_data->length-iobuffer.size);
             context->r_file_data->length-=iobuffer.size;
            }

          nr=iobuffer.length;
         }
       else
         {
#if USE_GNUTLS
          if(context->gnutls_context)
             nr=io_gnutls_read_with_timeout(context->gnutls_context,&iobuffer,context->r_timeout);
          else
#endif
             nr=io_read_with_timeout(fd,&iobuffer,context->r_timeout);
          if(nr>0) context->r_raw_bytes+=nr;
         }
      }
    else /* if(context->r_chunk_context) */
      {
       do
         {
#if USE_GNUTLS
          if(context->gnutls_context)
             err=io_gnutls_read_with_timeout(context->gnutls_context,context->r_file_data,context->r_timeout);
          else
#endif
             err=io_read_with_timeout(fd,context->r_file_data,context->r_timeout);
          if(err>0) context->r_raw_bytes+=err;
          if(err<0) break;
          err=io_chunk_decode(context->r_file_data,context->r_chunk_context,&iobuffer);
          if(err<0 || err==1) break;
         }
       while(iobuffer.length==0);
       nr=iobuffer.length;
      }
#if USE_ZLIB
   }
 else /* if(context->r_zlib_context) */
   {
    if(!context->r_chunk_context)
      {
       do
         {
#if USE_GNUTLS
          if(context->gnutls_context)
             err=io_gnutls_read_with_timeout(context->gnutls_context,context->r_file_data,context->r_timeout);
          else
#endif
             err=io_read_with_timeout(fd,context->r_file_data,context->r_timeout);
          if(err>0) context->r_raw_bytes+=err;
          if(err<0) break;
          err=io_zlib_uncompress(context->r_file_data,context->r_zlib_context,&iobuffer);
          if(err<0 || err==1) break;
         }
       while(iobuffer.length==0);
       nr=iobuffer.length;
      }
    else /* if(context->r_chunk_context) */
      {
       do
         {
#if USE_GNUTLS
          if(context->gnutls_context)
             err=io_gnutls_read_with_timeout(context->gnutls_context,context->r_file_data,context->r_timeout);
          else
#endif
             err=io_read_with_timeout(fd,context->r_file_data,context->r_timeout);
          if(err>0) context->r_raw_bytes+=err;
          if(err<0) break;
          err=io_chunk_decode(context->r_file_data,context->r_chunk_context,context->r_zlch_data);
          if(err<0) break;

          /* Try uncompressing only if chunking is finished or there is data or zlib is initialised. */
          if(err==1 || context->r_zlch_data->length>0 || context->r_zlib_context->stream.state)
             err=io_zlib_uncompress(context->r_zlch_data,context->r_zlib_context,&iobuffer);

          if(err<0 || err==1) break;
         }
       while(iobuffer.length==0);
       nr=iobuffer.length;
      }
   }
#endif /* USE_ZLIB */

 if(err<0)
    return(err);
 else
    return(nr);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a single line of data from a file descriptor like fgets does from a FILE*.

  char *read_line Returns the modified string or NULL for the end of file.

  int fd The file descriptor.

  char *line The previously allocated line of data.
  ++++++++++++++++++++++++++++++++++++++*/

char *read_line(int fd,char *line)
{
 io_context *context;
 int found=0,eof=0;
 size_t n=0;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function read_line(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function read_line(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the temporary line buffer if there is not one */

 if(!context->r_file_data)
    context->r_file_data=create_io_buffer(LINE_BUFFER_SIZE+1);

 /* Use the existing data or read in some more */

 do
   {
    line=(char*)realloc((void*)line,n+(LINE_BUFFER_SIZE+1));

    if(context->r_file_data->length>0)
      {
       for(n=0;n<context->r_file_data->length && n<LINE_BUFFER_SIZE;n++)
          if(context->r_file_data->data[n]=='\n')
            {
             found=1;
             n++;
             break;
            }

       memcpy(line,context->r_file_data->data,n);

       if(n==context->r_file_data->length)
          context->r_file_data->length=0;
       else
         {
          context->r_file_data->length-=n;
          memmove(context->r_file_data->data,context->r_file_data->data+n,context->r_file_data->length);
         }
      }
    else
      {
       ssize_t nn;
       io_buffer iobuffer;

       /* FIXME
          Cannot call read_line() on compressed files.
          Probably not important, wasn't possible with old io.c either.
          FIXME */

       iobuffer.data=line+n;
       iobuffer.size=LINE_BUFFER_SIZE;
       iobuffer.length=0;

#if USE_GNUTLS
       if(context->gnutls_context)
          nn=io_gnutls_read_with_timeout(context->gnutls_context,&iobuffer,context->r_timeout);
       else
#endif
          nn=io_read_with_timeout(fd,&iobuffer,context->r_timeout);
       if(nn>0) context->r_raw_bytes+=nn;

       if(nn<=0)
         {eof=1;break;}
       else
          nn+=n;

       for(;n<nn;n++)
          if(line[n]=='\n')
            {
             found=1;
             n++;
             break;
            }

       if(found)
         {
          context->r_file_data->length=nn-n;
          memcpy(context->r_file_data->data,line+n,context->r_file_data->length);
         }
      }
   }
 while(!found && !eof);

 if(found)
    line[n]=0;

 if(eof)
   {free(line);line=NULL;}

 return(line);
}


/*++++++++++++++++++++++++++++++++++++++
  Write data to a file descriptor instead of write().

  ssize_t write_data Returns the number of bytes written.

  int fd The file descriptor.

  const char *data The data buffer to write.

  size_t n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t write_data(int fd,const char *data,size_t n)
{
 io_context *context;
 io_buffer iobuffer;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function write_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function write_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 if(context->w_buffer_data && context->w_buffer_data->length)
   {
    int err=io_write_data(fd,context,context->w_buffer_data);
    if(err<0) return(err);
   }

 /* Create the temporary input buffer */

 iobuffer.data=(char*)data;
 iobuffer.size=n;
 iobuffer.length=n;

 return(io_write_data(fd,context,&iobuffer));
}


/*++++++++++++++++++++++++++++++++++++++
  Write data to a file descriptor instead of write() but with buffering.

  ssize_t write_buffer_data Returns the number of bytes written.

  int fd The file descriptor.

  const char *data The data buffer to write.

  size_t n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t write_buffer_data(int fd,const char *data,size_t n)
{
 io_context *context;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function write_buffer_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function write_buffer_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the temporary write buffer if there is not one */

 if(!context->w_buffer_data)
    context->w_buffer_data=create_io_buffer(2*WRITE_BUFFER_SIZE);

 if(n<=WRITE_BUFFER_SIZE || (n+context->w_buffer_data->length)<=(2*WRITE_BUFFER_SIZE))
   {
    memcpy(context->w_buffer_data->data+context->w_buffer_data->length,data,n);
    context->w_buffer_data->length+=n;

    if(context->w_buffer_data->length>WRITE_BUFFER_SIZE)
       return(io_write_data(fd,context,context->w_buffer_data));
    else
       return(n);
   }
 else
   {
    io_buffer iobuffer;

    if(context->w_buffer_data && context->w_buffer_data->length)
      {
       int err=io_write_data(fd,context,context->w_buffer_data);
       if(err<0) return(err);
      }

    /* Create the temporary input buffer */

    iobuffer.data=(char*)data;
    iobuffer.size=n;
    iobuffer.length=n;

    return(io_write_data(fd,context,&iobuffer));
   }
}


/*++++++++++++++++++++++++++++++++++++++
  An internal function to actually write the data bypassing the write buffer.

  int io_write_data Returns the number of bytes written.

  int fd The file descriptor to write to.

  io_context *context The IO context to write to.

  io_buffer *iobuffer The buffer of data to write.
  ++++++++++++++++++++++++++++++++++++++*/

static int io_write_data(int fd,io_context *context,io_buffer *iobuffer)
{
 int err=0;

 /* Write the output data */

#if USE_ZLIB
 if(!context->w_zlib_context)
   {
#endif
    if(!context->w_chunk_context)
      {
#if USE_GNUTLS
       if(context->gnutls_context)
          err=io_gnutls_write_with_timeout(context->gnutls_context,iobuffer,context->w_timeout);
       else
#endif
          err=io_write_with_timeout(fd,iobuffer,context->w_timeout);
       if(err>0) context->w_raw_bytes+=err;
      }
    else /* if(context->w_chunk_context) */
      {
       do
         {
          err=io_chunk_encode(iobuffer,context->w_chunk_context,context->w_file_data);
          if(err<0) break;
          if(context->w_file_data->length>0)
            {
#if USE_GNUTLS
             if(context->gnutls_context)
                err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
             else
#endif
                err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
             if(err>0) context->w_raw_bytes+=err;
             if(err<0) break;
            }
         }
       while(iobuffer->length>0);
      }
#if USE_ZLIB
   }
 else /* if(context->w_zlib_context) */
   {
    if(!context->w_chunk_context)
      {
       do
         {
          err=io_zlib_compress(iobuffer,context->w_zlib_context,context->w_file_data);
          if(err<0) break;
          if(context->w_file_data->length>0)
            {
#if USE_GNUTLS
             if(context->gnutls_context)
                err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
             else
#endif
                err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
             if(err>0) context->w_raw_bytes+=err;
             if(err<0) break;
            }
         }
       while(iobuffer->length>0);
      }
    else /* if(context->w_chunk_context) */
      {
       do
         {
          err=io_zlib_compress(iobuffer,context->w_zlib_context,context->w_zlch_data);
          if(err<0) break;
          if(context->w_zlch_data->length>0)
            {
             err=io_chunk_encode(context->w_zlch_data,context->w_chunk_context,context->w_file_data);
             if(err<0) break;
             if(context->w_file_data->length>0)
               {
#if USE_GNUTLS
                if(context->gnutls_context)
                   err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
                else
#endif
                   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
                if(err>0) context->w_raw_bytes+=err;
                if(err<0) break;
               }
            }
         }
       while(iobuffer->length>0);
      }
   }
#endif /* USE_ZLIB */

 if(err<0)
    return(err);
 else
    return(iobuffer->length);
}


/*++++++++++++++++++++++++++++++++++++++
  Write a simple string to a file descriptor like fputs does to a FILE*.

  ssize_t write_string Returns the number of bytes written.

  int fd The file descriptor.

  const char *str The string.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t write_string(int fd,const char *str)
{
 return(write_data(fd,str,strlen(str)));
}


/*++++++++++++++++++++++++++++++++++++++
  Write a formatted string to a file descriptor like fprintf does to a FILE*.

  ssize_t write_formatted Returns the number of bytes written.

  int fd The file descriptor.

  const char *fmt The format string.

  ... The other arguments.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t write_formatted(int fd,const char *fmt,...)
{
 int i,width;
 ssize_t n;
 char *str,*strp;
 va_list ap;

 /* Estimate the length of the string. */

#ifdef __STDC__
 va_start(ap,fmt);
#else
 va_start(ap);
#endif

 n=strlen(fmt);

 for(i=0;fmt[i];i++)
    if(fmt[i]=='%')
      {
       i++;

       if(fmt[i]=='%')
          continue;

       width=atoi(fmt+i);
       if(width<0)
          width=-width;

       while(!isalpha(fmt[i]))
          i++;

       if(fmt[i]=='s')
         {
          strp=va_arg(ap,char*);
          if(width && width>strlen(strp))
             n+=width;
          else
             n+=strlen(strp);
         }
       else
         {
          (void)va_arg(ap,void*);
          if(width && width>22) /* 22 characters for 64-bit octal (%llo) value ~0. */
             n+=width;
          else
             n+=22;
         }
      }

 va_end(ap);

 /* Allocate the string and vsprintf into it. */

 str=(char*)malloc(n+1);

#ifdef __STDC__
 va_start(ap,fmt);
#else
 va_start(ap);
#endif

 n=vsprintf(str,fmt,ap);

 va_end(ap);

 /* Write the string. */

 n=write_data(fd,str,n);

 free(str);

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the number of raw bytes read and written to the file descriptor.

  int fd The file descriptor.

  unsigned long* r Returns the number of bytes read.

  unsigned long *w Returns the raw number of bytes written.
  ++++++++++++++++++++++++++++++++++++++*/

void tell_io(int fd,unsigned long* r,unsigned long *w)
{
 io_context *context;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function tell_io(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function tell_io(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Return the data */

 if(r)
    *r=context->r_raw_bytes;
 if(r && context->r_file_data)
    *r-=context->r_file_data->length; /* Pretend we have not read the contents of the line data buffer */

 if(w)
    *w=context->w_raw_bytes;
}


/*++++++++++++++++++++++++++++++++++++++
  Finish with the IO context used for this file descriptor and report the number of bytes.

  int fd The file descriptor to finish.

  unsigned long* r Returns the number of bytes read.

  unsigned long *w Returns the raw number of bytes written.
  ++++++++++++++++++++++++++++++++++++++*/

void finish_tell_io(int fd,unsigned long* r,unsigned long *w)
{
 io_context *context;
 int err=0;
 int more;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function finish_io(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function finish_io(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Finish the reading side */

#if USE_ZLIB
 if(!context->r_zlib_context)
   {
#endif
    if(context->r_chunk_context)
       io_finish_chunk_decode(context->r_chunk_context,NULL);
#if USE_ZLIB
   }
 else /* if(context->r_zlib_context) */
   {
    if(!context->r_chunk_context)
       io_finish_zlib_uncompress(context->r_zlib_context,NULL);
    else /* if(context->r_chunk_context) */
      {
       io_finish_chunk_decode(context->r_chunk_context,NULL);
       io_finish_zlib_uncompress(context->r_zlib_context,NULL);
      }
   }
#endif /* USE_ZLIB */

 /* Write out any remaining data */

 if(context->w_buffer_data && context->w_buffer_data->length)
    io_write_data(fd,context,context->w_buffer_data);

#if USE_ZLIB
 if(!context->w_zlib_context)
   {
#endif
    if(context->w_chunk_context)
      {
       do
         {
          more=io_finish_chunk_encode(context->w_chunk_context,context->w_file_data);
          if(more>=0)
            {
#if USE_GNUTLS
             if(context->gnutls_context)
                err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
             else
#endif
                err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
             if(err>0) context->w_raw_bytes+=err;
            }
         }
       while(more==1);
      }
#if USE_ZLIB
   }
 else /* if(context->w_zlib_context) */
   {
    if(!context->w_chunk_context)
      {
       do
         {
          more=io_finish_zlib_compress(context->w_zlib_context,context->w_file_data);
          if(more>=0)
            {
#if USE_GNUTLS
             if(context->gnutls_context)
                err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
             else
#endif
                err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
             if(err>0) context->w_raw_bytes+=err;
            }
         }
       while(more==1);
      }
    else /* if(context->w_chunk_context) */
      {
       do
         {
          more=io_finish_zlib_compress(context->w_zlib_context,context->w_zlch_data);
          if(more>=0)
            {
             int more2=io_chunk_encode(context->w_zlch_data,context->w_chunk_context,context->w_file_data);
             if(more2>=0)
               {
#if USE_GNUTLS
                if(context->gnutls_context)
                   err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
                else
#endif
                   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
                if(err>0) context->w_raw_bytes+=err;
               }
            }
         }
       while(more==1);
       do
         {
          more=io_finish_chunk_encode(context->w_chunk_context,context->w_file_data);
          if(more>=0)
            {
#if USE_GNUTLS
             if(context->gnutls_context)
                err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
             else
#endif
                err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
             if(err>0) context->w_raw_bytes+=err;
            }
         }
       while(more==1);
      }
   }
#endif /* USE_ZLIB */

 /* Destroy the encryption information */

#if USE_GNUTLS
 if(context->gnutls_context)
    io_finish_gnutls(context->gnutls_context);
#endif

 /* Return the data */

 if(r)
    *r=context->r_raw_bytes;

 if(w)
    *w=context->w_raw_bytes;

 /* Free all data structures */

 if(context->w_buffer_data)
    destroy_io_buffer(context->w_buffer_data);

#if USE_ZLIB
 if(context->r_zlch_data)
    destroy_io_buffer(context->r_zlch_data);
#endif

 if(context->r_file_data)
    destroy_io_buffer(context->r_file_data);

#if USE_ZLIB
 if(context->w_zlch_data)
    destroy_io_buffer(context->w_zlch_data);
#endif

 if(context->w_file_data)
    destroy_io_buffer(context->w_file_data);

 free(context);

 io_contexts[fd]=NULL;
}
