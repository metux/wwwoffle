/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/iopriv.h 1.16 2006/07/21 17:46:52 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Functions for file input and output (private data structures).
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef IO_PRIV_H
#define IO_PRIV_H    /*+ To stop multiple inclusions. +*/

#include <unistd.h>

#if USE_ZLIB
#include <zlib.h>
#endif

#if USE_GNUTLS
#include <gnutls/gnutls.h>
#endif


/*+ A type defined to maintain a buffer of data. +*/

typedef struct io_buffer
{
 char *data;                    /*+ The data buffer. +*/
 size_t length;                 /*+ The occupied length of the data buffer. +*/
 size_t size;                   /*+ The size of the data buffer. +*/
}
io_buffer;


#if USE_ZLIB

/*+ A data structure to hold the deflate and gzip context. +*/

typedef struct io_zlib
{
 int type;                      /*+ The type using the RFC number, zlib=1950, deflate=1951, gzip=1952. +*/

 z_stream stream;               /*+ The deflate / inflate stream (defined in zlib.h). +*/

 unsigned long crc;             /*+ The gzip crc. +*/

 short doing_head;              /*+ A flag to indicate that we are doing a gzip head (decompression). +*/
 unsigned short head_extra_len; /*+ A gzip header extra field length (decompression). +*/
 unsigned short head_flag;      /*+ A flag to store the gzip header flag (decompression). +*/

 short doing_body;              /*+ A flag to indicate that we are doing a gzip body (decompression). +*/

 short doing_tail;              /*+ A flag to indicate that we are doing a gzip tail (decompression). +*/
 unsigned long tail_crc;        /*+ The crc value stored in the gzip tail (decompression). +*/
 unsigned long tail_len;        /*+ The length value stored in the gzip tail (decompression). +*/

 short insert_head;             /*+ A flag to indicate that the gzip header must be inserted (compression). +*/

 unsigned short lastlen;        /*+ The length of the input data last time. +*/
}
io_zlib;

#endif /* USE_ZLIB */


#if USE_GNUTLS

/*+ A data structure to hold the gnutls context. +*/

typedef struct io_gnutls
{
 int type;                            /*+ The type (server=1, client=0). +*/

 int fd;                              /*+ The file descriptor used for this session. +*/

 gnutls_certificate_credentials_t cred; /*+ The certificate credentials. +*/

 gnutls_session_t session;            /*+ The session information. +*/
}
io_gnutls;

#endif /* USE_GNUTLS */


/*+ A data structure to hold the chunked encoding context. +*/

typedef struct io_chunk
{
 short doing_head;              /*+ A flag to indicate that we are doing the chunk head (decoding). +*/

 short doing_chunk;             /*+ A flag to indicate that we are doing the chunk data (decoding). +*/
 unsigned long remain;          /*+ The number of bytes remaining in this chunk (decoding). +*/

 short doing_trailer;           /*+ A flag to indicate that we are handling the trailer part (decoding). +*/

 io_buffer *chunk_buffer;       /*+ A buffer of data that is too small to send in a single chunk (encoding). +*/
}
io_chunk;


/*+ A structure to contain a complete set of io buffering operations. +*/

typedef struct io_context
{
 /* Read parameters */

 unsigned r_timeout;            /*+ The timeout to use when reading. +*/

 io_buffer *r_line_data;        /*+ The spare data when reading lines (reading). +*/

#if USE_ZLIB
 io_zlib   *r_zlib_context;     /*+ The zlib compression/decompression private data (reading). +*/

 io_buffer *r_zlch_data;        /*+ The IO buffer between zlib and chunked encoding (reading). +*/
#endif

 io_chunk  *r_chunk_context;    /*+ The chunked encoding/decoding private data (reading). +*/

 io_buffer *r_file_data;        /*+ The IO buffer closest to the raw file (reading). +*/

 unsigned long r_raw_bytes;     /*+ The number of raw bytes read. +*/

 /* Write parameters */

 unsigned w_timeout;            /*+ The timeout to use when writing. +*/

 io_buffer *w_buffer_data;      /*+ Input buffering to avoid very small writes (writing). +*/

#if USE_ZLIB
 io_zlib   *w_zlib_context;     /*+ The zlib compression/decompression private data (writing). +*/

 io_buffer *w_zlch_data;        /*+ The IO buffer between zlib and chunked encoding (writing). +*/
#endif

 io_chunk  *w_chunk_context;    /*+ The chunked encoding/decoding private data (writing). +*/

 io_buffer *w_file_data;        /*+ The IO buffer closest to the raw file (writing). +*/

 unsigned long w_raw_bytes;     /*+ The number of raw bytes written. +*/

#if USE_GNUTLS

 /* SSL/https parameters */

 io_gnutls *gnutls_context;     /*+ The gnutls encryption/decryption context. +*/

#endif
}
io_context;


/* In io.c */

/*+ The IO functions error number. +*/
extern int io_errno;

/*+ The IO functions error message string. +*/
extern char *io_strerror;


#if USE_ZLIB

/* In io_zlib.c */

io_zlib /*@null@*/ /*@special@*/ *io_init_zlib_compress  (int type) /*@allocates result@*/;
io_zlib /*@null@*/ /*@special@*/ *io_init_zlib_uncompress(int type) /*@allocates result@*/;

int io_zlib_compress  (io_buffer *in,io_zlib *context,io_buffer *out);
int io_zlib_uncompress(io_buffer *in,io_zlib *context,io_buffer *out);

int io_finish_zlib_compress  (io_zlib *context,io_buffer *out);
int io_finish_zlib_uncompress(io_zlib *context,/*@null@*/ io_buffer *out);

#endif /* USE_ZLIB */


#if USE_GNUTLS

/* In iognutls.c */

io_gnutls /*@null@*/ /*@special@*/ *io_init_gnutls(int fd,/*@null@*/ const char *host,int type) /*@allocates result@*/;
int io_finish_gnutls(io_gnutls *context);

ssize_t io_gnutls_read_with_timeout(io_gnutls *context,io_buffer *out,unsigned timeout);
ssize_t io_gnutls_write_with_timeout(io_gnutls *context,io_buffer *in,unsigned timeout);

#endif /* USE_GNUTLS */


/* In io_chunk.c */

io_chunk /*@special@*/ *io_init_chunk_encode(void) /*@allocates result@*/;
io_chunk /*@special@*/ *io_init_chunk_decode(void) /*@allocates result@*/;

int io_chunk_encode(/*@null@*/ io_buffer *in,io_chunk *context,io_buffer *out);
int io_chunk_decode(io_buffer *in,io_chunk *context,io_buffer *out);

int io_finish_chunk_encode(io_chunk *context,io_buffer *out);
int io_finish_chunk_decode(io_chunk *context,/*@null@*/ io_buffer *out);


/* In iopriv.c */

io_buffer /*@special@*/ *create_io_buffer(size_t size) /*@allocates result@*/;
void destroy_io_buffer(/*@special@*/ io_buffer *buffer) /*@releases buffer@*/;

ssize_t io_read_with_timeout(int fd,io_buffer *out,unsigned timeout);

ssize_t io_write_with_timeout(int fd,io_buffer *in,unsigned timeout);


#endif /* IO_PRIV_H */
