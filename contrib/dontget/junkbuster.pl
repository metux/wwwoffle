#!/bin/sh
# -*- perl -*-
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.6d.
#
# A Perl script to convert JunkBuster lists to WWWOFFLE format.
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
print "# WWWOFFLE DontGet list converted from JunkBuster format by junkbuster.pl\n";
print "#\n";
print "# Converted on  : ".scalar localtime()."\n";
print "#\n";
print "# Converted from: $file (".scalar localtime((stat($file))[9]).")\n";
print "#\n";
print "\n";

# Read the file in (and parse it in reverse).

open(FILE,"<$file") || die "Cannot read '$file'\n";

@lines=(<FILE>);

close(FILE);

foreach $_ (reverse (@lines))
  {
   s/\r*\n//g;
   s/#.*$//g;

   next if(m/^\#/);
   next if(m/^$/);

   if(m/^:/)
       {
        $port=$_;

        print "\n*://*$port/*\n";
       }
   else
       {
        $url=$_;

        $original_url=$url;
        $regexp_found=0;

        # Find negated selection

        if($url =~ m/^~/)
            {
             $negated=1;
             $url=substr($url,1);
            }
        else
            {
             $negated=0;
            }

        # Insert protocol and hostname

        if($url =~ m%^/%)
            {$url="*://*$url";}
        else
            {$url="*://$url";}

        # Replace '/*.*/' with '/*'

        $regexp_found=1 if($url =~ m%/\*\.\*/%);

        $url =~ s%/\*\.\*/%/*%g;

        # Replace '.*' with '*' and '.+' with '*'

        $regexp_found=1 if($url =~ m/\.[*+]/);

        $url =~ s/\.[*+]/*/g;

        # Replace '**' with '*'

        $url =~ s/\*+/*/g;

        # Replace '\.' with '.'

        $regexp_found=1 if($url =~ m/\\\./);

        $url =~ s/\\\././g;

        # Start expanding things.

        @urls=($url);

        # Replace 'ab?c' with 'abc', 'ac'

        $more=1;

        while($more)
            {
             $more=0;

             foreach $url (@urls)
                 {
                  next if(!$url);

                  if($url =~ m/^([^?()]*)(\([^()?]+\))\?(.*)$/)
                      {
                       $start=$1;
                       $mid=$2;
                       $end=$3;

                       push(@urls,$start.$end);
                       push(@urls,$start.$mid.$end);

                       $url='';
                       $more=1;
                       $regexp_found=1;
                      }
                  if($url =~ m/^([^?]*)\?(.*)$/)
                      {
                       $start=substr($1,0,length($1)-1);
                       $mid=substr($1,length($1)-1);
                       $end=$2;

                       push(@urls,$start.$end);
                       push(@urls,$start.$mid.$end);

                       $url='';
                       $more=1;
                       $regexp_found=1;
                      }
                 }
            }

        # Replace 'xxx.(aaa|bbb|ccc).yyy' with 'xxx.aaa.yyy','xxx.bbb.yyy','xxx.ccc.yyy'

        $more=1;

        while($more)
            {
             $more=0;

             foreach $url (@urls)
                 {
                  next if(!$url);

                  next if($url !~ m/^([^()]*)\(([^()]+)\)(.*)$/);

                  $start=$1;
                  $middle=$2;
                  $end=$3;

                  foreach $mid (split('\|',$middle))
                      {
                       push(@urls,$start.$mid.$end);
                      }

                  if($middle =~ m%^\|% || $middle =~ m%\|$%)
                      {
                       push(@urls,$start.$end);
                      }

                  $url='';
                  $more=1;
                  $regexp_found=1;
                 }
            }

        # Print the result

        print "\n";
        print "# $original_url\n" if($regexp_found);

        $lasturl="";

        foreach $url (sort(@urls))
            {
             next if(!$url);
             next if($url eq $lasturl);

             $url="$url*" if($url !~ m%\.[^./]*$%);

             # Too complex regexps

             next if($url =~ m%[][?+(){}]%);
             next if($url =~ m%^\*://[^/]+/(.+)$% && ($path=$1) && ($path =~ y/\*/\*/)>2);

             if($negated)
                 {
                  print "!$url\n";
                 }
             else
                 {
                  print "$url\n";
                 }

             $lasturl=$url;
            }
       }
  }
