#!/bin/sh
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.6d.
#
# A Perl script to update the configuration file to version 2.6d standard (from version 2.5).
#
# Written by Andrew M. Bishop
#
# This file Copyright 2000,01 Andrew M. Bishop
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

exec perl -x $0 $1

exit 1

#!perl

$#ARGV==0 || die "Usage: $0 wwwoffle.conf\n";

$conf=$ARGV[0];

$myname = "wwwoffle-upgrade-config-2.5-2.6";
$version="2.6d";

# The new options that have been added (since version 2.5).

$urlspec="[^ \t\r\n:<!]+://[^ \t\r\n/=]+/?[^ \t\r\n=>]*";
$url="[^ \t\r\n:<!]*:?/?/?[^ \t\r\n/=]*/?[^ \t\r\n=>]*";

%new_StartUp=(
              "bind-ipv4 *=", "bind-ipv4 = 0.0.0.0",
              "#?bind-ipv6 *=", "#bind-ipv6 = ::"
              );

%new_Options=(
              "dns-timeout *=",            "dns-timeout = 60",
              "lock-files *=" ,            "lock-files = no",
              "reply-compressed-data *=" , "reply-compressed-data = no"
              );

%new_OnlineOptions=(
                    "try-without-password *=",    "try-without-password = yes",
                    "request-compressed-data *=", "request-compressed-data = yes"
                    );

%new_FetchOptions=(
                   "webbug-images *=", "webbug-images = yes"
                   );

%new_MIMETypes=(
                ".rpm *=" , ".rpm = application/octet-stream",
                ".deb *=" , ".deb = application/octet-stream"
                );

%new_ModifyHTML=(
                 "disable-meta-refresh *="      , "disable-meta-refresh = no",
                 "disable-meta-refresh-self *=" , "disable-meta-refresh-self = no",
                 "demoronise-ms-chars *="       , "demoronise-ms-chars = no",
                 "enable-modify-online *="      , "enable-modify-online = no",
                 "disable-applet *="            , "disable-applet = no",
                 "disable-style *="             , "disable-style = no",
                 "disable-dontget-links *="     , "disable-dontget-links = no",
                 "replace-dontget-images *="    , "replace-dontget-images = no",
                 "replacement-dontget-image *=" , "replacement-dontget-image = /local/dontget/replacement.gif",
                 "replace-webbug-images *="     , "replace-webbug-images = no",
                 "replacement-webbug-image *="  , "replacement-webbug-image = /local/dontget/replacement.gif"
                 );

%new_DontGet=(
              "location-error *=" , "location-error = no"
              );

%new_DontCompress=(
                   "mime-type *= *image/gif"         , "mime-type = image/gif",
                   "mime-type *= *image/jpeg"        , "mime-type = image/jpeg",
                   "mime-type *= *image/png"         , "mime-type = image/png",
                   "mime-type *= *image/tiff"        , "mime-type = image/tiff",
                   "mime-type *= *video/x-msvideo"   , "mime-type = video/x-msvideo",
                   "mime-type *= *video/quicktime"   , "mime-type = video/quicktime",
                   "mime-type *= *video/mpeg"        , "mime-type = video/mpeg",
                   "mime-type *= *audio/basic"       , "mime-type = audio/basic",
                   "mime-type *= *audio/x-wav"       , "mime-type = audio/x-wav",
                   "mime-type *= *application/x-dvi" , "mime-type = application/x-dvi",
                   "mime-type *= *application/pdf"   , "mime-type = application/pdf",
                   "mime-type *= *application/zip"   , "mime-type = application/zip",
                   "mime-type *= *application/x-ns-proxy-autoconfig" , "mime-type = application/x-ns-proxy-autoconfig",
                   "file-ext *= *.gz"   , "file-ext = .gz",
                   "file-ext *= *.bz"   , "file-ext = .bz",
                   "file-ext *= *.bz2"  , "file-ext = .bz2",
                   "file-ext *= *.Z"    , "file-ext = .Z",
                   "file-ext *= *.zip"  , "file-ext = .zip",
                   "file-ext *= *.tgz"  , "file-ext = .tgz",
                   "file-ext *= *.rpm"  , "file-ext = .rpm",
                   "file-ext *= *.deb"  , "file-ext = .deb",
                   "file-ext *= *.gif"  , "file-ext = .gif",
                   "file-ext *= *.GIF"  , "file-ext = .GIF",
                   "file-ext *= *.jpg"  , "file-ext = .jpg",
                   "file-ext *= *.JPG"  , "file-ext = .JPG",
                   "file-ext *= *.jpeg" , "file-ext = .jpeg",
                   "file-ext *= *.JPEG" , "file-ext = .JPEG",
                   "file-ext *= *.png"  , "file-ext = .png",
                   "file-ext *= *.PNG"  , "file-ext = .PNG"
                   );

%new_Purge=(
            "compress-age *="   , "compress-age = -1"
            );

%new_options=(
              "StartUp"      , \%new_StartUp,
              "Options"      , \%new_Options,
              "OnlineOptions", \%new_OnlineOptions,
              "FetchOptions" , \%new_FetchOptions,
              "MIMETypes"    , \%new_MIMETypes,
              "ModifyHTML"   , \%new_ModifyHTML,
              "DontGet"      , \%new_DontGet,
              "DontCompress" , \%new_DontCompress,
              "Purge"        , \%new_Purge
              );

# The options that have changed (since version 2.5).

%changed_DontGet=(
                  "^#? *!($urlspec) *= *($url)", " !\$1\n",
                  "^#? *($urlspec) *= *($url)" , " \$1\n <\$1> replacement = \$2\n",
                  "^#? *<($urlspec)> *replacement *= */local/images/trans-1x1.gif" , " <\$1> replacement = /local/dontget/replacement.png\n",
                  "^#? *replacement *= */local/images/trans-1x1.gif"               , " replacement = /local/dontget/replacement.png\n"
                  );

#%changed_FTPOptions=(
                      # See below for handling of auth-(hostname|username|password)
#                     );

%changed_DontRequestOffline=(
                             "^#? *!($urlspec) *\$", " <\$1> dont-request = no\n",
                             "^#? *($urlspec) *\$" , " <\$1> dont-request = yes\n"
                             );

%changed_DontGetRecursive=(
                           "^#? *!($urlspec) *\$", " <\$1> get-recursive = yes\n",
                           "^#? *($urlspec) *\$" , " <\$1> get-recursive = no\n"
                           );

%changed_DontIndex=(
                    "outgoing *= *!($urlspec)", " <\$1> list-outgoing = yes\n",
                    "outgoing *= *($urlspec)" , " <\$1> list-outgoing = no\n",
                    "latest *= *!($urlspec)"  , " <\$1> list-latest = yes\n",
                    "latest *= *($urlspec)"   , " <\$1> list-latest = no\n",
                    "monitor *= *!($urlspec)" , " <\$1> list-monitor = yes\n",
                    "monitor *= *($urlspec)"  , " <\$1> list-monitor = no\n",
                    "host *= *!($urlspec)"    , " <\$1> list-host = yes\n",
                    "host *= *($urlspec)"     , " <\$1> list-host = no\n",
                    "^#? *!($urlspec)"        , " <\$1> list-any = yes\n",
                    "^#? *($urlspec)"         , " <\$1> list-any = no\n"
                    );

%changed_Proxy=(
                # See below for handling of auth-(hostname|username|password)
                "default *= *([^ \t\r\n]*)"   , " proxy = \$1\n",
                "($urlspec) *= *([^ \t\r\n]*)", " <\$1> proxy = \$2\n"
                # See below that the proxy options are re-sorted
                );

%changed_Purge=(
                "default *= *(-?[0-9]+)"   , " age = \$1\n",
                "($urlspec) *= *(-?[0-9]+)", " <\$1> age = \$2\n"
                # See below that the age options are re-sorted
                );

%changed_options=(
                  "DontGet"           , \%changed_DontGet,
#                 "FTPOptions"        , \%changed_FTPOptions,
                  "DontRequestOffline", \%changed_DontRequestOffline,
                  "DontGetRecursive"  , \%changed_DontGetRecursive,
                  "DontIndex"         , \%changed_DontIndex,
                  "Proxy"             , \%changed_Proxy,
                  "Purge"             , \%changed_Purge
                  );

# The options that have been moved (since version 2.5).

%moved_StartUp=(
                "dir-perm *="     , "Options",
                "file-perm *="    , "Options",
                "run-online *="   , "Options",
                "run-offline *="  , "Options",
                "run-autodial *=" , "Options"
                );

%moved_Options=(
                "(<$urlspec>)? *pragma-no-cache *= *"      , "OfflineOptions",
                "(<$urlspec>)? *confirm-requests *= *"     , "OfflineOptions",
                "(<$urlspec>)? *request-changed *= *"      , "OnlineOptions",
                "(<$urlspec>)? *request-changed-once *= *" , "OnlineOptions",
                "(<$urlspec>)? *request-expired *= *"      , "OnlineOptions",
                "(<$urlspec>)? *request-no-cache *= *"     , "OnlineOptions",
                "(<$urlspec>)? *intr-download-keep *= *"   , "OnlineOptions",
                "(<$urlspec>)? *intr-download-size *= *"   , "OnlineOptions",
                "(<$urlspec>)? *intr-download-percent *= *", "OnlineOptions",
                "(<$urlspec>)? *timeout-download-keep *= *", "OnlineOptions",
                " *index-latest-days *= *"                 , "IndexOptions",
                " *no-lasttime-index *= *"                 , "IndexOptions"
                );

%moved_DontRequestOffline=(
                           ".", "OfflineOptions"
                           );

%moved_DontGetRecursive=(
                         ".", "DontGet"
                         );

%moved_DontIndex=(
                  ".", "IndexOptions"
                  );

%moved_options=(
                "StartUp",            \%moved_StartUp,
                "Options",            \%moved_Options,
                "DontRequestOffline", \%moved_DontRequestOffline,
                "DontGetRecursive",   \%moved_DontGetRecursive,
                "DontIndex",          \%moved_DontIndex
                );

# The options that have been deleted (since version 2.5).

@deleted_IndexOptions=(
                  " *index-latest-days *= *" ,
                  );

%deleted_options=(
                  "IndexOptions", \@deleted_IndexOptions
                  );

# The sections in the configuration file.

@old_sections=(
               "StartUp"            ,
               "Options"            ,
               "FetchOptions"       ,
               "ModifyHTML"         ,
               "LocalHost"          ,
               "LocalNet"           ,
               "AllowedConnectHosts",
               "AllowedConnectUsers",
               "DontCache"          ,
               "DontGet"            ,
               "DontGetRecursive"   ,
               "DontRequestOffline" ,
               "CensorHeader"       ,
               "FTPOptions"         ,
               "MIMETypes"          ,
               "Proxy"              ,
               "DontIndex"          ,
               "Alias"              ,
               "Purge"
               );

@new_sections=(
               "StartUp"            ,
               "Options"            ,
               "OnlineOptions"      ,
               "OfflineOptions"     ,
               "FetchOptions"       ,
               "IndexOptions"       ,
               "ModifyHTML"         ,
               "LocalHost"          ,
               "LocalNet"           ,
               "AllowedConnectHosts",
               "AllowedConnectUsers",
               "DontCache"          ,
               "DontGet"            ,
               "DontCompress"       ,
               "CensorHeader"       ,
               "FTPOptions"         ,
               "MIMETypes"          ,
               "Proxy"              ,
               "Alias"              ,
               "Purge"
               );

# The existing options in the old configuration file.

%options=(
          "StartUp"            , \@StartUp,
          "Options"            , \@Options,
          "OnlineOptions"      , \@OnlineOptions,
          "OfflineOptions"     , \@OfflineOptions,
          "FetchOptions"       , \@FetchOptions,
          "IndexOptions"       , \@IndexOptions,
          "ModifyHTML"         , \@ModifyHTML,
          "LocalHost"          , \@LocalHost,
          "LocalNet"           , \@LocalNet,
          "AllowedConnectHosts", \@AllowedConnectHosts,
          "AllowedConnectUsers", \@AllowedConnectUsers,
          "DontCache"          , \@DontCache,
          "DontGet"            , \@DontGet,
          "DontGetRecursive"   , \@DontGetRecursive,
          "DontRequestOffline" , \@DontRequestOffline,
          "CensorHeader"       , \@CensorHeader,
          "FTPOptions"         , \@FTPOptions,
          "MIMETypes"          , \@MIMETypes,
          "Proxy"              , \@Proxy,
          "DontIndex"          , \@DontIndex,
          "Alias"              , \@Alias,
          "Purge"              , \@Purge
          );

%include_files=();

%comments=();

# Read in the options from the current configuration file.

$lineno=0;

if(open(CURR,"<$conf"))
  {
   $section='';

   while(<CURR>)
       {
        $lineno++;
        if(m/^\# WWWOFFLE - World Wide Web Offline Explorer - (Version 2\.[0-47-9][a-z]?)/)
            {
             die "\nExisting configuration file is not for version 2.5 or 2.6.\n".
                 "(The header line says that it is '$1')\n".
                 "Try running upgrade-config-2.0-2.5.pl first.\n\n";
            }
        next if(/^[ \t]*\#/);
        next if(/^[ \t]*$/);

        if($section && /^[ \t]*\{/)
            {
             while(<CURR>)
                 {
                  $lineno++;
                  last if(/^[ \t]*\}/);
		  s,User-Agent\s*=\s*WWWOFFLE/2.[0-5].*,User-Agent = WWWOFFLE/2.6,;

                  push(@{$options{$section}},$_);
                 }

             $section='';
             next;
            }

        if($section && /^[ \t]*\[/)
            {
             while(<CURR>)
                 {
                  $lineno++;
                  s/\r*\n//;
                  s/\t/    /;
                  $incconf=$1,last if(m/^[ \t]*([^ \t\#]+)[ \t]*$/);
                 }

             @conf=split('/',$conf);
             pop(@conf);
             $incconf=join('/',(@conf,$incconf));

             $include_files{$section}=$incconf;

             open(INC,"<$incconf") || die "\nCannot open included config file '$incconf' to read\n\n";

             while(<INC>)
                 {
                  push(@{$options{$section}},$_);
                 }

             close(INC);

             rename "$incconf","$incconf.old" || die "\nCannot rename included config file '$incconf' to '$incconf.old'\n\n";

             while(<CURR>)
                 {
                  $lineno++;
                  last if(/^[ \t]*\]/);
                 }

             $section='';
             next;
            }

        if(!$section && /^[ \t]*([a-zA-Z]+)[ \t\r\n]*$/)
            {
             $section=$1;
             next;
            }

        die "\nParse Error line $lineno\n\n";
       }

   close(CURR);

   rename "$conf","$conf.old" || die "\nCannot rename config file '$conf' to '$conf.old'\n\n";
  }
else
  {
   die "\nCannot open config file '$conf' to read.\n\n";
  }

# Read in the new comments from the newly installed configuration file.
# First determine what to use for the new configuration file...
my $configsource;
if (-s '/usr/share/wwwoffle/default/wwwoffle.conf.gz') {
    $configsource = 'gzip -dc /usr/share/wwwoffle/default/wwwoffle.conf.gz |';
}
elsif (-s '/usr/share/wwwoffle/default/wwwoffle.conf') {
    $configsource = '/usr/share/wwwoffle/default/wwwoffle.conf';
}
else {
    $configsource = "$conf.install";
}
if(open(INST, $configsource))
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
             $comments{$section}=$comment;
             $comment='';
            }
       }

   close(INST);

   $comments{"TAIL"}=$comment;
  }
else
  {
   print "Cannot open 'wwwoffle.conf.install' or '$conf.install' - no new format comments\n";
  }

# Add the new options to the configuration file.

foreach $section (keys(%new_options))
  {
   $first=1;

   foreach $regexp_option (keys(%{$new_options{$section}}))
       {
        $match=0;

        foreach $line (@{$options{$section}})
            {
             if($line =~ m/$regexp_option/)
                 {$match=1;}
            }

        if($match==0)
            {
             if($first==1)
                 {
                  push(@{$options{$section}},"\n");
                  push(@{$options{$section}},"# Added for WWWOFFLE version $version by $myname\n");
                  push(@{$options{$section}},"\n");
                  $first=0;
                 }

             $new_option=$ {$new_options{$section}}{$regexp_option};
             print "$section\t- New option '$new_option'\n";
             push(@{$options{$section}}," $new_option\n");
            }
       }
  }

# Sort the purge ages to use "first found" rather than "longest match".

$resort_regexp="($urlspec|default) *= *[^ \t\r\n]+";

sub sortbyurllength
{
 $a =~ m/$resort_regexp/;
 $urla=$1;
 $urla=~ y/*//;
 $urla="" if($urla eq "default");

 $b =~ m/$resort_regexp/;
 $urlb=$1;
 $urlb=~ y/*//;
 $urlb="" if($urlb eq "default");

 length $urlb <=> length $urla;
}

foreach $section ("Proxy","Purge")
  {
   @sort_options=();
   $age_line = '';

   foreach $line (@{$options{$section}})
       {
        if($line =~ m/$resort_regexp/)
            {
             push(@sort_options,$line);
             $old_line=$line; $old_line =~ s/ *\r*\n//g; $old_line =~ s/^ *//;
             $line="";
             print "$section\t- Resorted the option '$old_line'\n";
            }
        elsif ($line =~ m/^[^#]*\s+age\s*=/)
            {
             $age_line = $line;
             $line='';
            }
       }

   if($#sort_options>=0)
       {
        push(@{$options{$section}},"\n");
        push(@{$options{$section}},"# Options re-sorted for WWWOFFLE version $version by $myname\n");
        push(@{$options{$section}},"# Now the \"first match\" is used, previously it was \"longest match\".\n");
        push(@{$options{$section}},"\n");

        foreach $line (sort sortbyurllength @sort_options)
            {
             push(@{$options{$section}},$line);
            }
       }
   if ($age_line)
       {
        push(@{$options{$section}},$age_line);
       }
  }

# Modify the options in the configuration file.

foreach $section (keys(%changed_options))
  {
   foreach $regexp_option (keys(%{$changed_options{$section}}))
       {
        foreach $line (@{$options{$section}})
            {
             if($line =~ m/$regexp_option/)
                 {
                  $old_line=$line; $old_line =~ s/ *\r*\n//g; $old_line =~ s/^ *//;
                  $line =~ m/$regexp_option/;
                  $line=$ {$changed_options{$section}}{$regexp_option};
                  $one=$1;
                  $two=$2;
                  $three=$3;
                  $line =~ s/\$1/$one/g;
                  $line =~ s/\$2/$two/g;
                  $line =~ s/\$3/$three/g;
                  $line="#".$line if($old_line =~ /^\#/);
                  $new_line=$line; $new_line =~ s/ *\r*\n//g; $new_line =~ s/^ *//;
                  print "$section\t- Changed option '$old_line' -> '$new_line'\n";
                 }
            }
       }
  }

# Modify the auth-(hostname|username|password) options.

foreach $section ("FTPOptions","Proxy")
  {
   $authhostname="";

   foreach $line (@{$options{$section}})
       {
        if($line =~ m/auth-hostname *= *([^ \t\r\n]+)/)
            {
             $authhostname=$1;
             $old_line=$line; $old_line =~ s/ *\r*\n//g; $old_line =~ s/^ *//;
             $line="";
             print "$section\t- Removed option '$old_line'\n";
            }

        if($line =~ m/^ *auth-(username|password) *= *([^ \t\r\n]+)/)
            {
             $authoption="auth-$1";
             $authuserpass=$2;
             $old_line=$line; $old_line =~ s/ *\r*\n//g; $old_line =~ s/^ *//;
             $line=" <ftp://$authhostname/*> $authoption = $authuserpass\n" if($section eq "FTPOptions");
             $line=" <http://$authhostname/*> $authoption = $authuserpass\n" if($section eq "Proxy");
             $line="#".$line if($old_line =~ /^\#/);
             $new_line=$line; $new_line =~ s/ *\r*\n//g; $new_line =~ s/^ *//;
             print "$section\t- Changed option '$old_line' -> '$new_line'\n";
            }
       }
  }

# Move the options that have moved.

foreach $section (keys(%moved_options))
  {
   %first=();

   foreach $line (@{$options{$section}})
       {
        foreach $regexp_option (keys(%{$moved_options{$section}}))
            {
             if($line =~ m/$regexp_option/)
                 {
                  $new_section=$ {$moved_options{$section}}{$regexp_option};
                  $old_line=$line; $old_line =~ s/ *\r*\n//g; $old_line =~ s/^ *//;
                  print "$section\t- Moved option '$old_line' -> $new_section\n";

                  if(!defined $first{$new_section})
                      {
                       push(@{$options{$new_section}},"\n");
                       push(@{$options{$new_section}},"# Options moved from $section section for WWWOFFLE version $version by $myname\n");
                       push(@{$options{$new_section}},"\n");
                       $first{$new_section}=0;
                      }

                  push(@{$options{$new_section}},$line);
                  $line="";
                 }
            }
       }
  }

# Delete the options that have been deleted.

foreach $section (keys(%deleted_options))
  {
   %first=();

   foreach $line (@{$options{$section}})
       {
        foreach $regexp_option (@{$deleted_options{$section}})
            {
             if($line =~ m/$regexp_option/ && $line !~ m/^\#\#\#/)
                 {
                  $old_line=$line; $old_line =~ s/ *\r*\n//g; $old_line =~ s/^ *//;
                  print "$section\t- Deleted option '$old_line'\n";

                  if(!defined $first{$section})
                      {
                       push(@{$options{$section}},"\n");
                       push(@{$options{$section}},"# Options deleted for WWWOFFLE version $version by $myname\n");
                       push(@{$options{$section}},"\n");
                       $first{$new_section}=0;
                      }

                  push(@{$options{$section}},"### $line");
                  $line="";
                 }
            }
       }
  }

# Write the new configuration file.

open(CONF,">$conf") || die "\nCannot open new config file '$conf' to write\n\n";

foreach $section (@new_sections)
  {
   print CONF $comments{$section};

   print CONF "$section\n";

   if($include_files{$section})
       {
        print CONF "[\n";

        $incconf=$include_files{$section};
        @incconf=split('/',$incconf);

        print CONF pop(@incconf)."\n";

        open(INC,">$incconf") || die "\nCannot open included config file '$incconf' to write\n\n";

        print INC @{$options{$section}};

        close(INC);

        print CONF "]\n";
       }
   else
       {
        print CONF "{\n";

        print CONF @{$options{$section}};

        print CONF "}\n";
       }
  }

print CONF $comments{"TAIL"};

close(CONF);
