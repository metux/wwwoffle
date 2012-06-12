#!/bin/sh
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.7g.
#
# A Perl script to update the configuration file to version 2.7[abcdefg] standard (from version 2.6).
#
# Written by Andrew M. Bishop
#
# This file Copyright 2000,01,02 Andrew M. Bishop
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

exec perl -x $0 $1

exit 1

#!perl

$#ARGV==0 || die "Usage: $0 wwwoffle.conf\n";

$conf=$ARGV[0];

$version="2.7";

$urlspec="[^ \t\r\n:<!]+://[^ \t\r\n/=]+/?[^ \t\r\n=>]*";
$urlspec1="([^ \t\r\n:<!]+)://([^ \t\r\n/=]+)(/?[^ \t\r\n=>]*)";

# The new options that have been added (since version 2.6).

%new_Options=(
              "exec-cgi *=" , "#exec-cgi = /local/cgi-bin/*\n #exec-cgi = /local/*.cgi"
             );

%new_FetchOptions=(
                   "only-same-host-images *=" , "only-same-host-images = no"
                  );

%new_OfflineOptions=(
                     "cache-control-no-cache *=" , "cache-control-no-cache = yes"
                    );

%new_IndexOptions=(
                   "create-history-indexes *=", "#create-history-indexes = yes\n"
                  );

%new_MIMETypes=(
                ".exe *=" , ".exe = application/octet-stream"
                );

%new_ModifyHTML=(
                "disable-dontget-iframes *=" , "disable-dontget-iframes = no",
                "disable-flash *="           , "disable-flash = no"
                );

%new_LocalHost=(
                "ip6-localhost"    , "ip6-localhost",
                "::1"              , "::1",
                "::ffff:127.0.0.1" , "::ffff:127.0.0.1"
               );

%new_options=(
              "Options"        , \%new_Options,
              "FetchOptions"   , \%new_FetchOptions,
              "OfflineOptions" , \%new_OfflineOptions,
              "IndexOptions"   , \%new_IndexOptions,
              "MIMETypes"      , \%new_MIMETypes,
              "ModifyHTML"     , \%new_ModifyHTML,
              "LocalHost"      , \%new_LocalHost
              );

# The options that have changed (since version 2.6).

%changed_CensorHeader=(
                       "^#? *(<$urlspec>) *User-Agent *= *WWWOFFLE/[0-9.]+[a-z]*(.*)", " \$1 User-Agent = WWWOFFLE/$version\$2\n",
                       "^#? *User-Agent *= *WWWOFFLE/[0-9.]+[a-z]*(.*)"              , " User-Agent = WWWOFFLE/$version\$1\n",
                       "^#? *(<$urlspec>) *User-Agent *= *WWWOFFLE/[0-9. ]+[a-z]*\$" , " \$1 User-Agent = WWWOFFLE/$version\n",
                       "^#? *User-Agent *= *WWWOFFLE/[0-9. ]+[a-z]*\$"               , " User-Agent = WWWOFFLE/$version\n"
                       );

%changed_IndexOptions=(
                       "^#? *no-lasttime-index *= (yes|true|1)", " create-history-indexes = no\n",
                       "^#? *no-lasttime-index *= (no|false|0)", " create-history-indexes = yes\n"
                       );

%changed_Purge=(
                "^#? *max-size *= 0", " max-size = -1\n",
                "^#? *min-free *= 0", " min-free = -1\n"
               );

%changed_options=(
                  "CensorHeader"   , \%changed_CensorHeader,
                  "IndexOptions"   , \%changed_IndexOptions,
                  "Purge"          , \%changed_Purge
                  );

# The options that have been moved (since version 2.6).

%moved_options=(
                );

# The options that have been deleted (since version 2.6).

%deleted_options=(
                  );

# The sections in the configuration file.

@sections=(
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

# The options in the configuration file.

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
          "DontCompress"       , \@DontCompress,
          "CensorHeader"       , \@CensorHeader,
          "FTPOptions"         , \@FTPOptions,
          "MIMETypes"          , \@MIMETypes,
          "Proxy"              , \@Proxy,
          "Alias"              , \@Alias,
          "Purge"              , \@Purge
          );

%includes=();

%comments=();

# Read in the options from the current configuration file.

$lineno=0;

if(open(CURR,"<$conf"))
  {
   $section='';

   while(<CURR>)
       {
        $lineno++;
        if(m/^\# WWWOFFLE - World Wide Web Offline Explorer - (Version 2\.[0-58-9][a-z]?)/)
            {
             die "\nExisting configuration file is not for version 2.6 or 2.7.\n".
                 "(The header line says that it is '$1')\n".
                 "Try running upgrade-config-2.0-2.5.pl\n".
                 "     and/or upgrade-config-2.5-2.6.pl\n".
                 "\n";
            }
        next if(/^[ \t]*\#/);
        next if(/^[ \t]*$/);

        if($section && /^[ \t]*\{/)
            {
             while(<CURR>)
                 {
                  $lineno++;
                  last if(/^[ \t]*\}/);

                  push(@{$options{$section}},$_);

                  # Warn about URL-SPECIFICATION changes: http://www.foo/ is not http://www.foo/*

                  if((($section eq "DontGet" || $section eq "DontCache") && m%^#? *$urlspec1%) ||
                      m%^#? *<$urlspec1>%)
                    {
                      print "$section\tThe URL-SPEC $1://$2$3 does not match all pages on $2 did you mean $1://$2/*\n" if($3 eq "/");
                    }
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

             $includes{$section}=$incconf;

             open(INC,"<$incconf") || die "\nCannot open included config file '$incconf' to read\n\n";

             while(<INC>)
                 {
                  push(@{$options{$section}},$_);

                  # Warn about URL-SPECIFICATION changes: http://www.foo/ is not http://www.foo/*

                  if((($section eq "DontGet" || $section eq "DontCache") && m%^#? *$urlspec1%) ||
                      m%^#? *<$urlspec1>%)
                    {
                      print "$section\tThe URL-SPEC $1://$2$3 does not match all pages on $2 did you mean $1://$2/*\n" if($3 eq "/");
                    }
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
                  push(@{$options{$section}},"# Added for WWWOFFLE version $version by upgrade-config.pl\n");
                  push(@{$options{$section}},"\n");
                  $first=0;
                 }

             $new_option=$ {$new_options{$section}}{$regexp_option};
             print "$section\t- New option '$new_option'\n";
             push(@{$options{$section}}," $new_option\n");
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
                       push(@{$options{$new_section}},"# Options moved from $section section for WWWOFFLE version $version by upgrade-config.pl\n");
                       push(@{$options{$new_section}},"\n");
                       $first{$new_section}=0;
                      }

                  push(@{$options{$new_section}},$line);
                  $line="";
                 }
            }
       }
  }

# Change the options in the configuration file.

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
                  print "$section\t- Changed option '$old_line' -> '$new_line'\n" if($old_line ne $new_line);
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
                       push(@{$options{$section}},"# Options deleted for WWWOFFLE version $version by upgrade-config.pl\n");
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

foreach $section (@sections)
  {
   print CONF $comments{$section};

   print CONF "$section\n";

   if($includes{$section})
       {
        print CONF "[\n";

        $incconf=$includes{$section};
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
