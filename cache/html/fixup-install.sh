#!/bin/sh
#
# A shell script that is run by the installation process and then deleted.
#
# Usage: fixup-install.sh LOCALHOST
#

localhost=$1

#

for file in */wwwoffle.pac ; do
    sed -e "s%LOCALHOST%$localhost%g" < $file > $file.tmp
    mv $file.tmp $file
done
