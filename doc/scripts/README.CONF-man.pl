#!/usr/bin/perl

#
# Copyright Andrew M. Bishop 1996.97,98,2001.
#
# $Header: /home/amb/wwwoffle/doc/scripts/RCS/README.CONF-man.pl 1.2 2003/06/15 11:08:22 amb Exp $
#
# Usage: README.CONF-man.pl wwwoffle.conf.man.template < README.CONF > wwwoffle.conf.man
#

die "Usage: $0 wwwoffle.conf.man.template < README.CONF\n" if($#ARGV!=0 || ! -f $ARGV[0]);

open(TEMPLATE,"<$ARGV[0]");

while(<TEMPLATE>)
  {
   last if(m%\#\# README.CONF \#\#%);
   print;
  }

$_=<STDIN>;
s/^ *//;
s/ *\n//;
$title=$_;

$hr=1;
$blank=0;
$intro=-1;
$appendix=0;
$first=1;
$dl=0;

while(<STDIN>)
  {
   chop;

   # Separator

   if ($_ eq "--------------------------------------------------------------------------------")
     {
      $hr=1;
      $intro=0;
     }

   # Underlines

   elsif (m/^ *[-=]+ *$/)
     {
      next;
     }

   # Section heading

   elsif ($hr==1 && m/^([-A-Za-z0-9]+)$/)
     {
      $section = $1;

      if ($section eq "WILDCARD")
        {
         $appendix=1;
        }

      $intro=1 if($intro==-1);

      print "\n.SH $section\n\n";

      $hr=0;
      $blank=0;
      $first=1;
     }

   # Item

   elsif (!$intro && !$appendix && m/^(\[?<URL-SPEC>\]? *)?(.?[-()a-z0-9]+)( *= *.+)?$/)
     {
      s/-/\\-/g;
      s/\./\\./g;

      print ".TP\n.B $_\n";

      $blank=0;
      $first=1;
     }

   # Item

   elsif (!$intro && !$appendix && (m/^(\[!\])?URL-SPECIFICATION/ || m/^\(/))
     {
      s/-/\\-/g;
      s/\./\\./g;

      print ".TP\n.B $_\n";

      $blank=0;
      $first=1;
     }

   # Blank

   elsif (m/^$/)
     {
      $blank=1 if(!$first);
      $dl=0;
     }

   # Text list

   elsif ($appendix && m%^([-a-zA-Z0-9():?*/.]+)   +(.+)%)
     {
      $thing=$1;
      $descrip=$2;

      s/-/\\-/g;
      s/\./\\./g;

      print ".TP\n.B $thing\n$descrip\n";

      $blank=0;
      $first=0;
      $dl=1;
     }

   # Text

   else
     {
      s/^ *//;

      s/-/\\-/g;
      s/\./\\./g;

      s%(wwwoffle\\.conf|CHANGES\\.CONF|URL\\-SPECIFICATION|URL\\-SPEC|WILDCARD) *%\n.I $1\n%g;
      s%^\n%%;

      print ".LP\n" if($blank);
      print "$_\n";

      $blank=0;
      $first=0;
     }
  }

print "\n";

while(<TEMPLATE>)
  {
   print;
  }

close(TEMPLATE);
