/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/https.c 1.6 2010/01/19 19:53:23 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9f.
  Functions for getting URLs using HTTPS.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997-2010 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>

#if USE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#endif

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


#if USE_GNUTLS

#include "certificates.h"

/*+ Set to the name of the proxy if there is one. +*/
static URL /*@null@*/ /*@only@*/ *proxyUrl=NULL;

/*+ The file descriptor of the server. +*/
static int server=-1;


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using HTTPS.

  char *HTTPS_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTPS_Open(URL *Url)
{
 char *msg=NULL;
 char *proxy=NULL;
 char *server_host=NULL;
 int server_port=-1;

 /* Sort out the host. */

 if(!IsLocalNetHost(Url->host))
    proxy=ConfigStringURL(SSLProxy,Url);

 if(proxy)
   {
    if(proxyUrl)
       FreeURL(proxyUrl);
    proxyUrl=NULL;

    proxyUrl=CreateURL("http",proxy,"/",NULL,NULL,NULL);
    server_host=proxyUrl->host;
    server_port=proxyUrl->port;
   }
 else
   {
    server_host=Url->host;
    server_port=Url->port;
   }

 if(server_port==-1)
    server_port=DefaultPort(Url);

 /* Open the connection. */

 server=OpenClientSocket(server_host,server_port);

 if(server==-1)
   {
    msg=GetPrintMessage(Warning,"Cannot open the https (SSL) connection to %s port %d; [%!s].",server_host,server_port);
    return(msg);
   }
 else
   {
    init_io(server);
    configure_io_timeout(server,ConfigInteger(SocketTimeout),ConfigInteger(SocketTimeout));
   }

 if(proxy)
   {
    char *head,*hostport;
    int connect_status;
    Header *connect_request,*connect_reply;

    if(Url->port==-1)
      {
       hostport=(char*)malloc(strlen(Url->hostport)+1+3+1);
       strcpy(hostport,Url->hostport);
       strcat(hostport,":443");
      }
    else
       hostport=Url->hostport;

    connect_request=CreateHeader("CONNECT fakeurl HTTP/1.0\r\n",1);
    AddToHeader(connect_request,"Host",hostport);

    MakeRequestProxyAuthorised(proxyUrl,connect_request);

    ChangeURLInHeader(connect_request,Url->hostport);

    head=HeaderString(connect_request);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to https (SSL) proxy)\n%s",head);

    if(write_string(server,head)==-1)
       msg=GetPrintMessage(Warning,"Failed to write to remote https (SSL) proxy; [%!s].");

    if(Url->port==-1)
       free(hostport);
    free(head);
    FreeHeader(connect_request);

    if(msg)
       return(msg);

    connect_status=ParseReply(server,&connect_reply);

    if(StderrLevel==ExtraDebug)
      {
       head=HeaderString(connect_reply);
       PrintMessage(ExtraDebug,"Incoming Reply Head (from https (SSL) proxy)\n%s",head);
       free(head);    
      }

    if(connect_status!=200)
       msg=GetPrintMessage(Warning,"Received error message from https (SSL) proxy; code=%d.",connect_status);

    FreeHeader(connect_reply);

    if(msg)
       return(msg);
   }

 if(configure_io_gnutls(server,Url->hostport,0))
    msg=GetPrintMessage(Warning,"Cannot secure the https (SSL) connection to %s port %d; [%!s].",server_host,server_port);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL.

  char *HTTPS_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the HTTPS request for the URL.

  Body *request_body The body of the HTTPS request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTPS_Request(URL *Url,Header *request_head,Body *request_body)
{
 char *msg=NULL,*head;

 /* Make the request OK for a proxy or not. */

 if(proxyUrl)
    MakeRequestProxyAuthorised(proxyUrl,request_head);
 else
    MakeRequestNonProxy(Url,request_head);

 /* Send the request. */

 head=HeaderString(request_head);

 if(proxyUrl)
    PrintMessage(ExtraDebug,"Outgoing Request Head (to proxy)\n%s",head);
 else
    PrintMessage(ExtraDebug,"Outgoing Request Head (to server)\n%s",head);

 if(write_string(server,head)==-1)
    msg=GetPrintMessage(Warning,"Failed to write head to remote https (SSL) %s; [%!s].",proxyUrl?"proxy":"server");
 if(request_body)
    if(write_data(server,request_body->content,request_body->length)==-1)
       msg=GetPrintMessage(Warning,"Failed to write body to remote https (SSL) %s; [%!s].",proxyUrl?"proxy":"server");

 free(head);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  int HTTPS_ReadHead Returns the server socket file descriptor.

  Header **reply_head Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTPS_ReadHead(Header **reply_head)
{
 ParseReply(server,reply_head);

 return(server);
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t HTTPS_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t HTTPS_ReadBody(char *s,size_t n)
{
 return(read_data(server,s,n));
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using HTTPS.

  int HTTPS_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTPS_Close(void)
{
 unsigned long r,w;

 finish_tell_io(server,&r,&w);

 PrintMessage(Inform,"Server bytes; %d Read, %d Written.",r,w); /* Used in audit-usage.pl */

 if(proxyUrl)
    FreeURL(proxyUrl);
 proxyUrl=NULL;

 return(CloseSocket(server));
}

#endif /* USE_GNUTLS */
