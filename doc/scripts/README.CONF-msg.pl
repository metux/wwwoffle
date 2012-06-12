#!/usr/bin/perl

#
# Copyright Andrew M. Bishop 1996.97,98,2001,03.
#
# $Header: /home/amb/wwwoffle/doc/scripts/RCS/README.CONF-msg.pl 1.5 2003/06/15 11:08:23 amb Exp $
#
# Usage: README.CONF-msg.pl < README.CONF > messages/README.CONF.txt
#

$_=<STDIN>;
s/^ *//;
s/ *\n//;
$title=$_;

print "TITLE $title\n";
print "HEAD\n";

$hr=1;
$blank=0;
$intro=-1;
$appendix=0;
$first=1;
$dl=0;

while(<STDIN>)
  {
   chop;

   s/&/&amp;/g;
   s/</&lt;/g;
   s/>/&gt;/g;
   s/ä/&auml;/g;
   s/ö/&ouml;/g;
   s/ü/&uuml;/g;
   s/Ä/&Auml;/g;
   s/Ö/&Ouml;/g;
   s/Ü/&Uuml;/g;
   s/ß/&szlig;/g;

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
         print "\nTAIL\n";
         $appendix=1;
        }

      $intro=1 if($intro==-1);

      if ($appendix || $intro)
        {
         print "<h2><a name=\"$section\">$section</a></h2> ";
        }
      else
        {
         print "\nSECTION $section\n";
        }

      $hr=0;
      $blank=0;
      $first=1;
     }

   # Item

   elsif (!$intro && !$appendix && m/^(\[?&lt;URL-SPEC&gt;\]? *)?(.?[-()a-z0-9]+)( *= *.+)?$/)
     {
      $item=$2;
      $item="" if($item =~ m%\(%);

      print "\nITEM $item\n";

      s/&amp;/&/g;
      s/&lt;/</g;
      s/&gt;/>/g;

      print "$_\n";

      $blank=0;
      $first=1;
     }

   # Item

   elsif (!$intro && !$appendix && (m/^(\[!\])?URL-SPECIFICATION/ || m/^\(/))
     {
      $item = "";

      print "\nITEM \n";

      s/&amp;/&/g;
      s/&lt;/</g;
      s/&gt;/>/g;

      print "$_\n";

      $blank=0;
      $first=1;
     }

   # Blank

   elsif (m/^$/)
     {
      print "</dl>" if($dl);

      $blank=1 if(!$first);
      $dl=0;
     }

   # Text list

   elsif ($appendix && m%^([-a-zA-Z0-9():?*/.]+)   +(.+)%)
     {
      $thing=$1;
      $descrip=$2;

      print "<dl><dt>$thing<dd>$descrip";

      $blank=0;
      $first=0;
      $dl=1;
     }

   # Text

   else
     {
      s/^ *//;

      s%(URL-SPECIFICATION|URL-SPEC)%<a href="/configuration/#URL-SPECIFICATION">$1</a>%g;
      s%(WILDCARD)%<a href="/configuration/#WILDCARD">$1</a>%g;

      print " " if(!$first);

      print "<p> " if($blank);
      print "$_";

      $blank=0;
      $first=0;
     }
  }

print "\n";
