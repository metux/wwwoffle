#!/bin/sh
#
# A shell script that is run by the installation process and then deleted.
#
# Usage: fixup-install.sh SPOOLDIR LOCALHOST
#

spooldir=$1
localhost=$2

#

searches="htdig mnogosearch namazu hyperestraier"

#

for search in $searches; do

    for file in $search/conf/* ; do
        sed -e "s%SPOOLDIR%$spooldir%g" -e "s%LOCALHOST%$localhost%g" < $file > $file.tmp
        mv $file.tmp $file
    done

    for file in $search/scripts/* ; do
        sed -e "s%SPOOLDIR%$spooldir%g" -e "s%LOCALHOST%$localhost%g" < $file > $file.tmp
        mv $file.tmp $file
        chmod 755 $file
    done

done;
