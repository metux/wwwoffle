/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/wwwoffle.h,v 2.117 2007-09-29 18:54:08 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9d.
  A header file for all of the programs wwwoffle, wwwoffled.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996-2007 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef WWWOFFLE_H
#define WWWOFFLE_H    /*+ To stop multiple inclusions. +*/

#include "autoconfig.h"

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

#include "misc.h"


/* In connect.c */

void CommandConnect(int client);
void ForkRunModeScript(/*@null@*/ char *filename,char *mode,/*@null@*/ char *arg,int client);
void ForkServer(int fd);


/* In purge.c */

void PurgeCache(int fd);


/* In spool.c */

int OpenNewOutgoingSpoolFile(void);
void CloseNewOutgoingSpoolFile(int fd,URL *Url);
int OpenExistingOutgoingSpoolFile(/*@out@*/ URL **Url);

void CloseOutgoingSpoolFile(int fd,URL *Url);
int ExistsOutgoingSpoolFile(URL *Url);
char /*@null@*/ *HashOutgoingSpoolFile(URL *Url);
char /*@only@*/ /*@null@*/ *DeleteOutgoingSpoolFile(/*@null@*/ URL *Url);

int OpenWebpageSpoolFile(int rw,URL *Url);
char /*@only@*/ /*@null@*/ *DeleteWebpageSpoolFile(URL *Url,int all);
void TouchWebpageSpoolFile(URL *Url,time_t when);
time_t ExistsWebpageSpoolFile(URL *Url);

void CreateBackupWebpageSpoolFile(URL *Url);
void RestoreBackupWebpageSpoolFile(URL *Url);
void DeleteBackupWebpageSpoolFile(URL *Url);
int OpenBackupWebpageSpoolFile(URL *Url);

int CreateLockWebpageSpoolFile(URL *Url);
void DeleteLockWebpageSpoolFile(URL *Url);
int ExistsLockWebpageSpoolFile(URL *Url);

int CreateLastTimeSpoolFile(URL *Url);
char /*@only@*/ /*@null@*/ *DeleteLastTimeSpoolFile(URL *Url);
void CycleLastTimeSpoolFile(void);
void CycleLastOutSpoolFile(void);

int CreateMonitorSpoolFile(URL *Url,char MofY[13],char DofM[32],char DofW[8],char HofD[25]);
long ReadMonitorTimesSpoolFile(URL *Url,/*@out@*/ char MofY[13],/*@out@*/ char DofM[32],/*@out@*/ char DofW[8],/*@out@*/ char HofD[25]);
char /*@only@*/ /*@null@*/ *DeleteMonitorSpoolFile(/*@null@*/ URL *Url);

URL /*@null@*/ *FileNameToURL(const char *file);
char /*@observer@*/ *URLToFileName(URL *Url,char prefix,char postfix);
char /*@observer@*/ *URLToDirName(URL *Url);

int ChangeToSpoolDir(const char *dir);
int /*@alt void@*/ ChangeBackToSpoolDir(void);


/* In parse.c */

URL /*@null@*/ *ParseRequest(int fd,/*@out@*/ Header **request_head,/*@out@*/ Body **request_body);

int RequireForced(const Header *request_head,const URL *Url,int online);
int RequireChanges(int fd,Header *request_head,const URL *Url);
int IsModified(int fd,const Header *request_head);
URL /*@null@*/ *MovedLocation(const URL *Url,const Header *reply_head);
Header *RequestURL(const URL *Url,/*@null@*/ const URL *refererUrl);

void FinishParse(void);

void ModifyRequest(const URL *Url,Header *request_head);

void MakeRequestProxyAuthorised(const URL *proxyUrl,Header *request_head);
void MakeRequestNonProxy(const URL *Url,Header *request_head);

int ParseReply(int fd,/*@out@*/ Header **reply_head);

int SpooledPageStatus(URL *Url,int backup);

int WhichCompression(char *content_encoding);

void ModifyReply(const URL *Url,Header *reply_head);


/* In messages.c (messages.l) */

void SetMessageOptions(int compressed,int chunked);
void HTMLMessage(int fd,int status_val,char *status_str,/*@null@*/ char *location,char *template, ...);
void HTMLMessageHead(int fd,int status_val,char *status_str, ...);
void HTMLMessageBody(int fd,char *template, ...);
char /*@observer@*/ *HTMLMessageString(char *template, ...);
void FinishMessages(void);


/* In local.c */

void LocalPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);

int OpenLanguageFile(char* search);
void SetLanguage(char *accept);


/* In cgi.c */

void LocalCGI(int fd,URL *Url,char *file,Header *request_head,/*@null@*/ Body *request_body);


/* In index.c */

void IndexPage(int fd,URL *Url);


/* In info.c */

void InfoPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);


#if USE_GNUTLS

/* In certinfo.c */

void CertificatesPage(int fd,URL *Url,Header *request_head);

#endif


/* In control.c */

void ControlPage(int fd,URL *Url,/*@null@*/ Body *request_body);


/* In controledit.c */

void ControlEditPage(int fd,char *request_args,/*@null@*/ Body *request_body);


/* In configuration.h */

void ConfigurationPage(int fd,URL *Url,/*@null@*/ Body *request_body);


/* In refresh.c */

URL /*@null@*/ *RefreshPage(int fd,URL *Url,/*@null@*/ Body *request_body,int *recurse);
void DefaultRecurseOptions(URL *Url,Header *head);
int RecurseFetch(URL *Url);
int RecurseFetchRelocation(URL *Url,URL *locationUrl);
char /*@only@*/ *CreateRefreshPath(URL *Url,char *limit,int depth,
                                   int force,
                                   int stylesheets,int images,int frames,int iframes,int scripts,int objects);
int RefreshForced(void);


/* In monitor.c */

void MonitorPage(int fd,URL *Url,/*@null@*/ Body *request_body);
void RequestMonitoredPages(void);
void MonitorTimes(URL *Url,/*@out@*/ int *last,/*@out@*/ int *next);


/* In wwwoffles.c */

int wwwoffles(int online,int fetching,int client);

ssize_t wwwoffles_read_data(/*@out@*/ char *data,size_t len);
ssize_t wwwoffles_write_data(/*@in@*/ const char *data,size_t len);


/* In search.c */

void SearchPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);


#endif /* WWWOFFLE_H */
