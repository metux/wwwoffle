/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/sockets6.c,v 1.20 2007-06-10 08:37:39 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  IPv4+IPv6 Socket manipulation routines.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

#include <fcntl.h>
#include <errno.h>

#include <netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <setjmp.h>
#include <signal.h>

#include "errors.h"
#include "sockets.h"


/* Local functions */

static void sigalarm(int signum);
static struct addrinfo /*@null@*/ *getaddrinfo_or_timeout(char *name,char *port,int ai_flags);
static int getnameinfo_or_timeout(struct sockaddr *addr,size_t len,/*@null@*/ /*@out@*/ char **host,/*@null@*/ /*@out@*/ char **ip,/*@null@*/ /*@out@*/ char **port);
static int connect_or_timeout(int sockfd,struct addrinfo *addr);

/* Local variables */

/*+ The timeout on DNS lookups. +*/
static int timeout_dns=0;

/*+ The timeout for socket connections. +*/
static int timeout_connect=0;

/*+ A longjump variable for aborting DNS lookups. +*/
static jmp_buf dns_jmp_env;

/* Global variables */

/*+ The global IPv6 error number. +*/
int gai_errno=0;


/*++++++++++++++++++++++++++++++++++++++
  Opens a socket for a client.

  int OpenClientSocket Returns the socket file descriptor.

  char* host The name of the remote host.

  int port The socket number.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenClientSocket(char* host,int port)
{
 int s=-1;
 struct addrinfo *server,*ss;
 char hostipstr[INET6_ADDRSTRLEN],*hoststr,portstr[5+1];

 if(*host=='[') /* accept IP addresses inside '[...]' */
   {
    strcpy(hostipstr,host+1);
    hostipstr[strlen(hostipstr)-1]=0;
    hoststr=hostipstr;
   }
 else
    hoststr=host;

 sprintf(portstr,"%d",port&0xffff);

 server=getaddrinfo_or_timeout(hoststr,portstr,0);

 if(!server)
   {
    if(errno!=ETIMEDOUT)
       errno=ERRNO_USE_GAI_ERRNO;
    PrintMessage(Warning,"Unknown host '%s' for server [%!s].",hoststr);
    return(-1);
   }

 for(ss=server;ss;ss=ss->ai_next)
   {
    if((ss->ai_protocol==IPPROTO_TCP || ss->ai_protocol==IPPROTO_IPV6 || ss->ai_protocol==IPPROTO_IP) &&
       (ss->ai_family==AF_INET || ss->ai_family==AF_INET6))
      {
       s=socket(ss->ai_family,ss->ai_socktype,ss->ai_protocol);
       if(s==-1)
         {
          PrintMessage(Inform,"Failed to create %s client socket [%!s].",ss->ai_family==AF_INET?"IPv4":"IPv6");
          continue;
         }

       if(connect_or_timeout(s,ss)==-1)
         {
          close(s);
          s=-1;
          PrintMessage(Inform,"Failed to connect %s socket to '%s' port '%d' [%!s].",ss->ai_family==AF_INET?"IPv4":"IPv6",hoststr,port);
          continue;
         }

       break;
      }
   }

 freeaddrinfo(server);

 if(s==-1)
    PrintMessage(Warning,"Failed to create and connect client socket.");

 return(s);
}


/*++++++++++++++++++++++++++++++++++++++
  Opens a socket for a server.

  int OpenServerSocket Returns a socket to be a server.

  char *host The local IP address to use.

  int port The port number to use.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenServerSocket(char *host,int port)
{
 int s=-1;
 struct addrinfo *server,*ss;
 char hostipstr[INET6_ADDRSTRLEN],*hoststr,portstr[5+1];
 int reuse_addr=1;

 if(*host=='[') /* accept IP addresses inside '[...]' */
   {
    strcpy(hostipstr,host+1);
    hostipstr[strlen(hostipstr)-1]=0;
    hoststr=hostipstr;
   }
 else
    hoststr=host;

 sprintf(portstr,"%d",port&0xffff);

 // debian bug #527235: ->
 if(!strcmp(hoststr,"0.0.0.0") || !strcmp(hoststr,"::"))
   server=getaddrinfo_or_timeout(NULL,portstr,AI_PASSIVE);
 else
   server=getaddrinfo_or_timeout(hoststr,portstr,AI_PASSIVE);
 // <- #527235

 if(!server)
   {
    if(errno!=ETIMEDOUT)
       errno=ERRNO_USE_GAI_ERRNO;
    PrintMessage(Warning,"Unknown host '%s' for server [%!s].",hoststr);
    return(-1);
   }

 for(ss=server;ss;ss=ss->ai_next)
   {
    if((ss->ai_protocol==IPPROTO_TCP || ss->ai_protocol==IPPROTO_IPV6 || ss->ai_protocol==IPPROTO_IP) &&
       (ss->ai_family==AF_INET || ss->ai_family==AF_INET6))
      {
       s=socket(ss->ai_family,ss->ai_socktype,ss->ai_protocol);
       if(s==-1)
         {
          PrintMessage(Warning,"Failed to create %s server socket [%!s].",ss->ai_family==AF_INET?"IPv4":"IPv6");
          freeaddrinfo(server);
          return(-1);
         }

       setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&reuse_addr,sizeof(reuse_addr));

       if(bind(s,ss->ai_addr,server->ai_addrlen)==-1)
         {
          PrintMessage(Warning,"Failed to bind %s server socket to '%s' port '%d' [%!s].",ss->ai_family==AF_INET?"IPv4":"IPv6",hoststr,port);
          freeaddrinfo(server);
          return(-1);
         }

       listen(s,4);

       break;
      }
   }

 freeaddrinfo(server);

 return(s);
}


/*++++++++++++++++++++++++++++++++++++++
  Waits for a connection on the socket.

  int AcceptConnect returns a socket with a remote connection on the end.

  int socket Specifies the socket to check for.
  ++++++++++++++++++++++++++++++++++++++*/

int AcceptConnect(int socket)
{
 int s;

 s=accept(socket,(struct sockaddr*)0,(socklen_t*)0);

 if(s==-1)
    PrintMessage(Warning,"Failed to accept on IPv4/IPv6 server socket [%!s].");

 return(s);
}


/*++++++++++++++++++++++++++++++++++++++
  Determines the hostname and port number used for a socket on the other end.

  int SocketRemoteName Returns 0 on success and -1 on failure.

  int socket Specifies the socket to check.

  char **name Returns the hostname.

  char **ipname Returns the hostname as an IP address.

  int *port Returns the port number.
  ++++++++++++++++++++++++++++++++++++++*/

int SocketRemoteName(int socket,char **name,char **ipname,int *port)
{
 struct sockaddr_in6 server;
 socklen_t length=sizeof(server);
 int retval;

 retval=getpeername(socket,(struct sockaddr*)&server,&length);
 if(retval==-1)
    PrintMessage(Warning,"Failed to get IPv4/IPv6 socket peer name [%!s].");
 else
   {
    char *portstr;

    getnameinfo_or_timeout((struct sockaddr*)&server,length,name,ipname,&portstr);

    if(port)
       *port=atol(portstr);
   }

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Determines the hostname and port number used for a socket on this end.

  int SocketLocalName Returns 0 on success and -1 on failure.

  int socket Specifies the socket to check.

  char **name Returns the hostname.

  char **ipname Returns the hostname as an IP address.

  int *port Returns the port number.
  ++++++++++++++++++++++++++++++++++++++*/

int SocketLocalName(int socket,char **name,char **ipname,int *port)
{
 struct sockaddr_in6 server;
 socklen_t length=sizeof(server);
 int retval;

 retval=getsockname(socket,(struct sockaddr*)&server,&length);
 if(retval==-1)
    PrintMessage(Warning,"Failed to get IPv4/IPv6 socket local name [%!s].");
 else
   {
    char *portstr;

    getnameinfo_or_timeout((struct sockaddr*)&server,length,name,ipname,&portstr);

    if(port)
       *port=atol(portstr);
   }

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Closes a previously opened socket.

  int CloseSocket Returns 0 on success, -1 on error.

  int socket The socket to close
  ++++++++++++++++++++++++++++++++++++++*/

int CloseSocket(int socket)
{
 int retval=close(socket);

 if(retval==-1)
    PrintMessage(Warning,"Failed to close IPv4/IPv6 socket [%!s].");

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Shuts down a previously opened socket cleanly.

  int ShutdownSocket Returns 0 on success, -1 on error.

  int socket The socket to close
  ++++++++++++++++++++++++++++++++++++++*/

int ShutdownSocket(int socket)
{
 struct linger lingeropt;

 shutdown(socket, 1);

 /* Check socket and read until end or error. */

 while(1)
   {
    char buffer[1024];
    int n;
    fd_set readfd;
    struct timeval tv;

    FD_ZERO(&readfd);

    FD_SET(socket,&readfd);

    tv.tv_sec=tv.tv_usec=0;

    n=select(socket+1,&readfd,NULL,NULL,&tv);

    if(n>0)
      {
       n=read(socket,buffer,1024);

       if(n<=0)
          break;
      }
    else if(n==0 || errno!=EINTR)
       break;
   }

 /* Enable lingering */

 lingeropt.l_onoff=1;
 lingeropt.l_linger=15;

 if(setsockopt(socket,SOL_SOCKET,SO_LINGER,&lingeropt,sizeof(lingeropt))<0)
    PrintMessage(Important,"Failed to set socket SO_LINGER option [%!s].");

 return(CloseSocket(socket));
}


/*++++++++++++++++++++++++++++++++++++++
  Get the Fully Qualified Domain Name for the local host.

  char *GetFQDN Return the FQDN.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetFQDN(void)
{
 static char fqdn[256];
 struct addrinfo *server;

 /* Try gethostname(). */

 if(gethostname(fqdn,255)==-1)
   {
    PrintMessage(Warning,"Failed to get hostname [%!s].");
    return(NULL);
   }

 if(strchr(fqdn,'.'))
    return(fqdn);

 /* Try getaddrinfo(). */

 server=getaddrinfo_or_timeout(fqdn,"80",AI_CANONNAME);
 if(!server)
   {
    if(errno!=ETIMEDOUT)
       errno=ERRNO_USE_GAI_ERRNO;
    PrintMessage(Warning,"Failed to get name/IP address for host '%s' [%!s].",fqdn);
    return("localhost");
   }
 else
    if(server->ai_canonname && strchr(server->ai_canonname,'.'))
       strcpy(fqdn,server->ai_canonname);

 freeaddrinfo(server);

 return(fqdn);
}


/*++++++++++++++++++++++++++++++++++++++
  Set the timeout for the DNS system calls.

  int timeout The timeout in seconds.
  ++++++++++++++++++++++++++++++++++++++*/

void SetDNSTimeout(int timeout)
{
 timeout_dns=timeout;
}


/*++++++++++++++++++++++++++++++++++++++
  Set the timeout for the connect system call.

  int timeout The timeout in seconds.
  ++++++++++++++++++++++++++++++++++++++*/

void SetConnectTimeout(int timeout)
{
 timeout_connect=timeout;
}


/*++++++++++++++++++++++++++++++++++++++
  The signal handler for the alarm signal to timeout the DNS lookup.

  int signum The signal number.
  ++++++++++++++++++++++++++++++++++++++*/

static void sigalarm(int signum)
{
 longjmp(dns_jmp_env,1);
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a name lookup with a timeout.

  struct addrinfo *getaddrinfo_or_timeout Returns the host information.

  char *name The name of the host.

  char *port The port number.

  int ai_flags The ai_flags to pass to the function.
  ++++++++++++++++++++++++++++++++++++++*/

static struct addrinfo *getaddrinfo_or_timeout(char *name,char *port,int ai_flags)
{
 struct addrinfo hints,*result;
 struct sigaction action;

 hints.ai_flags=ai_flags|AI_ADDRCONFIG;
 hints.ai_family=AF_UNSPEC;
 hints.ai_socktype=SOCK_STREAM;
 hints.ai_protocol=0;
 hints.ai_addrlen=0;
 hints.ai_canonname=NULL;
 hints.ai_addr=NULL;
 hints.ai_next=NULL;

start:

 if(!timeout_dns)
   {
    gai_errno=getaddrinfo(name,port,&hints,&result);
    if(gai_errno)
       result=NULL;
    return(result);
   }

 /* DNS with timeout */

 action.sa_handler = sigalarm;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
   {
    PrintMessage(Warning, "Failed to set SIGALRM; cancelling timeout for DNS.");
    timeout_dns=0;
    goto start;
   }

 alarm(timeout_dns);

 if(setjmp(dns_jmp_env))
   {
    result=NULL;
    errno=ETIMEDOUT;
   }
 else
   {
    gai_errno=getaddrinfo(name,port,&hints,&result);
    if(gai_errno)
       result=NULL;
   }

 alarm(0);
 action.sa_handler = SIG_IGN;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
    PrintMessage(Warning, "Failed to clear SIGALRM.");

 return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a name lookup with a timeout.

  int getnameinfo_or_timeout Returns the host information.

  struct sockaddr *addr The address of the host.

  size_t len The length of the address.

  char **host Returns the hostname.

  char **ip Returns the IP address.

  char **port Returns the port number.
  ++++++++++++++++++++++++++++++++++++++*/

static int getnameinfo_or_timeout(struct sockaddr *addr,size_t len,char **host,char **ip,char **port)
{
 struct sigaction action;
 static char _host[NI_MAXHOST],_ip[INET6_ADDRSTRLEN],_port[12];

 if(host) *host="(unknown)";
 if(ip)   *ip="(unknown)";
 if(port) *port="0";

start:

 if(!timeout_dns)
   {
    gai_errno=getnameinfo(addr,len,_ip,INET6_ADDRSTRLEN,_port,12,NI_NUMERICHOST|NI_NUMERICSERV);
    if(gai_errno==0)
      {
       if(ip)
          *ip=_ip;
       if(port)
          *port=_port;
      }

    gai_errno=getnameinfo(addr,len,_host,NI_MAXHOST,NULL,0,0);
    if(gai_errno==0)
       if(host)
          *host=_host;

    return(gai_errno);
   }

 /* DNS with timeout */

 action.sa_handler = sigalarm;
 sigemptyset (&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
   {
    PrintMessage(Warning, "Failed to set SIGALRM; cancelling timeout for DNS.");
    timeout_dns=0;
    goto start;
   }

 alarm(timeout_dns);

 if(setjmp(dns_jmp_env))
   {
    errno=ETIMEDOUT;
   }
 else
   {
    gai_errno=getnameinfo(addr,len,_ip,INET6_ADDRSTRLEN,_port,12,NI_NUMERICHOST|NI_NUMERICSERV);
    if(gai_errno==0)
      {
       if(ip)
          *ip=_ip;
       if(port)
          *port=_port;
      }

    gai_errno=getnameinfo(addr,len,_host,NI_MAXHOST,NULL,0,0);
    if(gai_errno==0)
       if(host)
          *host=_host;
   }

 alarm(0);
 action.sa_handler = SIG_IGN;
 sigemptyset (&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
    PrintMessage(Warning, "Failed to clear SIGALRM.");

 return(gai_errno);
}


/*++++++++++++++++++++++++++++++++++++++
  Call the connect function with a timeout.

  int connect_or_timeout Returns an error status.

  int sockfd The socket to be connected.

  struct addrinfo *addr The address information.
  ++++++++++++++++++++++++++++++++++++++*/

static int connect_or_timeout(int sockfd,struct addrinfo *addr)
{
 int noblock,flags;
 int retval;

 if(!timeout_connect)
    return(connect(sockfd,addr->ai_addr,addr->ai_addrlen));

 /* Connect with timeout */

#ifdef O_NONBLOCK
 flags=fcntl(sockfd,F_GETFL,0);
 if(flags!=-1)
    noblock=fcntl(sockfd,F_SETFL,flags|O_NONBLOCK);
 else
    noblock=-1;
#else
 flags=1;
 noblock=ioctl(sockfd,FIONBIO,&flags);
#endif

 retval=connect(sockfd,addr->ai_addr,addr->ai_addrlen);

 if(retval==-1 && noblock!=-1 && errno==EINPROGRESS)
   {
    fd_set writefd;
    struct timeval tv;

    FD_ZERO(&writefd);
    FD_SET(sockfd,&writefd);

    tv.tv_sec=timeout_connect;
    tv.tv_usec=0;

    retval=select(sockfd+1,NULL,&writefd,NULL,&tv);

    if(retval>0)
      {
       socklen_t arglen=sizeof(int);

       if(getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&retval,&arglen)<0)
          retval=errno;

       if(retval!=0)
          errno=retval,retval=-1;
       if(errno==EINPROGRESS)
          errno=ETIMEDOUT;
      }
    else if(retval==0)
       errno=ETIMEDOUT,retval=-1;
   }

 if(retval>=0)
   {
#ifdef O_NONBLOCK
    flags=fcntl(sockfd,F_GETFL,0);
    if(flags!=-1)
       fcntl(sockfd,F_SETFL,flags&~O_NONBLOCK);
#else
    flags=0;
    ioctl(sockfd,FIONBIO,&flags);
#endif
   }

 return(retval);
}
