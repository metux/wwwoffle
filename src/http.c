/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/http.c 1.44 2006/01/08 10:27:21 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions for getting URLs using HTTP.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


/*+ Set to the name of the proxy if there is one. +*/
static URL /*@null@*/ /*@only@*/ *proxyUrl=NULL;

/*+ The file descriptor of the server. +*/
static int server=-1;


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using HTTP.

  char *HTTP_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTP_Open(URL *Url)
{
 char *msg=NULL;
 char *proxy=NULL;
 char *server_host=NULL;
 int server_port=-1;

 /* Sort out the host. */

 if(!IsLocalNetHost(Url->host))
    proxy=ConfigStringURL(Proxies,Url);

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
    msg=GetPrintMessage(Warning,"Cannot open the HTTP connection to %s port %d; [%!s].",server_host,server_port);
 else
   {
    init_io(server);
    configure_io_timeout(server,ConfigInteger(SocketTimeout),ConfigInteger(SocketTimeout));
   }

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL.

  char *HTTP_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the HTTP request for the URL.

  Body *request_body The body of the HTTP request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTP_Request(URL *Url,Header *request_head,Body *request_body)
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
    msg=GetPrintMessage(Warning,"Failed to write head to remote HTTP %s; [%!s].",proxyUrl?"proxy":"server");
 if(request_body)
    if(write_data(server,request_body->content,request_body->length)==-1)
       msg=GetPrintMessage(Warning,"Failed to write body to remote HTTP %s; [%!s].",proxyUrl?"proxy":"server");

 free(head);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  int HTTP_ReadHead Returns the server socket file descriptor.

  Header **reply_head Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTP_ReadHead(Header **reply_head)
{
 ParseReply(server,reply_head);

 return(server);
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t HTTP_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t HTTP_ReadBody(char *s,size_t n)
{
 return(read_data(server,s,n));
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using HTTP.

  int HTTP_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTP_Close(void)
{
 unsigned long r,w;

 finish_tell_io(server,&r,&w);

 PrintMessage(Inform,"Server bytes; %d Read, %d Written.",r,w); /* Used in audit-usage.pl */

 if(proxyUrl)
    FreeURL(proxyUrl);
 proxyUrl=NULL;

 return(CloseSocket(server));
}
