/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/ssl.c 1.32 2006/01/10 19:25:38 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  SSL (Secure Socket Layer) Tunneling functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02,03,04,05,06 Andrew M. Bishop
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
  Open a connection to get a URL using SSL tunnelling.

  char *SSL_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open (used for host only).
  ++++++++++++++++++++++++++++++++++++++*/

char *SSL_Open(URL *Url)
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

 server=-1;

 if(server_port)
   {
    server=OpenClientSocket(server_host,server_port);

    if(server==-1)
       msg=GetPrintMessage(Warning,"Cannot open the SSL connection to %s port %d; [%!s].",server_host,server_port);
    else
      {
       init_io(server);
       configure_io_timeout(server,ConfigInteger(SocketTimeout),ConfigInteger(SocketTimeout));
      }
   }
 else
    msg=GetPrintMessage(Warning,"No port given for the SSL connection to %s.",server_host);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL and reply to the client.

  char *SSL_Request Returns NULL on success, a useful message on error.

  int client The client socket.

  URL *Url The URL to get (used for host only).

  Header *request_head The head of the HTTP request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *SSL_Request(int client,URL *Url,Header *request_head)
{
 char *msg=NULL;

 if(proxyUrl)
   {
    char *head;

    ModifyRequest(Url,request_head);

    MakeRequestProxyAuthorised(proxyUrl,request_head);

    ChangeURLInHeader(request_head,Url->hostport);

    head=HeaderString(request_head);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to SSL proxy)\n%s",head);

    if(write_string(server,head)==-1)
       msg=GetPrintMessage(Warning,"Failed to write to remote SSL proxy; [%!s].");

    free(head);
   }
 else
    write_string(client,"HTTP/1.0 200 WWWOFFLE SSL OK\r\n\r\n");

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Perform the transfer between client and proxy/server.

  int client The client socket.
  ++++++++++++++++++++++++++++++++++++++*/

void SSL_Transfer(int client)
{
 int nfd=client>server?client+1:server+1;
 fd_set readfd;
 struct timeval tv;
 int n,nc,ns;
 char buffer[IO_BUFFER_SIZE];

 while(1)
   {
    nc=ns=0;

    FD_ZERO(&readfd);

    FD_SET(server,&readfd);
    FD_SET(client,&readfd);

    tv.tv_sec=ConfigInteger(SocketTimeout);
    tv.tv_usec=0;

    n=select(nfd,&readfd,NULL,NULL,&tv);

    if(n<0 && errno==EINTR)
       continue;
    else if(n<=0)
       return;

    if(FD_ISSET(client,&readfd))
      {
       nc=read_data(client,buffer,IO_BUFFER_SIZE);
       if(nc>0)
          write_data(server,buffer,nc);
      }
    if(FD_ISSET(server,&readfd))
      {
       ns=read_data(server,buffer,IO_BUFFER_SIZE);
       if(ns>0)
          write_data(client,buffer,ns);
      }

    if(nc==0 && ns==0)
       return;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using SSL.

  int SSL_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int SSL_Close(void)
{
 unsigned long r,w;

 finish_tell_io(server,&r,&w);

 PrintMessage(Inform,"Server bytes; %d Read, %d Written.",r,w); /* Used in audit-usage.pl */

 if(proxyUrl)
    FreeURL(proxyUrl);
 proxyUrl=NULL;

 return(CloseSocket(server));
}
