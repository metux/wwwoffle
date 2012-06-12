#!/bin/sh
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.5b.
#
# A Perl script to update the configuration file to version 2.5 standard.
#
# Written by Andrew M. Bishop
#
# This file Copyright 1998,99 Andrew M. Bishop
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

exec perl -x $0 $1

exit 1

#!perl

$#ARGV==0 || die "Usage: $0 wwwoffle.conf\n";

$conf=$ARGV[0];

#

%new_MIMETypes=(".js"     ,  "application/x-javascript",
                ".htm"    ,  "text/html",
                ".pac"    ,  "application/x-ns-proxy-autoconfig",
                ".png"    ,  "image/png",
                ".class"  ,  "application/java",
                ".wrl"    ,  "model/vrml",
                ".vr"     ,  "model/vrml",
                ".css"    ,  "text/css",
                ".xml"    ,  "application/xml",
                ".dtd"    ,  "application/xml");

#

if(open(INST,"<wwwoffle.conf.install") || open(INST,"<$conf.install"))
  {
   $section=$comment='';

   while(<INST>)
       {
        if(!$section && (/^[ \t]*[\#{]/ || /^[ \t\r\n]*$/))
             {
              $comment.=$_;
              next;
             }

       if(/^[ \t]*}/)
            {
             $section=$comment='';
             next;
            }

        if(!$section && /^[ \t]*([a-zA-Z]+)[ \t\r\n]*$/)
            {
             $section=$1;
             $comment{$section}=$comment;
             $comment='';
            }
       }

   close(INST);
  }
else
  {
   print "Cannot open wwwoffle.conf.install or $conf.install - no new format comments\n";
  }

$comment{"TAIL"}=$comment;

#

system "mv $conf $conf.old" || die "Cannot rename config file $conf to $conf.old\n";

open(OLD,"<$conf.old") || die "Cannot open old config file $conf.old to read\n";

open(NEW,">$conf") || die "Cannot open new config file $conf to write\n";

#

$add_info_refresh="no";
$fetch_images="no";
$fetch_frames="no";

#

$prevsection='';
$section='';
$includedfile=0;
$filenamepending=0;

$OLD=OLD;
$NEW=NEW;

while(<$OLD>)
  {
   $wildcards=1 if(m/^# WWWOFFLE - World Wide Web Offline Explorer - Version 2\.[45]/);

   next if(!$section && (/^[ \t]*[\#{[]/ || /^[ \t\r\n]*$/));

   if($filenamepending)
       {
        print $NEW $_;
        if(/^[ \t\r\n]*([^ \t\r\n#]+)/)
            {
             @conf=split('/',$conf);
             pop(@conf);
             $incconf=join('/',(@conf,$1));

             system "mv $incconf $incconf.old" || die "Cannot rename included config file $incconf to $incconf.old\n";

             open(INCOLD,"<$incconf.old") || die "Cannot open old included config file $incconf.old to read\n";

             open(INCNEW,">$incconf") || die "Cannot open new included config file $incconf to write\n";

             $OLD=INCOLD;
             $NEW=INCNEW;
             $filenamepending=0;
             $includedfile=1;
            }
        next;
       }

   if(/^[ \t]*{/)
       {
        print $NEW $_;
        next;
       }

   if($section && /^[ \t]*\[/)
       {
        print $NEW $_;
        $filenamepending=1;
        next;
       }

   if(!$section && /^[ \t]*([a-zA-Z]+)[ \t\r\n]*$/)
       {
        $section=$1;
        $line=$_;

        if($prevsection eq "Options" && $section ne "FetchOptions") # Added in version 2.2
            {
             print "New Section - FetchOptions\n";
             print $NEW $comment{"FetchOptions"};
             print $NEW "FetchOptions\n";
             print $NEW "{\n";
             print $NEW " images = $fetch_images\n";
             print $NEW " frames = $fetch_frames\n";
             print $NEW "}\n";
            }

        if(($prevsection eq "Options" && $section ne "FetchOptions") ||
           ($prevsection eq "FetchOptions" && $section ne "ModifyHTML")) # Added in version 2.4, Modified in version 2.5
            {
             print "New Section - ModifyHTML\n";
             print $NEW $comment{"ModifyHTML"};
             print $NEW "ModifyHTML\n";
             print $NEW "{\n";
             print $NEW " enable-modify-html = no\n";
             print $NEW "\n";
             print $NEW " add-cache-info = $add_info_refresh\n";
             print $NEW "\n";
             print $NEW "#anchor-cached-begin     = <font color=\"#00B000\">\n";
             print $NEW "#anchor-cached-end       = </font>\n";
             print $NEW "#anchor-requested-begin  = <font color=\"#B0B000\">\n";
             print $NEW "#anchor-requested-end    = </font>\n";
             print $NEW "#anchor-not-cached-begin = <font color=\"#B00000\">\n";
             print $NEW "#anchor-not-cached-end   = </font>\n";
             print $NEW "\n";
             print $NEW " disable-script          = no\n";
             print $NEW " disable-blink           = no\n";
             print $NEW "\n";
             print $NEW " disable-animated-gif    = no\n";
             print $NEW "}\n";
            }

        if($section eq "AllowedConnect") # Renamed in version 2.4
            {
             print "Renamed Section - AllowedConnectHosts\n";
             $section="AllowedConnectHosts";
            }

        if($prevsection eq "AllowedConnectHosts" && $section ne "AllowedConnectUsers") # Added in version 2.4
            {
             print "New Section - AllowedConnectUsers\n";
             print $NEW $comment{"AllowedConnectUsers"};
             print $NEW "AllowedConnectUsers\n";
             print $NEW "{\n";
             print $NEW "}\n";
            }

        if($prevsection eq "Proxy" && $section ne "DontIndex") # Added in version 2.3
            {
             print "New Section - DontIndex\n";
             print $NEW $comment{"DontIndex"};
             print $NEW "DontIndex\n";
             print $NEW "{\n";
             print $NEW "}\n";
            }

        if($section eq "Mirror") # Renamed in version 2.3
            {
             print "Renamed Section - Alias\n";
             $section="Alias";
            }

        if($prevsection eq "DontGetRecursive" && $section ne "DontRequestOffline") # Added in version 2.4
            {
             print "New Section - DontRequestOffline\n";
             print $NEW $comment{"DontRequestOffline"};
             print $NEW "DontRequestOffline\n";
             print $NEW "{\n";
             print $NEW " *://*\n" if($no_offline_requests);
             print $NEW "}\n";
            }

        print "Section - $section\n";
        print $NEW $comment{$section};
        print $NEW "$section\n";
        next;
       }

   if($section && !/^[ \t]*}/)
       {
        $line=$_;

        if($section eq "Options") # Changed in version 2.1 / 2.2 / 2.4 / 2.5
            {
             if($line =~ /request-expired[ \t]*=[ \t]*(yes|no|1|0|true|false)/) # Added in version 2.5
                 {
                  $gotrequestexpired=1;
                 }
             elsif($line =~ /request-no-cache[ \t]*=[ \t]*(yes|no|1|0|true|false)/) # Added in version 2.5
                 {
                  $gotrequestnocache=1;
                 }
             elsif($line =~ /intr-download-keep[ \t]*=[ \t]*(yes|no|1|0|true|false)/) # Added in version 2.5
                 {
                  $gotintrdownloadkeep=1;
                 }
             elsif($line =~ /connect-timeout[ \t]*=[ \t]*[0-9]+/) # Added in version 2.5
                 {
                  $gotconnecttimeout=1;
                 }
             elsif($line =~ /ssl-allow-port[ \t]*=[ \t]*[0-9]+/) # Added in version 2.4
                 {
                  $gotsslallowport=1;
                 }
             elsif($line =~ /request-changed[ \t]*=[ \t]*([-0-9])/) # Checked in version 2.4
                 {
                  $requestchanged=$1;
                 }
             elsif($line =~ /request-changed-once[ \t]*=[ \t]*/) # Added in version 2.4
                 {
                  $gotrequestchangedonce=1;
                 }

             if($line =~ /request-changed[ \t]*=[ \t]*(yes|no|1|0|true|false)/) # Changed in version 2.1
                 {
                  print "Changed Option - request-changed\n";
                  $requestchanged=-1;
                  $requestchanged=600 if($1 eq "yes" || $1 eq "1" || $1 eq "true");
                  print $NEW " request-changed = $requestchanged\n";
                 }
             elsif($line =~ /fetch-images[ \t]*=[ \t]*(yes|no|1|0|true|false)/) # Changed in version 2.2
                 {
                  print "Removed Option - fetch-images\n";
                  $fetch_images=$1;
                 }
             elsif($line =~ /fetch-frames[ \t]*=[ \t]*(yes|no|1|0|true|false)/) # Changed in version 2.2
                 {
                  print "Removed Option - fetch-frames\n";
                  $fetch_frames=$1;
                 }
             elsif($line =~ /offline-requests[ \t]*=[ \t]*(yes|no|1|0|true|false)/) # Changed in version 2.4
                 {
                  print "Removed Option - offline-requests\n";
                  $no_offline_requests=1 if($1 eq "no" || $1 eq "0" || $1 eq "false");
                 }
             elsif($line =~ /monitor-interval[ \t]*=[ \t]*/) # Removed in version 2.4
                 {
                  print "Removed Option - monitor-interval\n";
                 }
             elsif($line =~ /add-info-refresh[ \t]*=[ \t]*(yes|no|1|0|true|false)/) # Moved in version 2.4
                 {
                  print "Removed Option add-info-refresh\n";
                  $add_info_refresh=$1;
                 }
             else
                 {
                  print $NEW $line;
                 }
            }
        elsif($section eq "AllowedConnectHosts" || $section eq "LocalNet") # Changed in version 2.4
            {
             if($line =~ /^([ \t]*)([-a-zA-Z0-9.]+)[ \t\r\n]*$/)
                 {
                  $line=$1 . &wildcard_host($2) . "\n";
                 }
             print $NEW $line;
            }
        elsif($section eq "CensorHeader") # Changed in version 2.2
            {
             if($line =~ /^[ \t]*([-a-zA-Z0-9]+)[ \t\r\n]*$/)
                 {
                  $line=" $1 = \n";
                  print "Changed CensorHeader - header line '$1'\n";
                 }
             print $NEW $line;
            }
        elsif($section eq "MIMETypes") # New ones added in various versions
            {
             $gotmimetype{$1}=1 if($line =~ /^[ \t]*(\.[^ ]+)[ \t]*=/);
             print $NEW $line;
            }
        elsif($section eq "DontCache" || $section eq "DontGet" ||
              $section eq "DontGetRecursive" || $section eq "DontGetOffline") # Changed in Version 2.3 / 2.4
            {
             if($section eq "DontGet" && $line =~ /^[ \t]*\#?[ \t]*replacement[ \t]*=/)
                 {
                  $gotreplacement=1;
                 }
             elsif($line =~ /^[ \t]*([^ \t\#=]+)[ \t]*=[ \t]*([^ \t\r\n=]+)/)
                 {
                  $line=" ".&url_spec($1,$2)."\n";
                 }
             elsif($line =~ /^[ \t]*([^ \t\r\n\#=]+)/)
                 {
                  $line=" ".&url_spec($1,"")."\n";
                 }
             print $NEW $line;
            }
        elsif($section eq "Proxy") # Changed in version 2.3 / 2.4
            {
             if($line =~ /^[ \t]*([^ \t\#=]+)[ \t]*=[ \t]*([^ \t\r\n=]+)/ && $1 !~ /^auth-/)
                 {
                  $line=" ".&url_spec($1,"")." = $2\n";
                 }
             print $NEW $line;
            }
        elsif($section eq "Mirror") # Changed in version 2.3
            {
             if($line =~ /^[ \t]*([^ \t\#=]+)[ \t]*=[ \t]*([^ \t\r\n=]+)/)
                 {
                  $line1=&url_spec($1,"");
                  $line2=&url_spec($2,"");
                  $line=" $line1 = $line2\n";
                 }
             print $NEW $line;
            }
        elsif($section eq "Purge") # Changed in version 2.3 / 2.4
            {
             if($line =~ /^[ \t]*\#?[ \t]*use-url[ \t]*=/)
                 {
                  $gotuseurl=1;
                 }
             elsif($line =~ /^[ \t]*([^ \t\#=]+)[ \t]*=[ \t]*([^ \t\r\n=]+)/ && $1 !~ /^[ \t]*[a-z]+-[a-z]+/)
                 {
                  $line=" ".&url_spec($1,"")." = $2\n";
                 }
             print $NEW $line;
            }
        else
            {
             print $NEW $line;
            }
       }

   if(/^[ \t]*]/)
       {
        print $NEW $_;
        next;
       }

   if(/^[ \t]*}/ || ($includedfile && eof($OLD)))
       {
        $prevsection=$section;
        $section='';

        if($prevsection eq "MIMETypes") # Updated in several versions
            {
             foreach $fext (keys(%new_MIMETypes))
                 {
                  if(!$gotmimetype{$fext})
                      {
                       $mime=$new_MIMETypes{$fext};
                       print "New MIME Types - $mime\n";
                       print $NEW " $fext   = $mime\n";
                      }
                 }
            }

        if($prevsection eq "Purge" && !$gotuseurl) # Added in version 2.3
            {
             print "New Option - use-url\n";
             print $NEW "\n use-url = no\n";
            }

        if($prevsection eq "DontGet" && !$gotreplacement) # Added in version 2.3
            {
             print "New Option - replacement\n";
             print $NEW "\n# replacement = /local/images/trans-1x1.gif\n";
            }

        if($prevsection eq "Options" && !$gotintrdownloadkeep) # Added in version 2.5
            {
             print "New Option - intr-download-keep\n";
             print $NEW "\n intr-download-keep    = no\n";
             print "New Option - intr-download-size\n";
             print $NEW " intr-download-size    = 1\n";
             print "New Option - intr-download-percent\n";
             print $NEW " intr-download-percent = 80\n";
             print "New Option - timeout-download-keep\n";
             print $NEW "\n timeout-download-keep = no\n";
            }
        if($prevsection eq "Options" && !$gotconnecttimeout) # Added in version 2.5
            {
             print "New Option - connect-timeout\n";
             print $NEW "\n connect-timeout = 30\n";
            }
        if($prevsection eq "Options" && !$gotrequestexpired) # Added in version 2.5
            {
             print "New Option - request-expired\n";
             print $NEW "\n request-expired = no\n";
            }
        if($prevsection eq "Options" && !$gotrequestnocache) # Added in version 2.5
            {
             print "New Option - request-no-cache\n";
             print $NEW "\n request-no-cache = no\n";
            }
        if($prevsection eq "Options" && !$gotsslallowport) # Added in version 2.4
            {
             print "New Option - ssl-allow-port\n";
             print $NEW "\n ssl-allow-port = 443\n";
            }
        if($prevsection eq "Options" && !$gotrequestchangedonce) # Added in version 2.4
            {
             print "New Option - request-changed-once\n";
             print $NEW "\n request-changed-once = no\n" if($requestchanged < 0);
             print $NEW "\n request-changed-once = yes\n" if($requestchanged >= 0);
            }

        print $NEW $_ if(!$includedfile);

        if($includedfile)
            {
             close($OLD);
             close($NEW);
             $OLD=OLD;
             $NEW=NEW;
             $includedfile=0;
            }
        next;
       }
  }

print $NEW $comment{"TAIL"};

#

close($NEW);
close($OLD);

#
# Subroutine to convert HOST-SPECIFICATION and FILE-SPECIFICATION
# or URL-SPECIFICATION into URL-SPECIFICATION with wildcards.
#

sub url_spec
{
 ($protohost,$file)=@_;
 $url="";

 if($file eq "")
     {
      return("default") if($protohost eq "default");
      return("# $protohost") if($protohost !~ m%^([^:/]+)://([^:/]+)(|:[^/]*)(|/.*)$%);
      $proto=$1;
      $host=$2;
      $port=$3;
      $path=$4;
      $path.="*" if(!$wildcards && $path ne "" && $path !~ /\*/);
      $path="/" if($path eq "");
     }
 else
     {
      $proto="";

      $proto="*",$host=$1,$port=$3   if($protohost =~ m%^/?([^:/]+)(|:.*)$%);        # [/]hostname[:*]
      $proto=$1,$host=$2,$port=$3    if($protohost =~ m%^([^:/]+)/([^:/]+)(|:.*)$%); # protocol/hostname[:*]
      $proto=$1,$host="*",$port=$3   if($protohost =~ m%^([^:/]+)/(|:.*)$%);         # protocol/[:*]
      $proto="*",$host="*",$port=""  if($protohost eq "/:" || $protohost eq "default");
      $proto="*",$host="*",$port=":" if($protohost eq "/");

      return("# $host $file") if(!$proto);

      $path="$file*"  if($file =~ m%^/%);
      $path="/*$file" if($file =~ m%^\.%);
     }

 $host=&wildcard_host($host);

 $url="$proto://$host$port$path";

 return($url);
}

#
# Subroutine to convert a hostname into a host name with wildcards.
#

sub wildcard_host
{
 ($old)=@_;
 $new=$old;

 return($new) if($wildcards);

 $new="*$old" if($old =~ /^[a-z0-9-.]+$/);
 $new="$old*" if($old =~ /^[0-9.]+$/);
 $new="$old"  if($old =~ /^[0-9]+.[0-9]+.[0-9]+.[0-9]+$/);

 return($new);
}
