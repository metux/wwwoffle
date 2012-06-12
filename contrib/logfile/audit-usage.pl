#!/bin/sh
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
#
# A Perl script to get audit information from the standard error output.
#
# Written by Andrew M. Bishop
#
# This file Copyright 1998,99,2000,01,02,03,05,06 Andrew M. Bishop
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

exec perl -x $0

exit 1

#!perl

%commands=(
           "Online","-online",
           "Fetch","-fetch",
           "Offline","-offline",
           "In Autodial Mode","-autodial",
           "Re-Reading Configuration File","-config",
           "Dumping Configuration File","-dump",
           "Purge","-purge",
           "Status","-status",
           "Kill","-kill"
           );

%modes=(
        "-spool","S",
        "-fetch","F",
        "-real","R",
        "-autodial","A"
        );

%statuses=(
           "Internal Page","I",
           "Cached Page Used","C",
           "New Page","N",
           "Forced Reload","F",
           "Modified on Server","M",
           "Unmodified on Server","U",
           "Not Cached","X"
           );

@piddata=();

print "# Mode  : F=Fetch, R=Online (Real), S=Offline (Spool), A=Autodial,\n";
print "#         W=WWWOFFLE Command, T=Timestamp, X=Error Condition\n";
print "#\n";
print "# Status: C=Cached version used, N=New page, F=Forced refresh, I=Internal,\n";
print "#         M=Modified on server, U=Unmodified on server, X=Not cached\n";
print "#         +z=Compressed transfer, +c=Chunked transfer.\n";
print "#\n";
print "# Mode Status\n";
print "# ---- ------\n";
print "# /  ,---'                       Server Bytes  Client Bytes\n";
print "#/  /         Hostname Username     Read Writ Read     Writ Details\n";
print "# ### ---------------- -------- -------- ---- ---- -------- -------\n";

while(<STDIN>)
  {
   if(/^wwwoffled\[([0-9]+)\]/ || /wwwoffled\[([0-9]+)\]:/)
       {
        if(/Timestamp: ([a-zA-Z0-9 :]+)/)
            {
             &PrintLine(mode => "T", string => $1);
            }
        elsif(/WWWOFFLE (Online|Fetch|Offline|In Autodial Mode|Re-Reading Configuration File|Dumping Configuration File|Purge|Kill)\./)
            {
             &PrintLine(mode => "W", host => $host, string => "WWWOFFLE $commands{$1}");
            }
        elsif(/WWWOFFLE (Incorrect Password|Not a command|Unknown control command)/)
            {
             &PrintLine(mode => "X", host => $host, string => "WWWOFFLE Bad Connection($1)");
            }
        elsif(/HTTPS? Proxy connection from host ([^ ]+) /)
            {
             $host=$1;
            }
        elsif(/(HTTPS?) Proxy connection rejected from host ([^ ]+) /)
            {
             &PrintLine(mode => "X", host => $2, string => "$1 Proxy Host Connection Rejected");
            }
        elsif(/WWWOFFLE Connection from host ([^ ]+) /)
            {
             $host=$1;
            }
        elsif(/WWWOFFLE Connection rejected from host ([^ ]+) /)
            {
             &PrintLine(mode => "X", host => $1, string => "WWWOFFLE Connection Rejected");
            }
        elsif(/Forked wwwoffles (-[a-z]+) .pid=([0-9]+)/)
            {
             $pid=$2;
             $mode=$modes{$1};
             $host="" if($mode eq "F");

             ${$piddata[$pid]}{mode}=$mode;
             ${$piddata[$pid]}{host}=$host;
            }
        elsif(/Child wwwoffles (exited|terminated) with status ([0-9]+) .pid=([0-9]+)/)
            {
             $pid=$3;

             &PrintLine(%{$piddata[$pid]});

             undef %{$piddata[$pid]};
            }
       }
   elsif(/^wwwoffles\[([0-9]+)\]/ || /wwwoffles\[([0-9]+)\]:/)
       {
        $pid=$1;

        if(/: (URL|SSL[^=]*)=\'([^\']+)/)
            {
             $url=$2;
             if($1 =~ "SSL") {$url.=" (SSL)";}
             ${$piddata[$pid]}{string}=$url;
             ${$piddata[$pid]}{status}="I";
            }
        elsif(/: Cache Access Status=\'([^\']+)/)
            {
             ${$piddata[$pid]}{status}=$statuses{$1};
            }
        elsif(/: Server has used .Content-Encoding:/)
            {
             ${$piddata[$pid]}{compress}="z";
            }
        elsif(/: Server has used .Transfer-Encoding:/)
            {
             ${$piddata[$pid]}{chunked}="c";
            }
        elsif(/: Server bytes; ([0-9]+) Read, ([0-9]+) Written./)
            {
             ${$piddata[$pid]}{server_rd}=$1;
             ${$piddata[$pid]}{server_wr}=$2;
            }
        elsif(/: Client bytes; ([0-9]+) Read, ([0-9]+) Written./)
            {
             ${$piddata[$pid]}{client_rd}=$1;
             ${$piddata[$pid]}{client_wr}=$2;
            }
        elsif(/HTTP Proxy connection from user '([^ ]+)'/)
            {
             ${$piddata[$pid]}{user}=$1;
            }
        elsif(/No more outgoing requests/)
            {
             ${$piddata[$pid]}{mode}="W";
             ${$piddata[$pid]}{string}="WWWOFFLE fetch finished";
            }
        elsif(/HTTP Proxy connection rejected from unauthenticated user/)
            {
             ${$piddata[$pid]}{mode}="X";
             ${$piddata[$pid]}{string}="HTTP Proxy User Connection Rejected";
            }
        elsif(/(Cannot open temporary spool file|Could not parse HTTP request|The requested method '[^']+' is not supported)/)
            {
             ${$piddata[$pid]}{mode}="X";
             ${$piddata[$pid]}{string}="WWWOFFLE Error($1)";
            }
       }
  }

#
# Print a line
#

sub PrintLine
  {
   my(%params)=@_;

#   foreach $key (keys %params)
#     {
#      print "KEY=$key VALUE=$params{$key}\n";
#     }

   foreach $variable (mode,status,compress,chunked,host,user,server_wr,server_rd,client_wr,client_rd,string)
     {
      next if($params{$variable});
      $params{$variable}='-';
      $params{$variable}='' if($variable eq compress || $variable eq chunked);
     }

   printf("%1s %1s%-2s %16s %8s %8s %4s %4s %8s %s\n",
          $params{mode},$params{status},$params{compress}.$params{chunked},
          $params{host},$params{user},
          $params{server_rd},$params{server_wr},$params{client_rd},$params{client_wr},
          $params{string});
  }
