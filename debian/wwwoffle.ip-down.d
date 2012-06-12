#!/bin/sh -e
## This script is run when the ppp link goes down.

PATH=/bin:/sbin:/usr/bin:/usr/sbin
CONFIG=/etc/wwwoffle/wwwoffle.conf

# If ppp option is not there, don't do anything here.
if grep -qsx ppp /etc/wwwoffle/wwwoffle.options; then
    :
else
    exit 0
fi

# If there is *no* default route, then bring wwwoffle offline,
# as pppd first removes the entire interface and hence the routes
# over that interface before running ip-down .

# Else, check for default route over this interface; if the default
# route has nothing to do with this interface, don't modify wwwoffle's
# status.

DEFROUTEIF=`/sbin/route -n | grep '^0\.0\.0\.0 ' | awk '{print $8}'`
if [ ! -z "$DEFROUTEIF" ] && [ "x$DEFROUTEIF" != "x$PPP_IFACE" ]; then
    # default route not this interf.
    exit 0
fi

# Check for ISDN or diald in autodial mode.
mode=offline
if [ "$PPP_IFACE" = ippp0 ] && [ -n "`isdnctrl dialmode ippp0 2>/dev/null | grep auto`" ]; then
    mode=autodial
elif [ -n "`ps cax |grep diald`" ]; then
    mode=autodial
fi
wwwoffle -c /etc/wwwoffle/wwwoffle.conf -$mode || true

# we index the cached paged from the last session and merge them
# with the big wwwoffle-htdig index. This should be fast.

if [ -x /usr/bin/htdig ]; then
    # check whether user wants htdig to run for wwwoffle
    if grep -w '^htdig' /etc/wwwoffle/wwwoffle.options >/dev/null; then
        SPOOLDIR=$(perl -ne 'if(/^\s*spool-dir\s*=\s*(\S+)/) {print $1;exit}' $CONFIG 2>/dev/null)
        if [ -z "$SPOOLDIR" ]; then
            SPOOLDIR=/var/cache/wwwoffle
        fi
        mkdir -p "$SPOOLDIR/temp" >/dev/null 2>&1
        cd "$SPOOLDIR/temp"
        nohup su proxy -s /bin/sh -c /usr/share/wwwoffle/search/htdig/wwwoffle-htdig-lasttime >/dev/null 2>&1 &
        sleep 1
        rm -f nohup.out
    fi
fi

exit 0
