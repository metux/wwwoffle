#!/bin/sh -e

### BEGIN INIT INFO
# Provides:          wwwoffle
# Required-Start:    $local_fs $remote_fs $syslog $named $network
# Required-Stop:     $local_fs $remote_fs $syslog $named $network
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: wwwoffle World Wide Web OFFline Explorer
### END INIT INFO

$DEBIAN_SCRIPT_DEBUG || set -v -x 

PATH=/sbin:/bin:/usr/sbin:/usr/bin
NAME=wwwoffled
PROGRAM=/usr/bin/wwwoffle
DAEMON=/usr/sbin/$NAME
CONFIG=/etc/wwwoffle/wwwoffle.conf

test -f $DAEMON || exit 0

if grep -qsx ppp /etc/wwwoffle/wwwoffle.options; then
    # a whole lot of logic to determine what mode wwwoffle should run in...
    mode=offline
    # ISDN and diald can be in autodial mode
    # find default route
    WWWOFFLEDEFROUTEIF=`netstat -rn | awk '/^0\.0\.0\.0 / {print $8}'`
    if [ `expr "$WWWOFFLEDEFROUTEIF" : i` -eq 1 ]; then # default route via ISDN
        if isdnctrl status $WWWOFFLEDEFROUTEIF >/dev/null 2>&1; then
            mode=online
        elif isdnctrl dialmode $WWWOFFLEDEFROUTEIF 2>/dev/null |
                            grep auto >/dev/null; then
            # ISDN interface is in autodial mode
            mode=autodial
        fi
    elif pgrep '^diald$' >/dev/null; then
        # diald is running, hence also autodial
        mode=autodial
    elif pgrep '^pp(tp|poe|poa)$' >/dev/null; then
        # pptp or pppoe is running (for ADSL), hence always online
        mode=online
    fi
else
    # wwwoffle not configured to go over dialup, so assume online
    mode=online
fi

case "$1" in
  start)
	echo -n "Starting HTTP cache proxy server: $NAME "
        # extract location of spool-dir from config file
        if [ -f $CONFIG ]; then
            SPOOLDIR=$(perl -ne 'if(/^\s*spool-dir\s*=\s*(\S+)/) {print $1;exit}' $CONFIG 2>/dev/null)
            if [ -z "$SPOOLDIR" ]; then
                echo "error:"
                echo "No spool-dir configured in the config file '$CONFIG', cannot proceed."
                exit 1
            fi
            if [ ! -d "$SPOOLDIR" ]; then
                echo "notice:"
                echo "The configured spooldir '$SPOOLDIR' doesn't exist, trying to proceed."
                install -d -D -o proxy -g proxy "$SPOOLDIR"
            fi
        else
            echo "error:"
            echo "The config file '$CONFIG' doesn't exist."
            exit 1
        fi
	# Check if the cache dir is intact. According to the Debian Policy 
	# the cache dir may be deleted and has to recreate itself automatically.
        cd "$SPOOLDIR"
	install -d -D -o proxy -g proxy		\
	    http		\
	    lastout		\
	    lasttime	        \
	    local		\
	    outgoing	        \
	    prevtime1	        \
	    finger		\
	    ftp		        \
	    search/htdig/db	\
	    search/htdig/db-lasttime \
	    search/htdig/tmp    \
	    search/mnogosearch/db \
	    search/namazu/db
        # mark the directory as a cache directory
        if [ ! -f CACHEDIR.TAG ]; then
            cat > CACHEDIR.TAG << 'EOF'
Signature: 8a477f597d28d172789f06886806bc55
# This file is a cache directory tag created by wwwoffle.
# For information about cache directory tags, see:
#       http://www.brynosaurus.com/cachedir/
EOF
        fi
        cd /

	msg=" in $SPOOLDIR/search"
	for i in htdig mnogosearch namazu; do
	    if [ ! -L "$SPOOLDIR/search/$i/conf" ]; then
		echo "restoring $i/conf    symlink$msg"; msg=''
		rm -f "$SPOOLDIR/search/$i/conf"
		ln -s /etc/wwwoffle/$i "$SPOOLDIR/search/$i/conf"
	    fi
	    if [ ! -L "$SPOOLDIR/search/$i/scripts" ]; then
		echo "restoring $i/scripts symlink$msg"; msg=''
		rm -f "$SPOOLDIR/search/$i/scripts"
		ln -s /usr/share/wwwoffle/search/$i "$SPOOLDIR/search/$i/scripts"
	    fi
	done
	if [ ! -L "$SPOOLDIR/html" ];  then
		if [ -e "$SPOOLDIR/html" ]; then
			mv -f "$SPOOLDIR/html" "$SPOOLDIR/html.x"
		fi
		ln -sf /etc/wwwoffle/html "$SPOOLDIR/html"
	fi
        if [ ! -L "$SPOOLDIR/monitor" ]; then
            if [ -d "$SPOOLDIR/monitor" ]; then
                (
                  echo "  moving $SPOOLDIR/monitor to /var/lib/wwwoffle/monitor"
                  cd "$SPOOLDIR/monitor"
		  mv * /var/lib/wwwoffle/monitor 2>/dev/null || true
                )
            fi
            rm -rf "$SPOOLDIR/monitor"
            echo "  creating $SPOOLDIR/monitor symlink"
            ln -sf ../../lib/wwwoffle/monitor "$SPOOLDIR/monitor"
        fi
	# fix namazu NMZ files; they're in /etc, but symlinked to /var/cache
	(
	  cd /etc/wwwoffle/namazu
	  for i in NMZ.body NMZ.foot NMZ.head NMZ.result.normal NMZ.result.short NMZ.tips; do
		if [ ! -s "$SPOOLDIR/search/namazu/db/$i" ]; then
			ln -s /etc/wwwoffle/namazu/$i "$SPOOLDIR/search/namazu/db/$i"
		fi
	  done
	)
        if [ -s /etc/wwwoffle/certificates/root/root-key.pem -a -s /etc/wwwoffle/certificates/root/root-cert.pem ]; then
            if ! /usr/sbin/wwwoffle-checkcert; then
                echo "Stand by while a new certificate is generated."
            fi
            TEMPFILE=`tempfile -p WWWOFFLE`
            set +e
            start-stop-daemon --start  --exec $DAEMON -- -c $CONFIG >$TEMPFILE 2>&1
            RC=$?
            # If listening to IPv6 socket, then binding to the IPv4 socket _will_ fail
            # Ignore those warning lines
            if grep 'Warning: Failed to bind IPv4 server socket to.*Address already in use' $TEMPFILE >/dev/null &&
                grep 'Warning: Cannot create HTTP IPv4 server socket .but the IPv6 one might accept IPv4 connections' $TEMPFILE >/dev/null; then
                    fgrep -v 'Important: WWWOFFLE Demon Version 
Important: Detached from terminal and changed pid to
Information: WWWOFFLE Read Configuration File
Information: Running with uid=
Warning: Cannot create HTTP IPv4 server socket
Warning: Cannot create WWWOFFLE IPv4 server socket' $TEMPFILE
            else
                fgrep -v 'Important: WWWOFFLE Demon Version 
Important: Detached from terminal and changed pid to
Information: WWWOFFLE Read Configuration File
Information: Running with uid=' $TEMPFILE
            fi
            rm -f $TEMPFILE
        else # certs dont exist
            if [ -s /etc/wwwoffle/certificates/root/root-cert.pem ]; then
                # start with a clean slate
                rm -f /etc/wwwoffle/certificates/root/root-cert.pem
            fi
            echo "notice:"
            echo "The WWWOFFLE root CA files don't exist. Please stand by while these are"
            echo "generated (this may take very long, depending on your system)."
            start-stop-daemon --start  --exec $DAEMON -- -c $CONFIG
            RC=$?
        fi
        set -e
        if [ $RC -eq 0 ]; then
	  if [ "$mode" = "offline" ]; then
            echo "(offline mode) done."
          else
	    $PROGRAM -c $CONFIG -$mode
	  fi
	else 
	  echo "...failed."
	fi
        if grep -qsx nocheckconf /etc/wwwoffle/wwwoffle.options; then
          if grep '^[^#]*://.*/$' $CONFIG; then
            printf '\a'
            cat << 'EOF'

Note: you have URL patterns in your wwwoffle.conf that end with a '/'.
This may not do as you expect: '*:/*.foo/' matches only the path '/',
while while '*:/*.foo' and '*:/*.foo/*' both match all URLs on the host.

(To prevent this message, add a line with exactly "nocheckconf" to
/etc/wwwoffle/wwwoffle.options .)

EOF
            sleep 6
          fi
        fi
	;;
  stop)
	echo -n "Stopping HTTP cache proxy server: $NAME"
	TEMPFILE=`tempfile -p WWWOFFLE`
	$PROGRAM -c $CONFIG -kill >$TEMPFILE 2>&1 || true
	if grep 'Fatal:' $TEMPFILE >/dev/null; then
	  if grep 'Warning: Failed to create and connect client socket.' $TEMPFILE >/dev/null; then
	    rm -f $TEMPFILE
	    echo "...can't connect to control socket."
	    printf "\tTrying to signal daemon process"
	    if start-stop-daemon --stop  --signal 15 --quiet --exec $DAEMON; then 
	      echo "...done."
	      exit 0
	    else 
	      echo "...failed, daemon was not running."
	      exit 0
	    fi
	  fi
	  echo "... failed:"
	  sed 's,^,\t,' $TEMPFILE
	  rm -f $TEMPFILE
	  exit 1
	fi
	rm -f $TEMPFILE
	for i in 1 2 3 4 5; do
	  if pgrep wwwoffled >/dev/null 2>&1; then
	    sleep 1
	    echo -n "."
	  else
	    echo " ok."
	    exit 0
	  fi
	done
	if start-stop-daemon --stop  --quiet --exec $DAEMON; then 
	  echo " ok."
	  exit 0
	fi
	echo " stopping failed!"
	exit 1
	;;
  force-restart|restart|magic-restart) # last one preserves the online/offline/autodial state
	# The wwwoffled maybe was already online.
        if pgrep '^wwwoffled$' >/dev/null; then
            if wwwoffle -status -c $CONFIG | grep -qs online; then
                mode=online
            elif wwwoffle -status -c $CONFIG | grep -qs offline; then
                mode=offline
            else
                mode=autodial   # lucky guess?
            fi
        fi
	$0 stop
	sleep 2
	$0 start
        # if wwwoffle wasn't in the default startup mode, change it
        if pgrep '^wwwoffled$' >/dev/null; then
            if wwwoffle -status -c $CONFIG | grep -qs online; then
                newmode=online
            elif wwwoffle -status -c $CONFIG | grep -qs offline; then
                newmode=offline
            else
                newmode=autodial   # lucky guess?
            fi
        else
            echo "failed!"
            exit 1
        fi
        if [ "$newmode" != "$mode" ]; then
            if [ "$1" = magic-restart ]; then
                $PROGRAM -c $CONFIG -$mode
            else
                echo "not restoring wwwoffle to the previous '$mode' state."
            fi
	fi
	;;
  reload|force-reload)
	echo -n "Reloading $NAME configuration files..."
    	if start-stop-daemon --stop  --signal 1 --quiet --exec $DAEMON; then 
      	  echo "done."
    	else 
      	  echo "failed."
    	fi
    	;;
  *)
	echo "Usage: /etc/init.d/$NAME {start|stop|reload|restart|force-restart|force-reload}"
	exit 1
	;;
esac

exit 0
