#!/bin/bash

# a collection of scripts for those who want to use wwwoffle on windows. 
# automatic dialup depending on hour or day. 
# these scripts will work on XP, 2k I think, and probably on WinNT 3.x 
# but not on Win9x
# by JPT, j.p.t.@gmx.net in 2005

# example for a dialup provider in germany. 

# Arcor Day
# 01920782
# 0,65 ct/Min. 09:00 - 18:00 Weekday
# 0,60 ct/Min. 09:00 - 18:00 Weekend
# 1,99 ct/Min. 18:00 - 09:00 

# Arcor Night 
# 01920783
# 1,99 ct/Min. 09:00 - 18:00 
# 0,56 ct/Min. 18:00 - 09:00 

# before you may use this script, add a dial up config named 'auto'. 
# it does not need to have a phone number or login/password. 
# be sure the other settings are correct
LINE=auto

# if you defined login and password in the config above, you may leave the  
# vars blank. If their value depends on the phone number dialed (ie. the 
# current hour), define them in online.sh
LOGIN=arcor
PASSWORD=internet

# we log to: 
LOGFILE=/var/log/rasdial.log
# you may switch this off by adding a '#' to the 'echo' lines below
# logentry date
NOW=$(/usr/bin/date '+%Y-%m-%d %H:%M:%S')


# all files, ie
# $LOGFILE, offline.sh, online.sh, dialup_vars.sh
# need to be owner:group
# SYSTEM:root 
# need to have the following cygwin rights: 
# 660 (-rw-rw----) $LOGFILE
# 760 (-rwxrw----) *.sh
# you will be able to modify them when in the admin group (root)
# Do not forget to set the rights of the parent folders!

# I put the files into /etc/wwwoffle, but you may put them everywhere you 
# want. And I run wwwoffle as user SYSTEM, which is the default for 
# cygrunsrv services. You may add a new user like you'd do on linux, 
# but beware of the work and the troubles! ;-(

