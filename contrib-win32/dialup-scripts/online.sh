#!/bin/bash

# load the vars defined in 
. /etc/wwwoffle/dialup_vars.sh

# get the current hour
HOUR=$(/usr/bin/date +%k) 

# you may use 'date +%u' for the day of week 
#  %u   day of week (1..7);  1 represents Monday

if [ $HOUR -ge 9 -a $HOUR -lt 18 ]; then
	DIAL=01920782
else  	
	DIAL=01920783
fi;

echo $NOW Dial $LINE \<login\> \<passwd\> $DIAL >> $LOGFILE
rasdial $LINE $LOGIN $PASSWORD /PHONE:$DIAL

