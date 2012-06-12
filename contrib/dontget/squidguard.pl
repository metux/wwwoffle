#!/bin/sh
# -*- perl -*-
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.6d.
#
# A Perl script to convert SquidGuard lists to WWWOFFLE format.
#
# Written by Andrew M. Bishop
#
# This file Copyright 2001 Andrew M. Bishop
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

if [ "x$1" = "x" ]; then
   echo "Usage: $0 filename"
   exit 1
fi

exec perl -x $0 $1

exit 1

#!perl

die "No filename specified\n" if($#ARGV==-1);

$file=$ARGV[0];

# Print a header

print "\n";
print "#\n";
print "# WWWOFFLE DontGet list converted from SquidGuard format by squidguard.pl\n";
print "#\n";
print "# Converted on  : ".scalar localtime()."\n";
print "#\n";
print "# Converted from: $file (".scalar localtime((stat($file))[9]).")\n";
print "#\n";
print "\n";

# Read the file in.

open(FILE,"<$file") || die "Cannot read '$file'\n";

while(<FILE>)
  {
   s/\r*\n//g;

   next if(m/^\#/);
   next if(m/^$/);

   # Assume that there are no regular expressions

   if(m%/%)
       {
        $url=$_;
        $url =~ m%^([^/]+)/%;
        $domain=$1;

        print "*://$url*\n";
        print "*://www*.$url*\n" if($domain && $domain =~ tr/[a-z]/[a-z]/);
       }
   else
       {
        $domain=$_;

        print "*://$domain/*\n";
        print "*://www*.$domain/*\n" if($domain =~ tr/[a-z]/[a-z]/);
       }
  }

close(FILE);
