#!/bin/sh
# -*- perl -*-
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.6d.
#
# A Perl script to find a particular PID in the log file or syslog.
#
# Written by Andrew M. Bishop
#
# This file Copyright 1999,2001 Andrew M. Bishop
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

if [ "x$1" = "x" ]; then
   echo "Usage: $0 pid"
   exit 1
fi

exec perl -x $0 $1

exit 1

#!perl

$pid=$ARGV[0];

$on=0;

while(<STDIN>)
  {
   $line=$_;

   if($line =~ m/wwwoffle[ds]\[([0-9]+)\]:? ([a-zA-Z]+:)?/)
       {
        $on=0;
        $on=1 if($1 eq $pid);
        $on=0 if($2 eq "Timestamp:");
       }

   if($line =~ m/wwwoffle[ds]\[[0-9]+\]:?/ && $line =~ m/pid=([0-9]+)/)
       {
        $on=1 if($1 eq $pid);
       }

   print $line if($on);
  }
