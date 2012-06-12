/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/errors.h,v 2.14 2005-02-20 16:53:50 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8f.
  Error logging header file.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,2000,01,02,03,04,05 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef ERRORS_H
#define ERRORS_H    /*+ To stop multiple inclusions. +*/

/* Definitions that cause errno not to be used. */

#define ERRNO_USE_H_ERRNO   -1  /*+ Use h_errno (DNS resolver errors). +*/
#define ERRNO_USE_IO_ERRNO  -2  /*+ Use io_errno (IO function errors). +*/
#define ERRNO_USE_GAI_ERRNO -3  /*+ Use gai_errno (IPv6 network errors). +*/


/*+ The different error levels. +*/
typedef enum _ErrorLevel
{
 ExtraDebug,                    /*+ For extra debugging (not in syslog). +*/
 Debug,                         /*+ For debugging (debug in syslog). +*/
 Inform,                        /*+ General information (info in syslog). +*/
 Important,                     /*+ Important information (notice in syslog). +*/
 Warning,                       /*+ A warning (warning in syslog). +*/
 Fatal                          /*+ A fatal error (err in syslog). +*/
}
ErrorLevel;


/* In errors.c */

/*+ The level of error logging +*/
extern ErrorLevel SyslogLevel,  /*+ in the config file for syslog and stderr. +*/
                  StderrLevel;  /*+ on the command line for stderr. +*/


void InitErrorHandler(char *name,int syslogable,int stderrable);
void OpenErrorLog(char *name);
void PrintMessage(ErrorLevel errlev,const char* fmt, ...);
char /*@special@*/ *GetPrintMessage(ErrorLevel errlev,const char* fmt, ...) /*@defines result@*/;

#endif /* ERRORS_H */
