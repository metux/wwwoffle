/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/errors.c,v 2.52 2007-03-20 18:34:40 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Generate error messages in a standard format optionally to syslog and stderr.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <fcntl.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef __CYGWIN__
#include "config.h"
#endif


#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif


/* syslog definitions */

#if !defined(__CYGWIN__)
#include <syslog.h>

#if defined (hpux)
/* HP/UX does not declare these in <syslog.h> */
extern int syslog (int pri, const char *message, ...);
extern int openlog (const char *ident, int logopt, int facility);
extern int closelog (void);
#endif

#else
/* I prefer not to use the Cygwin syslog functions here. */
static void openlog(char *facility);
static void closelog(void);
static void syslog(int level,const char* format,char* string);
#endif /* __CYGWIN__ */


/* errno and str_error() definitions */

#include <errno.h>

#if defined(__sun__) && !defined(__svr4__)
/* SunOS 4.x does not have strerror(). */
char* strerror(int err);

extern int sys_nerr;
extern char *sys_errlist[];

char* strerror(int err)
{
 if(err>0 && err<sys_nerr)
    return(sys_errlist[err]);
 else
    return("Unknown error");
}
#endif /* defined(__sun__) && !defined(__svr4__) */


/* h_errno and h_strerror() definitions */

#include <netdb.h>

#if defined (hpux)
/* HP/UX has h_errno but does not declare it in any header file */
extern int h_errno;
#endif

/* A function to get an error message for h_errno. */

static /*@observer@*/ char* h_strerror(int err);

static char* h_strerror(int err)
{
#ifdef NETDB_INTERNAL
 if(err==NETDB_INTERNAL)
    return("Name Lookup Internal error");
 else
#endif
#ifdef NETDB_SUCCESS
 if(err==NETDB_SUCCESS)
    return("Name Lookup Success");
 else
#endif
#ifdef HOST_NOT_FOUND
 if(err==HOST_NOT_FOUND)
    return("Name Lookup Authoritative Answer Host not found");
 else
#endif
#ifdef TRY_AGAIN
 if(err==TRY_AGAIN)
    return("Name Lookup Non-Authoritative Answer Host not found");
 else
#endif
#ifdef NO_RECOVERY
 if(err==NO_RECOVERY)
    return("Name Lookup Non recoverable error");
 else
#endif
#ifdef NO_DATA
 if(err==NO_DATA)
    return("Name Lookup Valid name, no data record of requested type");
 else
#endif
#ifdef NO_ADDRESS
 if(err==NO_ADDRESS)
    return("Name Lookup Valid name, no data record of requested type");
 else
#endif
 return("Unknown error");
}


#include "io.h"
#include "errors.h"

/* io_errno and io_errstr definitions */

/*+ The chunked encoding/compression error number. +*/
extern int io_errno;

/*+ The chunked encoding/compression error message string. +*/
extern char *io_strerror;


/* gai_errno and gai_strerror() definitions */

#if USE_IPV6
/*+ The IPv6 functions error number. +*/
extern int gai_errno;
#endif


/* The function that does the work. */

static char /*@observer@*/ *print_message(ErrorLevel errlev,const char* fmt,va_list ap);

/*+ The name of the program. +*/
static char *program="?";

/*+ The process id of the program. +*/
static pid_t pid;

/*+ The last time that a message was printed. +*/
static time_t last_time=0;

/*+ The error messages. +*/
static const char *ErrorString[]={"ExtraDebug","Debug"  ,"Information","Important","Warning"  ,"Fatal"};

/*+ The priority to apply to syslog messages. +*/
#if defined(__CYGWIN__)
static const int ErrorPriority[]={-1          ,Debug    ,Inform       ,Important  ,Warning    ,Fatal};
#else
static const int ErrorPriority[]={-1          ,LOG_DEBUG,LOG_INFO     ,LOG_NOTICE ,LOG_WARNING,LOG_ERR};
#endif

/*+ The error facility to use, +*/
static int use_syslog=0,        /*+ use syslog. +*/
           use_stderr=1;        /*+ use stderr. +*/

/*+ The level of error logging +*/
ErrorLevel SyslogLevel=Important, /*+ in the config file for syslog. +*/
           StderrLevel=-1;        /*+ on the command line for stderr. +*/


/*++++++++++++++++++++++++++++++++++++++
  Initialise the error handler, get the program name and pid.

  char *name The name of the program.

  int syslogable Set to true if the errors are allowed to go to syslog (or -1 to remain unchanged).

  int stderrable Set to true if the errors are allowed to go to stderr (or -1 to remain unchanged).
  ++++++++++++++++++++++++++++++++++++++*/

void InitErrorHandler(char *name,int syslogable,int stderrable)
{
 if(use_syslog && *program!='?')
    closelog();

 program=name;
 if(syslogable!=-1)
    use_syslog=syslogable;
 if(stderrable!=-1)
    use_stderr=stderrable;

 if(use_syslog)
   {
#if defined(__CYGWIN__)
    openlog(program);
#elif defined(__ultrix__)
    openlog(program,LOG_PID);
#else
    openlog(program,LOG_CONS|LOG_PID,LOG_DAEMON);
#endif
    atexit(closelog);
   }

 pid=getpid();

 last_time=time(NULL)-3590;
}


/*++++++++++++++++++++++++++++++++++++++
  Open the log file.

  char *name The name of the log file.
  ++++++++++++++++++++++++++++++++++++++*/

void OpenErrorLog(char *name)
{
 int log;

 close(STDERR_FILENO);

 log=open(name,O_WRONLY|O_CREAT|O_BINARY,0600);

 use_stderr=1;

 if(log==-1)
   {
    use_stderr=0;
    PrintMessage(Warning,"Cannot open log file '%s' [%!s].",name);
   }
 else
   {
    lseek(log,(off_t)0,SEEK_END);

    if(log!=STDERR_FILENO)
      {
       if(dup2(log,STDERR_FILENO)==-1)
         {
          use_stderr=0;
          PrintMessage(Warning,"Cannot put log file on stderr [%!s].");
         }

       close(log);
      }
   }

 last_time=1;
}


/*++++++++++++++++++++++++++++++++++++++
  Print an error message.

  ErrorLevel errlev Which error level.

  const char* fmt The format of the message.

  ... The rest of the arguments (printf style).
  ++++++++++++++++++++++++++++++++++++++*/

void PrintMessage(ErrorLevel errlev,const char* fmt, ...)
{
 va_list ap;

 /* Shortcut (bypass if debug) */

 if(errlev<=Debug && (errlev<SyslogLevel && errlev<StderrLevel))
    return;

 /* Print the message */

#ifdef __STDC__
 va_start(ap,fmt);
#else
 va_start(ap);
#endif

 print_message(errlev,fmt,ap);

 va_end(ap);
}


/*++++++++++++++++++++++++++++++++++++++
  Print an error message and return the string.

  char *GetPrintMessage Return the error message (except the pid etc.).

  ErrorLevel errlev Which error level.

  const char* fmt The format of the message.

  ... The rest of the arguments (printf style).
  ++++++++++++++++++++++++++++++++++++++*/

char *GetPrintMessage(ErrorLevel errlev,const char* fmt, ...)
{
 va_list ap;
 char *string,*malloced;

 /* Shortcut (bypass if debug) */

 if(errlev<=Debug && (errlev<SyslogLevel && errlev<StderrLevel))
    return(NULL);

 /* Print the message */

#ifdef __STDC__
 va_start(ap,fmt);
#else
 va_start(ap);
#endif

 string=print_message(errlev,fmt,ap);

 va_end(ap);

 /* Copy the message and return it */

 malloced=(char*)malloc(strlen(string)+1);
 strcpy(malloced,string);

 return(malloced);
}


/*++++++++++++++++++++++++++++++++++++++
  Print an error message and return the string.

  char *print_message Return the error message (except the pid etc.).

  ErrorLevel errlev Which error level.

  const char* fmt The format of the message.

  va_list ap The rest of the arguments (printf style).
  ++++++++++++++++++++++++++++++++++++++*/

static char *print_message(ErrorLevel errlev,const char* fmt,va_list ap)
{
 int save_errno = errno;
 size_t str_len=1+strlen(fmt);
 static /*@only@*/ char* string=NULL;
 int i,j;
 time_t this_time=time(NULL);

 /* Periodic timestamp */

 if(last_time && (this_time-last_time)>3600)
   {
    last_time=this_time;
    if(use_stderr)
       fprintf(stderr,"%s[%ld] Timestamp: %s",program,(long)pid,ctime(&this_time)); /* Used in audit-usage.pl */
   }

 /* Prepare a string. */

 if(string)
    free(string);

 string=(char*)malloc(str_len);

 /* Parsing of printf style arguments. */

 for(i=0,j=0;fmt[i];i++)
    if(fmt[i]!='%')
       string[j++]=fmt[i];
    else
      {
       char str[20+16],*strp=NULL; /* 20 characters for 64-bit unsigned (%llu) value ~0. */

       switch(fmt[++i])
         {
         case '!':
          if(fmt[++i]=='s')
            {
             if(save_errno==ERRNO_USE_H_ERRNO)
                strp=h_strerror(h_errno);
             else if(save_errno==ERRNO_USE_IO_ERRNO)
                strp=io_strerror;
#if USE_IPV6
             else if(save_errno==ERRNO_USE_GAI_ERRNO)
                strp=(char*)gai_strerror(gai_errno);
#endif
             else
                strp=strerror(save_errno);
            }
          else
            {
             if(save_errno==ERRNO_USE_H_ERRNO)
                sprintf(strp=str,"%d (h_errno)",h_errno);
             else if(save_errno==ERRNO_USE_IO_ERRNO)
                sprintf(strp=str,"%d (io_errno)",io_errno);
#if USE_IPV6
             else if(save_errno==ERRNO_USE_GAI_ERRNO)
                sprintf(strp=str,"%d (gai_errno)",gai_errno);
#endif
             else
                sprintf(strp=str,"%d",save_errno);
            }
          break;

         case 'c':
          str[0]=(char)va_arg(ap,int); /* beware of type promotion */
          str[1]=0;
          strp=str;
          break;

         case 'd':
          sprintf(strp=str,"%d",va_arg(ap,int));
          break;

         case 's':
          strp=va_arg(ap,char*);
          if(!strp) strp="(null)";
          break;

         default:
          str[0]='%';
          str[1]=fmt[i];
          str[2]=0;
          strp=str;
          (void)va_arg(ap,void*);
         }

       str_len+=strlen(strp);
       string=realloc(string,str_len);
       strcpy(&string[j],strp);
       j+=strlen(strp);
      }

 string[j--]=0;

 while(j>=0 && (string[j]=='\r' || string[j]=='\n'))
    string[j--]=0;

 /* Output the result. */

 if(use_syslog && errlev>=SyslogLevel && ErrorPriority[errlev]!=-1)
    syslog(ErrorPriority[errlev],"%s",string);

 if(use_stderr && ((StderrLevel==-1 && errlev>=SyslogLevel) || (StderrLevel!=-1 && errlev>=StderrLevel)))
   {
    if(SyslogLevel==ExtraDebug || StderrLevel==ExtraDebug)
       fprintf(stderr,"%s[%ld] %10ld: %s: %s\n",program,(long)pid,(long)this_time,ErrorString[errlev],string);
    else
       fprintf(stderr,"%s[%ld] %s: %s\n",program,(long)pid,ErrorString[errlev],string);
   }

 if(errlev==Fatal)
    exit(2);

 return(string);
}


#if defined(__CYGWIN__)

static int syslog_file=-1;
static char *syslog_facility=NULL;

/*++++++++++++++++++++++++++++++++++++++
  Open the syslog file.
  ++++++++++++++++++++++++++++++++++++++*/

static void openlog(char *facility)
{
 char *config_file=ConfigurationFileName();
 char *syslogfile=(char*)malloc(strlen(config_file)+16),*p;

 strcpy(syslogfile,config_file);
 p=syslogfile+strlen(syslogfile)-1;
 while(p>=syslogfile && *p!='/')
    *p--=0;
 strcat(syslogfile,"wwwoffle.syslog");

 syslog_file=open(syslogfile,O_WRONLY|O_CREAT|O_APPEND,0600);

 if(syslog_file!=-1)
    init_io(syslog_file);

 syslog_facility=facility;
}


/*++++++++++++++++++++++++++++++++++++++
  Close the syslog file.
  ++++++++++++++++++++++++++++++++++++++*/

static void closelog(void)
{
 if(syslog_file!=-1)
   {
    finish_io(syslog_file);
    close(syslog_file);
   }

 syslog_file=-1;
 syslog_facility=NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the syslog file.
  ++++++++++++++++++++++++++++++++++++++*/

static void syslog(int level,const char* format,char* string)
{
 if(syslog_file!=-1 && syslog_facility)
    write_formatted(syslog_file,"%s[%ld](%s): %s\r\n",syslog_facility,(long)pid,ErrorString[level],string);
}

#endif /* __CYGWIN__ */
