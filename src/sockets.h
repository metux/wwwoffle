/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/sockets.h 2.12 2004/01/17 16:22:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Socket function header file.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef SOCKETS_H
#define SOCKETS_H    /*+ To stop multiple inclusions. +*/

/* in sockets.c */

int OpenClientSocket(char* host, int port);

int OpenServerSocket(char* host,int port);
int AcceptConnect(int socket);

int SocketRemoteName(int socket,/*@out@*/ /*@null@*/ char **name,/*@out@*/ /*@null@*/ char **ipname,/*@out@*/ /*@null@*/ int *port);
int SocketLocalName(int socket,/*@out@*/ /*@null@*/ char **name,/*@out@*/ /*@null@*/ char **ipname,/*@out@*/ /*@null@*/ int *port);

int /*@alt void@*/ CloseSocket(int socket);
int /*@alt void@*/ ShutdownSocket(int socket);

char /*@null@*/ *GetFQDN(void);

void SetDNSTimeout(int timeout);
void SetConnectTimeout(int timeout);

#endif /* SOCKETS_H */
