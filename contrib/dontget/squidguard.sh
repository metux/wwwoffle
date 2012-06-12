#!/bin/sh

# Configuration items

blacklists="./blacklists"

types="ads aggressive audio-video drugs gambling hacking porn violence warez"

lists="domains urls"

result="wwwoffle.DontGet.squidguard"

# Main program

echo "SquidGuard -> WWWOFFLE conversion"
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

# Check for the blacklists

if [ ! -d $blacklists ]; then
    echo "The blacklists directory '$blacklists' does not exist"
    exit 1
fi

# Convert the lists

PATH=${PATH}:.

for type in $types ; do
    if [ -d "$blacklists/$type" ]; then

        echo -n "Convert $type list? [Y/n] "
        read answer

        if [ "x$answer" = "x" -o "x$answer" = "xY" -o "x$answer" = "xy" ]; then

            for list in $lists; do
                if [ -f "$blacklists/$type/$list" ]; then

                    echo "Converting '$blacklists/$type/$list' ..."

                    squidguard.pl "$blacklists/$type/$list" >> $result

                    echo "Converting '$blacklists/$type/$list' ... done"

                fi
            done

        fi

    fi
done
