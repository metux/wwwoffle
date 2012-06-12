Name: wwwoffle

%define init_dir      /etc/rc.d/init.d
%define init          %{init_dir}/%{name}d
%define spool_dir     /var/spool/%{name}
%define inst_dir      /usr
%define conf_dir      /etc/wwwoffle

Version: 2.7d
Release: 4
Group: Networking/Daemons
Vendor: Andrew M. Bishop  <amb@gedanken.demon.co.uk>
Packager: Andrew Bray <andy@chaos.org.uk>
URL: http://www.gedanken.demon.co.uk/wwwoffle/
Summary: WWW Offline Explorer - Caching Web Proxy Server
Summary(it): WWW Offline Explorer - Proxy Server per connessioni via modem
Copyright: GPL

Source0: http://www.gedanken.demon.co.uk/download-wwwoffle/wwwoffle-%{version}.tgz
Source1: wwwoffled.rc
Source2: DontGet.txt
Source3: wwwoffle_hints-tips.txt
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

%prep
%setup

%build
./configure --prefix=%{inst_dir} --with-spooldir=%{spool_dir} --with-confdir=%{conf_dir}
make INSTDIR=%{inst_dir} CONFDIR=%{conf_dir} SPOOLDIR=%{spool_dir} all

%install
rm -rf $RPM_BUILD_ROOT

make INSTDIR=$RPM_BUILD_ROOT%{inst_dir} \
     CONFDIR=$RPM_BUILD_ROOT%{conf_dir} \
     DESTDIR=$RPM_BUILD_ROOT \
     SPOOLDIR=$RPM_BUILD_ROOT%{spool_dir} \
     docdir=%{_builddir}/%{buildsubdir}/doctmp install

perl -i -p -e '
  s|%{buildroot}||g;
  s|^#run-uid\s*=.*$| run-uid           = %{name}|;
  s|^#run-gid\s*=.*$| run-gid           = %{name}|;
  s|^ use-syslog\s*=.*$| use-syslog        = no|;
  s|^DontGet|DontGet\n# See also %{_docdir}/%{name}-%{version}/HINTS-TIPS\n#[\n# %{name}.dont_get\n#]\n|
' $RPM_BUILD_ROOT%{conf_dir}/*.conf

perl -i -p -e 's|%{buildroot}||g' $RPM_BUILD_ROOT%{spool_dir}/html/search/*/conf/* \
                                  $RPM_BUILD_ROOT%{spool_dir}/html/search/*/scripts/*

install -m755 -d $RPM_BUILD_ROOT%{init_dir} \
                 $RPM_BUILD_ROOT/etc/cron.daily \
                 $RPM_BUILD_ROOT/etc/cron.weekly \
                 $RPM_BUILD_ROOT/etc/logrotate.d

install -m755 %SOURCE1 $RPM_BUILD_ROOT%{init}

ln -s %{spool_dir}/html/search/htdig/scripts/wwwoffle-htdig-full \
      $RPM_BUILD_ROOT/etc/cron.daily/%{name}-full-index

cat >> purge <<EOF
#!/bin/sh
exec %{inst_dir}/bin/wwwoffle -purge
EOF
install -m755 purge $RPM_BUILD_ROOT/etc/cron.weekly/%{name}-purge

cat >>logrotate <<EOF
/var/log/wwwoffled {
        copytruncate
        compress
}
EOF
install -m644 logrotate $RPM_BUILD_ROOT/etc/logrotate.d/%{name}

install -m640 %SOURCE2 $RPM_BUILD_ROOT%{conf_dir}/%{name}.dont_get

install -m755 src/convert-cache $RPM_BUILD_ROOT%{inst_dir}/sbin/convert-cache

cp %SOURCE3 HINTS-TIPS

mkdir $RPM_BUILD_ROOT%{spool_dir}/ftp
mkdir $RPM_BUILD_ROOT%{spool_dir}/finger

%clean
[ "$RPM_BUILD_ROOT" != "" ] && rm -rf $RPM_BUILD_ROOT

%pre
[ -x %{init} -a -f /var/lock/subsys/wwwoffled ] && %{init} stop
useradd -d %{spool_dir} -r -s /dev/null %{name} >/dev/null 2>&1
[ -d %{spool_dir} ] && chown -R %{name}:%{name} %{spool_dir}
exit 0

%post
chkconfig --add wwwoffled
cat <<EOF

Please adjust now the %{name} configuration stored in %{conf_dir}/wwwoffle.*
(s.a. wwwoffle.conf(5)).
Afterwards call '%{init} start' with root permission.

Note: In case %{name} is upgraded from versions < 2.6 then the existing cache
contents must be migrated to the new cache format by calling
%{inst_dir}/bin/convert-cache root permission. Please read first the full
description of the conversion process in %{_docdir}/%{name}-%{version}/CONVERT.

EOF
exit 0

%preun
if [ $1 = 0 ]; then
  %{init} stop
  chkconfig --del wwwoffled
fi
exit 0

%postun
if [ $1 = 0 ] ; then
  userdel %{name}
fi
exit 0

%files
%defattr(-,root,root)
%doc ANNOUNCE ChangeLog* LSM HINTS-TIPS
%doc doctmp/*
%attr(-,%{name},%{name}) %dir %{spool_dir}
%attr(-,%{name},%{name})      %{spool_dir}/html
%attr(-,%{name},%{name})      %{spool_dir}/ftp
%attr(-,%{name},%{name})      %{spool_dir}/finger
%attr(-,%{name},%{name}) %dir %{spool_dir}/http
%attr(-,%{name},%{name}) %dir %{spool_dir}/lasttime
%attr(-,%{name},%{name}) %dir %{spool_dir}/monitor
%attr(-,%{name},%{name}) %dir %{spool_dir}/outgoing
%attr(-,%{name},%{name}) %dir %{spool_dir}/prevtime1
%attr(0640,%{name},%{name}) %config %{conf_dir}/wwwoffle*
%attr(-,%{name},%{name}) %dir %{conf_dir}
%{init}
/etc/cron.*/*
/etc/logrotate.d/*
%{inst_dir}/bin/*
%{inst_dir}/sbin/*
%{inst_dir}/man/*/*

%changelog
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
