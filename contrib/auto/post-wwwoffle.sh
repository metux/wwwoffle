#!/bin/sh
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.4c.
#
# Post WWWOFFLE script for processing the files fetched.
#
# Written by Andrew M. Bishop
#
# This file Copyright 1999 Andrew M. Bishop
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

# Set the spool directory

wwwoffle_spool=/var/cache/wwwoffle

cd $wwwoffle_spool

# Print out a header message.

echo ""
echo 'Post-WWWOFFLE Script'
echo '===================='

# Find the size of the files downloaded

echo ""
wwwoffle-ls lasttime | awk '{size+=$2} END{printf("Downloaded: %d Bytes in %d URLs\n",size,NR)}'

# Check them for interesting ones

files=`wwwoffle-ls lasttime | awk '{print $6}'`

for file in $files; do

    case $file in

    #
    # In here you can put a wildcard match for a URL followed by some shell script.
    #
    # The example below shows the Dilbert cartoon.
    # To get this you should monitor http://www.unitedmedia.com/comics/dilbert/
    #

    http://www.unitedmedia.com/comics/dilbert/archive/images/dilbert*.gif)

        # The United Media Comic Dilbert Cartoon
        # http://www.unitedmedia.com/comics/dilbert/

        echo ""
        echo "Dilbert: $file"

        # Display in an xv window
        wwwoffle -o $file | xv - &
        ;;

    esac

done
