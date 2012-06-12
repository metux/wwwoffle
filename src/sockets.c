/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/sockets.c,v 1.1 2001-09-05 18:47:31 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7.
  A dummy file that includes either the IPv4 or IPv4+IPv6 socket functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 2001 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#if USE_IPV6

#include "sockets6.c"

#else

#include "sockets4.c"

#endif
