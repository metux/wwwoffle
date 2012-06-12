@echo off

set CYGWIN=binmode

set path=c:\wwwoffle\bin

rem
rem Stop WWWOFFLED
rem

c:\wwwoffle\bin\wwwoffle -kill -c /wwwoffle/wwwoffle.conf
