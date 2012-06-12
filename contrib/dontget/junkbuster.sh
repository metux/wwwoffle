#!/bin/sh

# Configuration items

blockfile="./blockfile"

result="wwwoffle.DontGet.junkbuster"

# Main program

echo "JunkBuster -> WWWOFFLE conversion"
echo "================================="
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

# Check for the blockfile

if [ ! -f $blockfile ]; then
    echo "The blockfile '$blockfile' does not exist"
    exit 1
fi

# Convert the list

PATH=${PATH}:.

echo "Converting '$blockfile' ..."

junkbuster.pl "$blockfile" >> $result

echo "Converting '$blockfile' ... done"
