/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/proto.h 1.21 2005/12/14 19:27:22 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Information about the protocols that wwwoffle supports.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04,05 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef PROTO_H
#define PROTO_H    /*+ To stop multiple inclusions. +*/

#include "misc.h"

/*+ A type to contain the information that is required for a particular protocol. +*/
typedef struct _Protocol
{
 char *name;                    /*+ The protocol name. +*/

 int defport;                   /*+ The default port number. +*/

 int proxyable;                 /*+ Set to true if any known proxies can understand this. +*/
 int postable;                  /*+ Set to true if the POST method works. +*/
 int putable;                   /*+ Set to true if the PUT method works. +*/

 char*(*open)(URL*);            /*+ A function to open a connection to a remote server. +*/
 char*(*request)(URL*,Header*,Body*); /*+ A function to make a request to the remote server. +*/
 int  (*readhead)(Header**);    /*+ A function to read the header from the remote server. +*/
 int  (*readbody)(char*,size_t); /*+ A function to read the body data from the remote server. +*/
 int  (*close)(void);           /*+ A function to close the connection to the remote server. +*/
}
Protocol;

/* In proto.c */

/*+ The list of protocols. +*/
extern const Protocol Protocols[];

/*+ The number of protocols. +*/
extern int NProtocols;

const Protocol *GetProtocol(const URL *Url);

int IsProtocolHandled(const URL *Url);

int DefaultPort(const URL *Url);

int IsPOSTHandled(const URL *Url);
int IsPUTHandled(const URL *Url);
int IsProxyHandled(const URL *Url);

char /*@observer@*/ *ProxyableLink(URL *Url);

/* In http.c */

char /*@null@*/ /*@only@*/ *HTTP_Open(URL *Url);
char /*@null@*/ /*@only@*/ *HTTP_Request(URL *Url,Header *request_head,Body *request_body);
int   HTTP_ReadHead(/*@out@*/ Header **reply_head);
int   HTTP_ReadBody(char *s,size_t n);
int   HTTP_Close(void);

#if USE_GNUTLS

/* In https.c */

char /*@null@*/ /*@only@*/ *HTTPS_Open(URL *Url);
char /*@null@*/ /*@only@*/ *HTTPS_Request(URL *Url,Header *request_head,Body *request_body);
int   HTTPS_ReadHead(/*@out@*/ Header **reply_head);
int   HTTPS_ReadBody(char *s,size_t n);
int   HTTPS_Close(void);

#endif

/* In ftp.c */

char /*@null@*/ /*@only@*/ *FTP_Open(URL *Url);
char /*@null@*/ /*@only@*/ *FTP_Request(URL *Url,Header *request_head,Body *request_body);
int   FTP_ReadHead(/*@out@*/ Header **reply_head);
int   FTP_ReadBody(char *s,size_t n);
int   FTP_Close(void);

/* In finger.c */

char /*@null@*/ /*@only@*/ *Finger_Open(URL *Url);
char /*@null@*/ /*@only@*/ *Finger_Request(URL *Url,Header *request_head,Body *request_body);
int   Finger_ReadHead(/*@out@*/ Header **reply_head);
int   Finger_ReadBody(char *s,size_t n);
int   Finger_Close(void);

/* In ssl.c */

char /*@null@*/ /*@only@*/ *SSL_Open(URL *Url);
char /*@null@*/ /*@only@*/ *SSL_Request(int client,URL *Url,Header *request_head);
void  SSL_Transfer(int client);
int   SSL_Close(void);

#endif /* PROTO_H */
