#!/bin/sh

# load the vars defined in 
. /etc/wwwoffle/dialup_vars.sh

echo $NOW Disconnect $LINE >> $LOGFILE
rasdial $LINE /DISCONNECT
