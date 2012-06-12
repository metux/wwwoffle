#!/bin/sh -e

PATH=/bin:/sbin:/usr/bin:/usr/sbin

# If ppp option is not there, don't do anything here.
if ! grep -qsx ppp /etc/wwwoffle/wwwoffle.options; then
    exit 0
fi


if [ -e /var/run/ppp-quick ]; then
    # Don't do the rest in the background
    # (name chosen to not be picked up by runparts)
    sh /etc/ppp/ip-up.d/1wwwoffle.subpart_
else
    # Do the rest in the background
    nohup sh /etc/ppp/ip-up.d/1wwwoffle.subpart_ >/dev/null 2>&1 &
fi


exit 0
