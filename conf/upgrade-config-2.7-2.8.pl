#!/bin/sh
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.8e.
#
# A Perl script to update the configuration file to version 2.8 standard (from version 2.7).
#
# Written by Andrew M. Bishop
#
# This file Copyright 2000,01,02,03,04 Andrew M. Bishop
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

exec perl -x $0 $1

exit 1

#!perl

$#ARGV==0 || die "Usage: $0 wwwoffle.conf\n";

$conf=$ARGV[0];

$version="2.8";

$urlspec="[^ \t:<!]+://[^ \t/=]+/?[^ \t=>]*";
$urlspec1="([^ \t:<!]+)://([^ \t/=]+)(/?[^ \t=>]*)";

# The new options that have been added (since version 2.7).

%new_Options=(
              "reply-chunked-data *=" , "reply-chunked-data = yes"
             );

%new_OnlineOptions=(
                    "pragma-no-cache *="        , "pragma-no-cache = yes",
                    "cache-control-no-cache *=" , "cache-control-no-cache = yes",
                    "cache-control-max-age-0 *=", "cache-control-max-age-0 = yes",
                    "request-conditional *="    , "request-conditional = yes",
                    "validate-with-etag *="     , "validate-with-etag = yes",
                    "keep-cache-if-not-found *=", "keep-cache-if-not-found = no",
                    "request-chunked-data *="   , "request-chunked-data = yes"
                   );

%new_OfflineOptions=(
                     "cache-control-max-age-0 *=", "cache-control-max-age-0 = yes"
                    );

%new_FetchOptions=(
                   "icon-images *=" , "icon-images = no"
                  );

%new_CensorHeader=(
                   "force-user-agent *=" , "force-user-agent = no"
                  );

%new_ModifyHTML=(
                 "disable-marquee *="         , "disable-marquee = no",
                 "disable-meta-set-cookie *=" , "disable-meta-set-cookie = no",
                 "fix-mixed-cyrillic *="      , "fix-mixed-cyrillic = no"
                  );

%new_options=(
              "Options"        , \%new_Options,
              "OnlineOptions"  , \%new_OnlineOptions,
              "OfflineOptions" , \%new_OfflineOptions,
              "FetchOptions"   , \%new_FetchOptions,
              "CensorHeader"   , \%new_CensorHeader,
              "ModifyHTML"     , \%new_ModifyHTML
              );

# The options that have changed (since version 2.7).

%changed_CensorHeader=(
                       "^#? *(<$urlspec>) *User-Agent *= *WWWOFFLE/[0-9.]+[a-z]*(.*)", " \$1 User-Agent = WWWOFFLE/$version\$2",
                       "^#? *User-Agent *= *WWWOFFLE/[0-9.]+[a-z]*(.*)"              , " User-Agent = WWWOFFLE/$version\$1",
                       );

%changed_IndexOptions=(
                       "^#? *no-lasttime-index *= (yes|true|1)", " create-history-indexes = no",
                       "^#? *no-lasttime-index *= (no|false|0)", " create-history-indexes = yes"
                       );

%changed_options=(
                  "CensorHeader"   , \%changed_CensorHeader,
                  "IndexOptions"   , \%changed_IndexOptions
                  );

# The options that have been moved (since version 2.7).

%moved_options=(
                );

# The options that have been deleted (since version 2.7).

@deleted_ModifyHTML=(
                     "enable-modify-online *="
                    );

%deleted_options=(
                  "ModifyHTML" , deleted_ModifyHTML
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
        s/\r*\n/\n/;

        if(m/^\# WWWOFFLE - World Wide Web Offline Explorer - (Version 2\.[0-69][a-z]?)/)
            {
             die "\nExisting configuration file is not for version 2.7 or 2.8.\n".
                 "(The header line says that it is '$1')\n".
                 "Try running upgrade-config-2.0-2.5.pl\n".
                 "     and/or upgrade-config-2.5-2.6.pl\n".
                 "     and/or upgrade-config-2.6-2.7.pl\n".
                 "\n";
            }
        next if(/^[ \t]*\#/);
        next if(/^[ \t]*$/);

        if($section && /^[ \t]*\{/)
            {
             while(<CURR>)
                 {
                  $lineno++;
                  s/\r*\n/\n/;
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
                  s/\r*\n/\n/;
                  s/\t/    /;
                  $incconf=$1,last if(m/^[ \t]*([^ \t\n\#]+)[ \t]*$/);
                 }

             @conf=split('/',$conf);
             pop(@conf);
             $incconf=join('/',(@conf,$incconf));

             $includes{$section}=$incconf;

             open(INC,"<$incconf") || die "\nCannot open included config file '$incconf' to read\n\n";

             while(<INC>)
                 {
                  s/\r*\n/\n/;

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
                  s/\r*\n/\n/;
                  last if(/^[ \t]*\]/);
                 }

             $section='';
             next;
            }

        if(!$section && /^[ \t]*([a-zA-Z]+)[ \t]*$/)
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
        s/\r*\n/\n/;

        if(!$section && (/^[ \t]*[\#{]/ || /^[ \t]*$/))
             {
              $comment.=$_;
              next;
             }

       if(/^[ \t]*}/)
            {
             $section=$comment='';
             next;
            }

        if(!$section && /^[ \t]*([a-zA-Z]+)[ \t]*$/)
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
                  $old_line=$line; $old_line =~ s/[ \t]*\n//g; $old_line =~ s/^[ \t]*//;
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
                  $old_line=$line; $old_line =~ s/[ \t]*\n//g; $old_line =~ s/^[ \t]*//;
                  $line =~ m/$regexp_option/;
                  $line=$ {$changed_options{$section}}{$regexp_option};
                  $one=$1;
                  $two=$2;
                  $three=$3;
                  $line =~ s/\$1/$one/g;
                  $line =~ s/\$2/$two/g;
                  $line =~ s/\$3/$three/g;
                  $line="#".$line if($old_line =~ /^\#/);
                  $line.="\n";
                  $new_line=$line; $new_line =~ s/[ \t]*\n$//g; $new_line =~ s/^[ \t]*//;
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
                  $old_line=$line; $old_line =~ s/[ \t]*\n//g; $old_line =~ s/^[ \t]*//;
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
