@echo off

set CYGWIN=binmode

set path=c:\wwwoffle\bin;c:\wwwoffle\htdig\bin

rem
rem Create the fuzzy database
rem

sh /wwwoffle/spool/html/htdig/scripts/wwwoffle-htfuzzy
