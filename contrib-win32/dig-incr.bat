@echo off

set CYGWIN=binmode

set path=c:\wwwoffle\bin;c:\wwwoffle\htdig\bin

rem
rem Do an incremental htdig
rem

sh /wwwoffle/spool/html/htdig/scripts/wwwoffle-htdig-incr
