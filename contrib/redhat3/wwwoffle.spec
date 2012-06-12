Name: wwwoffle

%define init_dir      %{_initrddir}
%define daemon        wwwoffled
%define dontget       wwwoffle-dontget.conf
%define init          %{init_dir}/%{daemon}
%define spool_dir     /var/cache/%{name}
%define inst_dir      %{_prefix}
%define conf_dir      %{_sysconfdir}/%{name}
%define man_dir       %{_mandir}
%define log_file      %{_localstatedir}/log/%{daemon}
%define pid_file      %{_localstatedir}/run/%{daemon}.pid
%define conf_file     %{conf_dir}/wwwoffle.conf
%define dontget_file  %{conf_dir}/%{dontget}

Version: 2.8a
Release: 1
Group: Networking/Daemons
Vendor: Andrew M. Bishop  <amb@gedanken.demon.co.uk>
Packager: Davide Bolcioni <dbolcion@libero.it>
URL: http://www.gedanken.demon.co.uk/wwwoffle/
Summary: WWW Offline Explorer - Caching Web Proxy Server
Summary(it): WWW Offline Explorer - Proxy Server per connessioni via modem
Copyright: GPL

Source0: http://www.gedanken.demon.co.uk/download-wwwoffle/wwwoffle-%{version}.tgz

Buildroot: /var/tmp/%{name}-root
Prereq: /sbin/chkconfig, /usr/sbin/useradd

%description
A proxy HTTP/FTP server for computers with intermittent internet access.
- Caching of pages viewed while connected for review later.
- Browsing of cached pages while not connected, with the ability
  to follow links and mark other pages for download.
- Downloading of specified pages non-interactively.
- Multiple indices of pages stored in cache for easy selection.
- Interactive or command line option to select pages for fetching
  individually or recursively.
- All options controlled using a simple configuration file with a
  web page to edit it.
  
%description -l it
Un proxy server HTTP/FTP per computer con accesso ad Internet via modem.
- Memorizza le pagine navigate durante il collegamento per consentire
  una successiva consultazione anche a modem inattivo.
- Permette la consultazione di pagine senza connessione ad Internet,
  con la possibilita' di seguire i link e annotare le pagine
  per un successivo scaricamento.
- Possibilita' di caricare pagine specificate in modo automatico.
- Indici multipli delle pagine memorizzate per una facile consultazione.
- Interazione tramite linea di comando o interattiva via browser
  con possibilita' di indicare pagine singoli o in modo ricorsivo
  a partire dalla pagina scelta.
- Tutte le opzioni sono controllate da un semplice file di configurazione
  che puo' essere modificato direttamente dal browser.

%package htdig
Summary:	Indexing and searching of WWWOFFLE's cache with htdig.
Group:		Networking/Daemons
Requires:	htdig
Requires:	%{name} = %{version}-%{release}

%description htdig
Indexing and searching of WWWOFFLE's cache with htdig.

%package mnogosearch
Summary:	Indexing and searching of WWWOFFLE's cache with mnogosearch.
Group:		Networking/Daemons
Requires:	mnogosearch
Requires:	%{name} = %{version}-%{release}

%description mnogosearch
Indexing and searching of WWWOFFLE's cache with mnogosearch.

%package namazu
Summary:	Indexing and searching of WWWOFFLE's cache with namazu.
Group:		Networking/Daemons
Requires:	namazu
Requires:	%{name} = %{version}-%{release}

%description namazu
Indexing and searching of WWWOFFLE's cache with namazu.

%prep
%setup -q

%build
%configure \
	--with-spooldir=%{spool_dir} \
	--with-confdir=%{conf_dir}
make all

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR=$RPM_BUILD_ROOT \
     docdir=%{_builddir}/%{buildsubdir}/doctmp install

perl -i -p -e '
  s|^#run-uid\s*=.*$| run-uid           = %{name}|;
  s|^#run-gid\s*=.*$| run-gid           = %{name}|;
  s|^ use-syslog\s*=.*$| use-syslog        = no|;
  s|^DontGet|# Uncomment to use a separate file.\n#DontGet\n#[\n# %{dontget}\n#]\nDontGet|
' $RPM_BUILD_ROOT%{conf_dir}/*.conf

install -m755 -d $RPM_BUILD_ROOT%{init_dir} \
                 $RPM_BUILD_ROOT/etc/cron.daily \
                 $RPM_BUILD_ROOT/etc/cron.weekly \
                 $RPM_BUILD_ROOT/etc/logrotate.d

cat > wwwoffled <<'EOF'
#! /bin/sh
# chkconfig: 2345 85 20
# pidfile: %{pid_file}
# config: %{conf_file}
# description: WWW Offline Explorer - Caching Web Proxy Server

# Author[s]:
#   Davide Bolcioni  dbolcion@libero.it
#
# Revision History:
#   21-Dic-2003  Davide  Originally from contrib/redhat{1,2};
#                        introduced wwwoffled-start to write pidfile;
#                        added initscripts tags above substituted from spec;
#                        used sourced functions, added condrestart;

# Source function library.
. /etc/rc.d/init.d/functions

# Source networking configuration.
. /etc/sysconfig/network

# Check that networking is up.
[ ${NETWORKING} = "no" ] && exit 0

# Substituted data.
prog=%{daemon}
CONFIG="%{conf_file}"
LOCK="/var/lock/subsys/%{daemon}"
DAEMON="%{inst_dir}/sbin/%{daemon}"
HELPER="%{_sbindir}/wwwoffled-start"

[ -x "$DAEMON" -a -x "$HELPER" ] || exit 0

start()
{
    # Start daemon.
    echo -n "Starting $prog: "
    daemon --check="$prog" "$HELPER" -c "$CONFIG" || return 1
    local pid="`pidofproc $prog`"
    [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null || return 1
    touch "$LOCK"
}

stop()
{
    # Stop daemons.
    echo -n "Shutting down $prog: "
    killproc $prog; local status=$?
    rm -f "$LOCK"; return $status
}


# See how we were called.
RETVAL=0
case "$1" in
  start)
	start
        RETVAL=$?; echo
        ;;
  stop)
	stop
        RETVAL=$?; echo
        ;;
  restart)
	stop; echo; start
        RETVAL=$?; echo
        ;;
  condrestart)
        if [ -f "$LOCK" ]; then
          stop; echo; start
          RETVAL=$?; echo
        fi
	;;
  reload)
        echo -n "Reloading $prog configuration: "
	killproc $prog -HUP
        RETVAL=$?; echo
        ;;
  status)
	status $prog
        RETVAL=$?
        ;;
  *)
        echo "Usage: wwwoffle {start|stop|restart|condrestart|reload|status}"
        exit 1
esac

exit $RETVAL

EOF

install -m755 wwwoffled $RPM_BUILD_ROOT%{init}

cat > wwwoffled-start <<'EOF'
#! /bin/bash
%{_sbindir}/wwwoffled "$@" -d >> %{log_file} 2>&1 &
echo $! > %{pid_file}
EOF

install -m755 wwwoffled-start $RPM_BUILD_ROOT%{_sbindir}

ln -s %{spool_dir}/html/search/htdig/scripts/wwwoffle-htdig-full \
      $RPM_BUILD_ROOT/etc/cron.daily/%{name}-full-index

cat >> purge <<EOF
#!/bin/sh
exec %{inst_dir}/bin/wwwoffle -purge
EOF
install -m755 purge $RPM_BUILD_ROOT/etc/cron.weekly/%{name}-purge

cat >>logrotate <<EOF
%{log_file} {
        copytruncate
        compress
}
EOF
install -m644 logrotate $RPM_BUILD_ROOT/etc/logrotate.d/%{daemon}

install -m640 contrib/dontget/DontGet.txt $RPM_BUILD_ROOT%{dontget_file}

install -m755 src/convert-cache $RPM_BUILD_ROOT%{inst_dir}/sbin/convert-cache

mkdir $RPM_BUILD_ROOT%{spool_dir}/ftp
mkdir $RPM_BUILD_ROOT%{spool_dir}/finger

%clean
[ "$RPM_BUILD_ROOT" != "" ] && rm -rf $RPM_BUILD_ROOT

%pre
id -u "%{name}" >/dev/null 2>&1 ||
  useradd -d "%{spool_dir}" -r -s /bin/false "%{name}" -c "%{name} daemon" >/dev/null || exit 1
test "%{name}" = `id -ng "%{name}"` || exit 1
test -x "%{init}" && "%{init}" stop >/dev/null 2>&1
exit 0

%post
chkconfig --add %{daemon}

%preun
if [ $1 = 0 ]; then
  test -x "%{init}" && "%{init}" stop >/dev/null 2>&1
  chkconfig --del %{daemon}
fi
exit 0

%postun
if [ $1 = 0 ] ; then
  userdel "%{name}"
  [ -d "%{spool_dir}" ] && rm -rf "%{spool_dir}" 2>/dev/null
fi
exit 0

%files
%defattr(-,root,root)
%doc doc/ANNOUNCE ChangeLog* LSM 
%doc doctmp/*
%attr(-,%{name},%{name}) %dir %{spool_dir}
%attr(-,%{name},%{name})      %{spool_dir}/html
%attr(-,%{name},%{name})      %{spool_dir}/ftp
%attr(-,%{name},%{name})      %{spool_dir}/finger
%attr(-,%{name},%{name}) %dir %{spool_dir}/http
%attr(-,%{name},%{name}) %dir %{spool_dir}/lasttime
%attr(-,%{name},%{name}) %dir %{spool_dir}/monitor
%attr(-,%{name},%{name}) %dir %{spool_dir}/outgoing
%attr(-,%{name},%{name}) %config(missingok) %{spool_dir}/outgoing/*
%attr(-,%{name},%{name}) %dir %{spool_dir}/prevtime1
%attr(-,%{name},%{name}) %dir %{spool_dir}/search
%attr(0640,%{name},%{name}) %config %{conf_file}
%attr(0640,%{name},%{name}) %config(noreplace) %{dontget_file}
%attr(-,%{name},%{name}) %dir %{conf_dir}
%{init}
/etc/cron.weekly/%{name}-purge
/etc/logrotate.d/*
%{inst_dir}/bin/*
%{inst_dir}/sbin/*
%doc %{man_dir}/*/*

%files htdig
%attr(-,%{name},%{name}) %dir %{spool_dir}/search/htdig
%attr(-,%{name},%{name})      %{spool_dir}/search/htdig/*
/etc/cron.daily/%{name}-full-index

%files mnogosearch
%attr(-,%{name},%{name}) %dir %{spool_dir}/search/mnogosearch
%attr(-,%{name},%{name})      %{spool_dir}/search/udmsearch
%attr(-,%{name},%{name})      %{spool_dir}/search/mnogosearch/*

%files namazu
%attr(-,%{name},%{name}) %dir %{spool_dir}/search/namazu
%attr(-,%{name},%{name})      %{spool_dir}/search/namazu/*

%changelog
* Mon Dec 16 2003 Davide Bolcioni <dbolcion@libero.it>
- spec file upgraded for 2.8a;
- split searching off into subpackages htdig, mnogosearch and namazu;
- removed redundant variables from make invocation;
- moved man pages into %%{man_dir};
- arranged for use of %%configure instead of direct invocation;
- arranged for use of RPM macros;
- no need to set INSTDIR, CONFDIR and SPOOLDIR explicitly;
- removed perl-patching of search conf and scripts, nothing to patch;
- made wwwoffle-dontget.conf a %%config(noreplace);
- made stuff under outgoing %%config(missingok);

* Wed Nov 29 2000 Werner Bosse <wbosse@berlin.snafu.de>
- spec file upgraded for 2.6
- patch of (build)scripts substituted by more robust inline perl scripts
- shutdown of running wwwoffled during %%pre(un)
- runtime data <spooldir>/html are no longer decl. as %%doc
- logrotate conf added
- DontGet conf file added

* Tue Mar 28 2000 Gianpaolo Macario <http://www.geocities.com/gmacario>
- Upgraded to version 2.5d

* Tue Nov 16 1999 Andrea Borgia <borgia@students.cs.unibo.it>
- upgraded to version 2.5b

* Sat Oct 23 1999 Andrea Borgia <borgia@students.cs.unibo.it>
- upgraded to version 2.5a

* Tue Oct 05 1999 Andrea Borgia <borgia@students.cs.unibo.it>
- upgraded to version 2.5
- fixed subsys locking in startup script
- now runs as non-privileged user 'wwwoffle'

* Sun May 23 1999 Andrea Borgia <borgia@students.cs.unibo.it>
- moved binaries to /usr (/usr/local is for non-packaged sw)
- disabled debugging during compilation
- re-enabled buildroot
- moved changelog to end-of-file (where it belongs)
- reorganized and fixed spec
- implemented clean section

* Wed May 12 1999 Gianpaolo Macario <gianpi@geocities.com>
- Upgraded to wwwoffle-2.4d
- Added post/preun scripts in /etc/cron.daily:
  wwwoffle-htdig-full, wwwoffle-htdig-purge

* Fri Feb 12 1999 Gianpaolo Macario <gianpi@geocities.com>
- Upgraded to wwwoffle-2.4b
- Added files in %doc
- Removed rc*.d from %install, %files

* Thu Jan 6 1999 Gianpaolo Macario <gianpi@geocities.com>
- Upgraded to wwwoffle-2.4a
- Added BuildRoot
- Added kill script at runlevel 2
- Added support for chkconfig
- Synced chkconfig with files

* Tue Jan 4 1999 Gianpaolo Macario <gianpi@geocities.com>
- Upgraded to wwwoffle-2.4
- Translated summary and description into Italian.
