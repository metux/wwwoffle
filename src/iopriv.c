/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/iopriv.c,v 1.9 2007-03-25 12:47:00 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  Private functions for file input and output.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>

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

#include <setjmp.h>
#include <signal.h>

#include "io.h"
#include "iopriv.h"
#include "errors.h"


/*+ A longjump context for write timeouts. +*/
static jmp_buf write_jmp_env;


/* Local functions */

static void sigalarm(int signum);

static ssize_t write_all(int fd,const char *data,size_t n);


/*++++++++++++++++++++++++++++++++++++++
  Create an io_buffer structure.

  io_buffer *create_io_buffer The io_buffer to create.

  size_t size The size to allocate.
  ++++++++++++++++++++++++++++++++++++++*/

io_buffer *create_io_buffer(size_t size)
{
 io_buffer *new=(io_buffer*)calloc((size_t)1,sizeof(io_buffer));

 new->data=(char*)malloc(size);
 new->size=size;

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Resize an io_buffer structure.

  io_buffer *buffer The io_buffer to resize.

  size_t size The new size to allocate.
  ++++++++++++++++++++++++++++++++++++++*/

void resize_io_buffer(io_buffer *buffer,size_t size)
{
 if(buffer->size>size)
    return;

 buffer->data=(char*)realloc((void*)buffer->data,size);
 buffer->size=size;
}


/*++++++++++++++++++++++++++++++++++++++
  Destroy an io_buffer structure.

  io_buffer *buffer The io_buffer to destroy.
  ++++++++++++++++++++++++++++++++++++++*/

void destroy_io_buffer(io_buffer *buffer)
{
 free(buffer->data);

 free(buffer);
}


/*++++++++++++++++++++++++++++++++++++++
  Read some data from a file descriptor and buffer it with a timeout.

  ssize_t io_read_with_timeout Returns the number of bytes read.

  int fd The file descriptor to read from.

  io_buffer *out The IO buffer to output the data.

  unsigned timeout The maximum time to wait for data to be read (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t io_read_with_timeout(int fd,io_buffer *out,unsigned timeout)
{
 int n;

 if(timeout)
   {
    fd_set readfd;
    struct timeval tv;

    while(1)
      {
       FD_ZERO(&readfd);

       FD_SET(fd,&readfd);

       tv.tv_sec=timeout;
       tv.tv_usec=0;

       n=select(fd+1,&readfd,NULL,NULL,&tv);

       if(n>0)
          break;
       else if(n==0 || errno!=EINTR)
         {
          if(n==0)
             errno=ETIMEDOUT;
          return(-1);
         }
      }
   }

 n=read(fd,out->data+out->length,out->size-out->length);

 if(n>0)
    out->length+=n;

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  The signal handler for the alarm signal to timeout the socket write.

  int signum The signal number.
  ++++++++++++++++++++++++++++++++++++++*/

static void sigalarm(/*@unused@*/ int signum)
{
 longjmp(write_jmp_env,1);
}


/*++++++++++++++++++++++++++++++++++++++
  Write some data to a file descriptor from a buffer with a timeout.

  ssize_t io_write_with_timeout Returns the number of bytes written or negative on error.

  int fd The file descriptor to write to.

  io_buffer *in The IO buffer with the input data.

  unsigned timeout The maximum time to wait for data to be written (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t io_write_with_timeout(int fd,io_buffer *in,unsigned timeout)
{
 int n;
 struct sigaction action;

 if(in->length==0)
    return(0);

start:

 if(!timeout)
   {
    n=write_all(fd,in->data,in->length);
    in->length=0;
    return(n);
   }

 if(in->length>(4*IO_BUFFER_SIZE))
   {
    size_t offset;
    io_buffer temp;

    temp.size=in->size;

    for(offset=0;offset<in->length;offset+=IO_BUFFER_SIZE)
      {
       temp.data=in->data+offset;

       temp.length=in->length-offset;
       if(temp.length>IO_BUFFER_SIZE)
          temp.length=IO_BUFFER_SIZE;

       n=io_write_with_timeout(fd,&temp,timeout);

       if(n<0)
         {
          in->length=0;
          return(n);
         }
      }

    in->length=0;
    return(in->length);
   }

 action.sa_handler = sigalarm;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
   {
    PrintMessage(Warning, "Failed to set SIGALRM; cancelling timeout for writing.");
    timeout=0;
    goto start;
   }

 alarm(timeout);

 if(setjmp(write_jmp_env))
   {
    n=-1;
    errno=ETIMEDOUT;
   }
 else
    n=write_all(fd,in->data,in->length);

 alarm(0);
 action.sa_handler = SIG_IGN;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
    PrintMessage(Warning, "Failed to clear SIGALRM.");

 in->length=0;
 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  A function to write all of a buffer of data to a file descriptor.

  ssize_t write_all Returns the number of bytes written.

  int fd The file descriptor.

  const char *data The data buffer to write.

  size_t n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

static ssize_t write_all(int fd,const char *data,size_t n)
{
 ssize_t nn=0;

 /* Unroll the first loop to optimise the obvious case. */

 nn=write(fd,data,n);

 if(nn<0 || nn==n)
    return(nn);

 /* Loop around until the data is finished. */

 do
   {
    int m=write(fd,data+nn,n-nn);

    if(m<0)
      {n=m;break;}
    else
       nn+=m;
   }
 while(nn<n);

 return(nn);
}
