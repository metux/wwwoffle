@echo off

set CYGWIN=binmode

set path=c:\wwwoffle\bin

rem
rem Put WWWOFFLE Online
rem

c:\wwwoffle\bin\wwwoffle -online -c /wwwoffle/wwwoffle.conf

rem
rem Start WWWOFFLE Fetching
rem

c:\wwwoffle\bin\wwwoffle -fetch -c /wwwoffle/wwwoffle.conf
