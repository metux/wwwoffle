#!/bin/sh

cd /var/cache/wwwoffle

find *time* -name U\* -exec sh -c "echo -n {} ; echo -n ' ' ; cat {} ; echo" \; |\
sed 's%[a-z0-9]*/U%D%g' |\
sort -k 2 |\
uniq -c -f 1 |\
sort
