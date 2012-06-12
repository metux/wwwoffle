#!/usr/bin/perl

#
# Copyright Andrew M. Bishop 1996.97,98,2001.
#
# $Header: /home/amb/wwwoffle/doc/scripts/RCS/README.CONF-conf.pl 1.3 2003/06/15 11:08:22 amb Exp $
#
# Usage: README.CONF-man.pl wwwoffle.conf.template < README.CONF > wwwoffle.conf
#

die "Usage: $0 wwwoffle.conf.template < README.CONF\n" if($#ARGV!=0 || ! -f $ARGV[0]);

open(TEMPLATE,"<$ARGV[0]");

while(<TEMPLATE>)
  {
   last if(m%^$%);
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
      print "# $_\n" if(m/-+/);
      next;
     }

   # Section heading

   elsif ($hr==1 && m/^([-A-Za-z0-9]+)$/)
     {
      $section = $1;

      if (!$intro)
        {
         print "\n$prevsection\n" if($prevsection);

         while (<TEMPLATE>)
           {
            chop;
            $inbracket=1 if(m%^{%);
            $inbracket=0 if(m%^}%);
            next if(!$inbracket && m%^$%);
            last if(m%^($section)$%);
            print "$_\n";
           }
         $prevsection=$section;
        }

      if ($section eq "WILDCARD")
        {
         $appendix=1;
         $prevsection="";
        }

      $intro=1 if($intro==-1);

      print "\n\n#\n# $section\n";

      $hr=0;
      $blank=0;
      $first=1;
     }

   # Item

   elsif (!$intro && !$appendix && m/^(\[?<URL-SPEC>\]? *)?(.?[-()a-z0-9]+)( *= *.+)?$/)
     {
      print "# $_\n";

      $blank=0;
      $first=1;
     }

   # Item

   elsif (!$intro && !$appendix && (m/^(\[!\])?URL-SPECIFICATION/ || m/^\(/))
     {
      print "# $_\n";

      $blank=0;
      $first=1;
     }

   # Blank

   elsif (m/^$/)
     {
      print "#\n" if(!$blank);
      $blank=1 if(!$first);
      $dl=0;
     }

   # Text list

   elsif ($appendix && m%^([-a-zA-Z0-9():?*/.]+)   +(.+)%)
     {
      $thing=$1;
      $descrip=$2;

      print "# $thing\n#         $descrip\n";

      $blank=0;
      $first=0;
      $dl=1;
     }

   # Text

   else
     {
      if ($dl)
        {
         s/^ *//;
         print "#         $_\n";
        }
      else
        {
         print "# $_\n";
        }

      $blank=0;
      $first=0;
     }
  }

print "#\n";

close(TEMPLATE);
