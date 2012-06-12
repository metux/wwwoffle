/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/ftp.c,v 1.84 2006-01-08 10:27:21 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions for getting URLs using FTP.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


/*+ Set this to 1 to see the full dialog with the FTP server. +*/
#define DEBUG_FTP 0

/*+ Set to the name of the proxy if there is one. +*/
static URL /*@null@*/ /*@only@*/ *proxyUrl=NULL;

/*+ The file descriptor of the socket +*/
static int server_ctrl=-1,      /*+ for the control connection to the server. +*/
           server_data=-1;      /*+ for the data connection to the server. +*/

/*+ A header to contain the reply. +*/
static /*@only@*/ /*@null@*/ Header *bufferhead=NULL;

/*+ A buffer to contain the reply body. +*/
static char /*@only@*/ /*@null@*/ *buffer=NULL;
/*+ A buffer to contain the reply tail. +*/
static char /*@only@*/ /*@null@*/ *buffertail=NULL;

/*+ The number of characters in the buffer +*/
static int nbuffer=0,           /*+ in total for the body part. +*/
           nread=0,             /*+ that have been read for the body. +*/
           nbuffertail=0,       /*+ in total for the tail . +*/
           nreadtail=0;         /*+ that have been read from the tail. +*/

static /*@null@*/ char *htmlise_dir_entry(void);


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using FTP.

  char *FTP_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *FTP_Open(URL *Url)
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

 server_ctrl=OpenClientSocket(server_host,server_port);

 if(server_ctrl==-1)
    msg=GetPrintMessage(Warning,"Cannot open the FTP control connection to %s port %d; [%!s].",server_host,server_port);
 else
   {
    init_io(server_ctrl);
    configure_io_timeout(server_ctrl,ConfigInteger(SocketTimeout),ConfigInteger(SocketTimeout));
   }

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL.

  char *FTP_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the HTTP request for the URL.

  Body *request_body The body of the HTTP request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *FTP_Request(URL *Url,Header *request_head,Body *request_body)
{
 char *msg=NULL,*str=NULL;
 char *path,*file=NULL;
 char *host,*mimetype="text/html";
 char *msg_reply=NULL;
 char *timestamp,sizebuf[MAX_INT_STR+1];
 int i,l,port;
 time_t modtime=0;
 char *user,*pass;

 /* Initial setting up. */

 sizebuf[0]=0;

 buffer=NULL;
 bufferhead=NULL;
 buffertail=NULL;

 /* Take a simple route if it is proxied. */

 if(proxyUrl)
   {
    char *head;

    /* Make the request OK for a proxy. */

    MakeRequestProxyAuthorised(proxyUrl,request_head);

    /* Send the request. */

    head=HeaderString(request_head);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to proxy)\n%s",head);

    if(write_string(server_ctrl,head)==-1)
       msg=GetPrintMessage(Warning,"Failed to write head to remote FTP proxy; [%!s].");
    if(request_body)
       if(write_data(server_ctrl,request_body->content,request_body->length)==-1)
          msg=GetPrintMessage(Warning,"Failed to write body to remote FTP proxy; [%!s].");

    free(head);

    return(msg);
   }

 /* Else Sort out the path. */

 path=URLDecodeGeneric(Url->path);
 if(path[strlen(path)-1]=='/')
   {
    path[strlen(path)-1]=0;
    file=NULL;
   }
 else
    for(i=strlen(path)-1;i>=0;i--)
       if(path[i]=='/')
         {
          path[i]=0;
          file=&path[i+1];
          break;
         }

 if(Url->user)
   {
    user=Url->user;
    pass=Url->pass;
   }
 else
   {
    user=ConfigStringURL(FTPAuthUser,Url);
    if(!user)
       user=ConfigString(FTPUserName);

    pass=ConfigStringURL(FTPAuthPass,Url);
    if(!pass)
       pass=ConfigString(FTPPassWord);
   }

 /* send all the RFC959 commands. */

 server_data=-1;

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: connected; got: %s",str);

    if(!file && !*path && str && isdigit(str[0]) && isdigit(str[1]) && isdigit(str[2]) && str[3]=='-')
      {
       if(msg_reply)
         {
          msg_reply=(char*)realloc((void*)msg_reply,strlen(msg_reply)+strlen(str));
          strcat(msg_reply,str+4);
         }
       else
         {
          msg_reply=(char*)malloc(strlen(str));
          strcpy(msg_reply,str+4);
         }
      }
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server when connected; timed out?");
    free(path);
    return(msg);
   }

 if(atoi(str)!=220)
   {
    char *p=str+strlen(str)-1;
    while(*p=='\n' || *p=='\r') *p--=0;
    msg=GetPrintMessage(Warning,"Got '%s' message when connected to FTP server.",str);
    free(path);
    free(str);
    return(msg);
   }

 /* Login */

 if(write_formatted(server_ctrl,"USER %s\r\n",user)==-1)
   {
    msg=GetPrintMessage(Warning,"Failed to write 'USER' command to remote FTP host; [%!s].");
    free(path);
    free(str);
    return(msg);
   }

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'USER %s'; got: %s",user,str);
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server to 'USER' command; timed out?");
    free(path);
    return(msg);
   }

 if(atoi(str)!=230 && atoi(str)!=331)
   {
    char *p=str+strlen(str)-1;
    while(*p=='\n' || *p=='\r') *p--=0;
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'USER' command to FTP server.",str);
    free(path);
    free(str);
    return(msg);
   }

 if(atoi(str)==331)
   {
    if(write_formatted(server_ctrl,"PASS %s\r\n",pass)==-1)
      {
       msg=GetPrintMessage(Warning,"Failed to write 'PASS' command to remote FTP host; [%!s].");
       free(path);
       free(str);
       return(msg);
      }

    do
      {
       str=read_line(server_ctrl,str);
       if(str)
          PrintMessage(ExtraDebug,"FTP: sent 'PASS %s'; got: %s",pass,str);

       if(!file && !*path && str && isdigit(str[0]) && isdigit(str[1]) && isdigit(str[2]) && str[3]=='-')
         {
          if(msg_reply)
            {
             msg_reply=(char*)realloc((void*)msg_reply,strlen(msg_reply)+strlen(str));
             strcat(msg_reply,str+4);
            }
          else
            {
             msg_reply=(char*)malloc(strlen(str));
             strcpy(msg_reply,str+4);
            }
         }
      }
    while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

    if(!str)
      {
       msg=GetPrintMessage(Warning,"No reply from FTP server to 'PASS' command; timed out?");
       free(path);
       return(msg);
      }

    if(atoi(str)==530)
      {
       bufferhead=CreateHeader("HTTP/1.0 401 FTP invalid password",0);

       goto near_end;
      }

    if(atoi(str)!=202 && atoi(str)!=230)
      {
       char *p=str+strlen(str)-1;
       while(*p=='\n' || *p=='\r') *p--=0;
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'PASS' command to FTP server.",str);
       free(path);
       free(str);
       return(msg);
      }
   }

 /* Change directory */

 if(write_formatted(server_ctrl,"CWD %s\r\n",*path?path:"/")==-1)
   {
    msg=GetPrintMessage(Warning,"Failed to write 'CWD' command to remote FTP host; [%!s].");
    free(path);
    free(str);
    return(msg);
   }

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'CWD %s' got: %s",*path?path:"/",str);

    if(!file && str && isdigit(str[0]) && isdigit(str[1]) && isdigit(str[2]) && str[3]=='-')
      {
       if(msg_reply)
         {
          msg_reply=(char*)realloc((void*)msg_reply,strlen(msg_reply)+strlen(str));
          strcat(msg_reply,str+4);
         }
       else
         {
          msg_reply=(char*)malloc(strlen(str));
          strcpy(msg_reply,str+4);
         }
      }
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server to 'CWD' command; timed out?");
    free(path);
    return(msg);
   }

 if(atoi(str)!=250)
   {
    char *p=str+strlen(str)-1;
    while(*p=='\n' || *p=='\r') *p--=0;
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'CWD' command to FTP server.",str);
    free(path);
    free(str);
    return(msg);
   }

 /* Change directory again to see if file is a dir. */

 if(file)
   {
    if(write_formatted(server_ctrl,"CWD %s\r\n",file)==-1)
      {
       msg=GetPrintMessage(Warning,"Failed to write 'CWD' command to remote FTP host; [%!s].");
       free(path);
       free(str);
       return(msg);
      }

    do
      {
       str=read_line(server_ctrl,str);
       if(str)
          PrintMessage(ExtraDebug,"FTP: sent 'CWD %s' got: %s",file,str);
      }
    while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

    if(!str)
      {
       msg=GetPrintMessage(Warning,"No reply from FTP server to 'CWD' command; timed out?");
       free(path);
       return(msg);
      }

    if(atoi(str)!=250 && atoi(str)!=501 && atoi(str)!=530 && atoi(str)!=550)
      {
       char *p=str+strlen(str)-1;
       while(*p=='\n' || *p=='\r') *p--=0;
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'CWD' command to FTP server.",str);
       free(path);
       free(str);
       return(msg);
      }

    if(atoi(str)==250)
      {
       char *loc=(char*)malloc(strlen(Url->file)+2);

       strcpy(loc,Url->file);
       strcat(loc,"/");

       bufferhead=CreateHeader("HTTP/1.0 302 FTP is a directory",0);
       AddToHeader(bufferhead,"Location",loc);
       AddToHeader(bufferhead,"Content-Type",mimetype);

       buffer=HTMLMessageString("Redirect",
                                "location",loc,
                                NULL);

       free(loc);

       goto near_end;
      }
   }

 /* Set mode to binary or ASCII */

 if(write_formatted(server_ctrl,"TYPE %c\r\n",file?'I':'A')==-1)
   {
    msg=GetPrintMessage(Warning,"Failed to write 'TYPE' command to remote FTP host; [%!s].");
    free(path);
    free(str);
    return(msg);
   }

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'TYPE %c'; got: %s",file?'I':'A',str);
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server to 'TYPE' command; timed out?");
    free(path);
    return(msg);
   }

 if(atoi(str)!=200)
   {
    char *p=str+strlen(str)-1;
    while(*p=='\n' || *p=='\r') *p--=0;
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'TYPE' command to FTP server.",str);
    free(path);
    free(str);
    return(msg);
   }

 if(!strcmp(request_head->method,"GET"))
   {
    /* Try and get the size and modification time. */

    if(file)
      {
       if(write_formatted(server_ctrl,"SIZE %s\r\n",file)==-1)
         {
          msg=GetPrintMessage(Warning,"Failed to write 'SIZE' command to remote FTP host; [%!s].");
          free(path);
          free(str);
          return(msg);
         }

       do
         {
          str=read_line(server_ctrl,str);
          if(str)
             PrintMessage(ExtraDebug,"FTP: sent 'SIZE %s'; got: %s",file,str);
         }
       while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

       if(!str)
         {
          msg=GetPrintMessage(Warning,"No reply from FTP server to 'SIZE' command; timed out?");
          free(path);
          return(msg);
         }

       if(atoi(str)==213)
          if(str[4])
             sprintf(sizebuf,"%ld",(long)atoi(str+4));

       if(write_formatted(server_ctrl,"MDTM %s\r\n",file)==-1)
         {
          msg=GetPrintMessage(Warning,"Failed to write 'MDTM' command to remote FTP host; [%!s].");
          free(path);
          free(str);
          return(msg);
         }

       do
         {
          str=read_line(server_ctrl,str);
          if(str)
             PrintMessage(ExtraDebug,"FTP: sent 'MDTM %s'; got: %s",file,str);
         }
       while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

       if(!str)
         {
          msg=GetPrintMessage(Warning,"No reply from FTP server to 'MDTM' command; timed out?");
          free(path);
          return(msg);
         }

       if(atoi(str)==213)
         {
          int year,mon,mday,hour,min,sec;

          if(sscanf(str,"%*d %4d%2d%2d%2d%2d%2d",&year,&mon,&mday,&hour,&min,&sec)==6)
            {
             struct tm modtm;

             memset(&modtm,0,sizeof(modtm));
             modtm.tm_year=year-1900;
             modtm.tm_mon=mon-1;
             modtm.tm_mday=mday;
             modtm.tm_hour=hour;
             modtm.tm_min=min;
             modtm.tm_sec=sec;

             modtime=mktime(&modtm);
            }
         }
      }
   }

 /* Create the data connection. */

 if(write_string(server_ctrl,"EPSV\r\n")==-1)
   {
    msg=GetPrintMessage(Warning,"Failed to write 'EPSV' command to remote FTP host; [%!s].");
    free(path);
    free(str);
    return(msg);
   }

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'EPSV'; got: %s",str);
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server to 'EPSV' command; timed out?");
    free(path);
    return(msg);
   }

 if(atoi(str)==229)
   {
    host=strchr(str,'(');
    if(!host || sscanf(host+1,"%*c%*c%*c%d%*c",&port)!=1)
      {
       char *p=str+strlen(str)-1;
       while(*p=='\n' || *p=='\r') *p--=0;
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'EPSV' command, cannot parse.",str);
       free(path);
       free(str);
       return(msg);
      }
    if(SocketRemoteName(server_ctrl,&host,NULL,NULL))
      {
       msg=GetPrintMessage(Warning,"Cannot determine server address.");
       free(path);
       free(str);
       return(msg);
      }
   }
 else if(str[0]!='5' || str[1]!='0' || !isdigit(str[2]))
   {
    char *p=str+strlen(str)-1;

    while(*p=='\n' || *p=='\r') *p--=0;
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'EPSV' command",str);
    free(path);
    free(str);
    return(msg);
   }
 else
   {
    int port_l,port_h;

    /* Let's try PASV instead then */

    if(write_string(server_ctrl,"PASV\r\n")==-1)
      {
       msg=GetPrintMessage(Warning,"Failed to write 'PASV' command to remote FTP host; [%!s].");
       free(path);
       free(str);
       return(msg);
      }

    do
      {
       str=read_line(server_ctrl,str);
       if(str)
          PrintMessage(ExtraDebug,"FTP: sent 'PASV'; got: %s",str);
      }
    while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

    if(!str)
      {
       msg=GetPrintMessage(Warning,"No reply from FTP server to 'PASV' command; timed out?");
       free(path);
       return(msg);
      }

    if(atoi(str)!=227)
      {
       char *p=str+strlen(str)-1;
       while(*p=='\n' || *p=='\r') *p--=0;
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'PASV' command",str);
       free(path);
       free(str);
       return(msg);
      }

    if((host=strchr(str,',')))
      {
       while(isdigit(*--host));
       host++;
      }

    if(!host || sscanf(host,"%*d,%*d,%*d,%*d%n,%d,%d",&l,&port_h,&port_l)!=2)
      {
       char *p=str+strlen(str)-1;
       while(*p=='\n' || *p=='\r') *p--=0;
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'PASV' command, cannot parse.",str);
       free(path);
       free(str);
       return(msg);
      }

    port=port_l+256*port_h;

    host[l]=0;
    for(;l>0;l--)
       if(host[l]==',')
          host[l]='.';
   }

 server_data=OpenClientSocket(host,port);

 if(server_data==-1)
   {
    msg=GetPrintMessage(Warning,"Cannot open the FTP data connection [%!s].");
    free(path);
    free(str);
    return(msg);
   }
 else
   {
    init_io(server_data);
    configure_io_timeout(server_data,ConfigInteger(SocketTimeout),ConfigInteger(SocketTimeout));
   }

 /* Make the request */

 if(!strcmp(request_head->method,"GET"))
   {
    char *command;

    if(file)
      {
       command="RETR";

       if(write_formatted(server_ctrl,"RETR %s\r\n",file)==-1)
         {
          msg=GetPrintMessage(Warning,"Failed to write 'RETR' command to remote FTP host; [%!s].");
          free(path);
          free(str);
          return(msg);
         }

       do
         {
          str=read_line(server_ctrl,str);
          if(str)
             PrintMessage(ExtraDebug,"FTP: sent 'RETR %s'; got: %s",file,str);
         }
       while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

       mimetype=WhatMIMEType(file);
      }
    else
      {
       int i;

       for(i=0;i<2;i++)
         {
          if(i==0)
             command="LIST -a";
          else
             command="LIST";

          if(write_formatted(server_ctrl,"%s\r\n",command)==-1)
            {
             msg=GetPrintMessage(Warning,"Failed to write '%s' command to remote FTP host; [%!s].",command);
             free(path);
             free(str);
             return(msg);
            }

          do
            {
             str=read_line(server_ctrl,str);
             if(str)
                PrintMessage(ExtraDebug,"FTP: sent '%s'; got: %s",command,str);
            }
          while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

          if(i==0 && str && (atoi(str)!=150 && atoi(str)!=125))
            {
             char *p=str+strlen(str)-1;
             while(*p=='\n' || *p=='\r') *p--=0;
             PrintMessage(Warning,"Got '%s' message after sending '%s' command to FTP server.",str,command);
            }
          else
             break;
         }
      }

    if(str && (atoi(str)==150 || atoi(str)==125))
      {
       char *p=str;
       while(*p!='\r' && *p!='\n')
          p++;
       *p=0;
      }
    else
      {
       if(str)
         {
          char *p=str+strlen(str)-1;
          while(*p=='\n' || *p=='\r') *p--=0;
          msg=GetPrintMessage(Warning,"Got '%s' message after sending '%s' command to FTP server.",str,command);
          free(str);
         }
       else
          msg=GetPrintMessage(Warning,"No reply from FTP server to '%s' command; timed out?",command);
       free(path);
       return(msg);
      }
   }
 else if(file) /* PUT */
   {
    if(file)
      {
       if(write_formatted(server_ctrl,"STOR %s\r\n",file)==-1)
         {
          msg=GetPrintMessage(Warning,"Failed to write 'STOR' command to remote FTP host; [%!s].");
          free(path);
          free(str);
          return(msg);
         }

       do
         {
          str=read_line(server_ctrl,str);
          if(str)
             PrintMessage(ExtraDebug,"FTP: sent 'STOR %s'; got: %s",file,str);
         }
       while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));
      }
    else
      {
       msg=GetPrintMessage(Warning,"Cannot use the PUT method on a directory name");
       free(path);
       free(str);
       return(msg);
      }

    if(str && (atoi(str)==150 || atoi(str)==125))
      {
       char *p=str;
       while(*p!='\r' && *p!='\n')
          p++;
       *p=0;
      }
    else
      {
       if(str)
         {
          char *p=str+strlen(str)-1;
          while(*p=='\n' || *p=='\r') *p--=0;
          msg=GetPrintMessage(Warning,"Got '%s' message after sending 'STOR' command to FTP server.",str);
          free(str);
         }
       else
          msg=GetPrintMessage(Warning,"No reply from FTP server to 'STOR' command; timed out?");
       free(path);
       return(msg);
      }

    write_data(server_data,request_body->content,request_body->length);
   }

 /* Prepare the HTTP header. */

 bufferhead=CreateHeader("HTTP/1.0 200 FTP Proxy OK",0);
 AddToHeader(bufferhead,"Content-Type",mimetype);

 if(!strcmp(request_head->method,"GET"))
   {
    if(*sizebuf)
       AddToHeader(bufferhead,"Content-Length",sizebuf);

    if(modtime!=0)
      {
       char *ims;

       timestamp=RFC822Date(modtime,1);
       AddToHeader(bufferhead,"Last-Modified",timestamp);

       if((ims=GetHeader(request_head,"If-Modified-Since")))
         {
          time_t lastmodtime=DateToTimeT(ims);

          if(modtime<=lastmodtime)
            {
             bufferhead->status=304;
             RemoveFromHeader(bufferhead,"Content-Length");

             goto near_end;
            }
         }
      }

    if(!file)
      {
       if(msg_reply)
         {
          char *old_msg_reply=msg_reply;
          msg_reply=HTMLString(msg_reply,0);
          free(old_msg_reply);
         }

       buffer=HTMLMessageString("FTPDir-Head",
                                "url",Url->name,
                                "base",Url->file,
                                "message",msg_reply,
                                NULL);

       buffertail=HTMLMessageString("FTPDir-Tail",
                                    NULL);
      }
   }
 else
    buffer=HTMLMessageString("FTPPut",
                             "url",Url->name,
                             NULL);

near_end:

 if(msg_reply)
    free(msg_reply);

 nbuffer=buffer?strlen(buffer):0; nread=0;
 nbuffertail=buffertail?strlen(buffertail):0; nreadtail=0;

 free(path);
 free(str);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  int FTP_ReadHead Returns the server socket file descriptor.

  Header **reply_head Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

int FTP_ReadHead(Header **reply_head)
{
 /* Take a simple route if it is proxied. */

 if(proxyUrl)
   {
    ParseReply(server_ctrl,reply_head);

    return(server_ctrl);
   }
 
 /* Else send the header. */

 *reply_head=bufferhead;

 return(server_data);
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t FTP_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t FTP_ReadBody(char *s,size_t n)
{
 int m=0;

 /* Take a simple route if it is proxied. */

 if(proxyUrl)
    return(read_data(server_ctrl,s,n));
 
 /* Else send the data then the tail. */

 if(server_data==-1)            /* Redirection */
   {
    for(;nread<nbuffer && m<n;nread++,m++)
       s[m]=buffer[nread];

    for(;nreadtail<nbuffertail && m<n;nreadtail++,m++)
       s[m]=buffertail[nreadtail];
   }
 else if(!buffer && !buffertail) /* File not dir entry. */
    m=read_data(server_data,s,n);
 else if(buffer && buffertail)  /* Middle of dir entry */
   {
    for(;nread<nbuffer && m<n;nread++,m++)
       s[m]=buffer[nread];

    if(nread==nbuffer)
      {
       free(buffer);
       buffer=htmlise_dir_entry();
       if(buffer)
          nbuffer=strlen(buffer),nread=0;
      }
   }
 else if(!buffer && buffertail) /* End of dir entry. */
   {
    for(;nreadtail<nbuffertail && m<n;nreadtail++,m++)
       s[m]=buffertail[nreadtail];
   }
 else /* if(buffer && !buffertail) */ /* Done a PUT */
   {
    for(;nread<nbuffer && m<n;nread++,m++)
       s[m]=buffer[nread];
   }

 return(m);
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using FTP.

  int FTP_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int FTP_Close(void)
{
 int err=0;
 char *str=NULL;
 unsigned long r1=0,w1=0,r2=0,w2=0;

 /* Take a simple route if it is proxied. */

 if(proxyUrl)
   {
    finish_tell_io(server_ctrl,&r1,&w1);

    PrintMessage(Inform,"Server bytes; %d Read, %d Written.",r1,w1); /* Used in audit-usage.pl */

    return(CloseSocket(server_ctrl));
   }

 /* Else say goodbye and close all of the sockets, */

 if(server_data!=-1)
   {
    finish_tell_io(server_data,&r2,&w2);
    CloseSocket(server_data);
   }

 write_string(server_ctrl,"QUIT\r\n");

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'QUIT'; got: %s",str);
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 finish_tell_io(server_ctrl,&r1,&w1);
 err=CloseSocket(server_ctrl);

 PrintMessage(Inform,"Server bytes; %d Read, %d Written.",r1+r2,w1+w2); /* Used in audit-usage.pl */

 if(str)
    free(str);

 if(buffer)
    free(buffer);
 if(buffertail)
    free(buffertail);

 if(proxyUrl)
    FreeURL(proxyUrl);
 proxyUrl=NULL;

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a line from the ftp server dir listing into a pretty listing.

  char *htmlise_dir_entry Returns the next line.
  ++++++++++++++++++++++++++++++++++++++*/

static char *htmlise_dir_entry(void)
{
 int i,isdir=0,islink=0;
 char *q,*p[16],*l;
 int file=0,link=0;

 l=read_line(server_data,NULL);

 if(!l)
    return(l);

 p[0]=l;

 for(q=l,i=0;*q && i<16;i++)
   {
    while(*q && isspace(*q))
       q++;
    if(*q)
       p[i]=q;  /* p[] contains a list of words in the line */
    else
       break;
    while(*q && !isspace(*q))
       q++;
   }

 if((*p[0]=='-' || *p[0]=='d' || *p[0]=='l') && i>=8) /* A UNIX 'ls -l' listing. */
   {
#ifdef SPACES_IN_FILENAMES_PS /* work in progress */
    if (i > 9) {
      char *l2, *p2, *p1 = l;
      p2 = l2 = malloc(strlen(l) + (i-9) * 2 + 1);    /* spaces -> "%20", + NULL */
      while (*p1) {
        if (p1 < p[8] || *p1 != ' ')
          *p2 = *p1;
        else {
          *p2++ = '%';
          *p2++ = '2';
          *p2   = '0';
        }
        p1++;
        p2++;
      }
      *p2 = 0;
      free(l);
      l = l2;
      i = 9;
    }
 syslog(6, "l = %s, i = %d, file = %d, link = %d", l, i, file, link); /* XXX */
#endif
    if(i==8)
      {file=7; link=file;}
    else if(i==10 && (*p[8]=='-' && *(p[8]+1)=='>'))
      {file=7; link=9;}
    else if(i==9)
      {file=8; link=file;}
    else if(i==11 && (*p[9]=='-' && *(p[9]+1)=='>'))
      {file=8; link=10;}

    if(*p[0]=='d')
       isdir=1;

    if(*p[0]=='l')
       islink=1;
   }

 if(file)
   {
    char *endf,endfc,*endl,endlc,*ll,*fileurlenc,*linkurlenc;
    char *h;

    /* Get the filename and link URLs. */

    endf=p[file];
    while(*endf && !isspace(*endf))
       endf++;
    endfc=*endf;

    endl=p[link];
    while(*endl && !isspace(*endl))
       endl++;
    endlc=*endl;

    *endf=0;
    fileurlenc=URLEncodePath(p[file]);
    *endf=endfc;

    *endl=0;
    linkurlenc=URLEncodePath(p[link]);
    *endl=endlc;

    /* Sanitise the string. */

    h=HTMLString(l,0);
    for(q=h,i=0;*q && i<16;i++)
      {
       while(*q && isspace(*q))
          q++;
       if(*q)
          p[i]=q;
       else
          break;
       while(*q && !isspace(*q))
          q++;
      }

    endf=p[file];
    while(*endf && !isspace(*endf))
       endf++;
    endfc=*endf;

    endl=p[link];
    while(*endl && !isspace(*endl))
       endl++;
    endlc=*endl;

    ll=(char*)malloc(strlen(h)+strlen(fileurlenc)+strlen(linkurlenc)+40);

    /* Create the line. */

    strncpy(ll,h,(size_t)(p[file]-h));
    strcpy(ll+(p[file]-h),"<a href=\"");
    strcat(ll,"./");
    strcat(ll,fileurlenc);
    if(isdir && *(endl-1)!='/')
       strcat(ll,"/");
    strcat(ll,"\">");
    *endf=0;
    strcat(ll,p[file]);
    *endf=endfc;
    strcat(ll,"</a>");

    if(islink)
      {
       strncat(ll,endf,(size_t)(p[link]-endf));
       strcat(ll,"<a href=\"");
       if(strncmp(linkurlenc,"../",(size_t)3) && strncmp(linkurlenc,"./",(size_t)2) && strncmp(linkurlenc,"/",(size_t)1))
          strcat(ll,"./");
       strcat(ll,linkurlenc);
       strcat(ll,"\">");
       *endl=0;
       strcat(ll,p[link]);
       *endl=endlc;
       strcat(ll,"</a>");

       strcat(ll,endl);
      }
    else
      {
       strcat(ll,endf);
      }

    free(fileurlenc);
    free(linkurlenc);
    free(l);
    free(h);
    l=ll;
   }

 return(l);
}
