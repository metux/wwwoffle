#!/bin/sh

# Configuration items

patterns="./patterns"

result="wwwoffle.DontGet.adzap"

# Main program

echo "AdZap -> WWWOFFLE conversion"
echo "============================"
echo ""

# Check for the output file

if [ -f $result ]; then
    echo -n "The output file '$result' exists, overwrite? [Y/n] "
    read answer

    if [ "x$answer" = "x" -o "x$answer" = "xY" -o "x$answer" = "xy" ]; then
        true;
    else
        exit 1
    fi
fi

# Create the output file

echo > $result

# Check for the patterns

if [ ! -f $patterns ]; then
    echo "The patterns file '$patterns' does not exist"
    exit 1
fi

# Convert the list

PATH=${PATH}:.

echo "Converting '$patterns' ..."

adzap.pl "$patterns" >> $result

echo "Converting '$patterns' ... done"
