#!/bin/sh

if [ "$1" = "" ]; then
   echo Specify a filename
   exit
fi

cd /var/spool/wwwoffle

echo *time*/$1 |\
xargs -n 1 awk '/^\r$/ {finished=1} {if(!finished) print}' |\
sort |\
uniq -c
