/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/io.h,v 1.15 2007-04-23 09:27:10 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  Functions for file input and output (public interfaces).
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef IO_H
#define IO_H    /*+ To stop multiple inclusions. +*/


#include <unistd.h>


/*+ The size of the buffer to use for the IO internals (also used elsewhere as general buffer size). +*/
#define IO_BUFFER_SIZE 4096


/* In io.c */

void init_io(int fd);
void reinit_io(int fd);

void configure_io_timeout(int fd,int timeout_r,int timeout_w);

#if USE_ZLIB
void configure_io_zlib(int fd,int zlib_r,int zlib_w);
#endif

void configure_io_chunked(int fd,int chunked_r,int chunked_w);

int configure_io_gnutls(int fd,const char *host,int type);

ssize_t read_data(int fd,/*@out@*/ char *buffer,size_t n);

char /*@null@*/ /*@only@*/ *read_line(int fd,/*@out@*/ /*@returned@*/ /*@null@*/ char *line);

ssize_t /*@alt void@*/ write_data(int fd,const char *data,size_t n);
ssize_t /*@alt void@*/ write_buffer_data(int fd,const char *data,size_t n);

ssize_t /*@alt void@*/ write_string(int fd,const char *str);

#ifdef __GNUC__
ssize_t /*@alt void@*/ write_formatted(int fd,const char *fmt,...) /*@printflike@*/ __attribute__ ((format (printf,2,3)));
#else
ssize_t /*@alt void@*/ write_formatted(int fd,const char *fmt,...) /*@printflike@*/;
#endif

void tell_io(int fd,/*@out@*/ /*@null@*/ unsigned long* r,/*@out@*/ /*@null@*/ unsigned long *w);

#define finish_io(fd) finish_tell_io(fd,NULL,NULL);

void finish_tell_io(int fd,/*@out@*/ /*@null@*/ unsigned long* r,/*@out@*/ /*@null@*/ unsigned long *w);

#endif /* IO_H */
