/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/index.c 2.107 2005/11/09 19:48:24 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Generate an index of the web pages that are cached in the system.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04,05 Andrew M. Bishop
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

#include <sys/stat.h>

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include "wwwoffle.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "proto.h"


/*+ A type to contain the information required to sort the files. +*/
typedef struct _FileIndex
{
 URL *url;                      /*+ The URL of the file. +*/
 char *name;                    /*+ The name of the URL. +*/
 char *host;                    /*+ The host of the URL. +*/
 char *type;                    /*+ The type (file extension) of the URL. +*/
 long time;                     /*+ The time of the file (access or modification as appropriate). +*/
 int random;                    /*+ Number generated from the file name (used for random sort). +*/
}
FileIndex;

/*+ The method of sorting used. +*/
static const char* const sorttype[]={"none"  ,
                                     "mtime" ,
                                     "atime" ,
                                     "dated" ,
                                     "alpha" ,
                                     "domain",
                                     "type"  ,
                                     "random",
                                     "none"  };

/*+ An enumerated type for the sort modes. +*/
typedef enum _SortMode
{
 None,                          /*+ No sorting, natural order. +*/
 MTime,                         /*+ Sort by modification time +*/
 ATime,                         /*+ Sort by access time. +*/
 Dated,                         /*+ Sort by modification time with separators. +*/
 Alpha,                         /*+ Sort Alphabetically. +*/
 Domain,                        /*+ Sort by domain name. +*/
 Type,                          /*+ Sort by type (file extension). +*/
 Random,                        /*+ Sort by checksum of URL. +*/
 NSortModes                     /*+ The number of sort modes. +*/
}
SortMode;


/*+ The list of files. +*/
static /*@null@*/ /*@only@*/ FileIndex **files=NULL;

/*+ The number of files. +*/
static size_t nfiles=0;

/*+ The current time. +*/
static time_t now;

static void IndexRoot(int fd);
static void IndexProtocol(int fd,char *proto,SortMode mode,int allopt);
static void IndexHost(int fd,char *proto,char *host,SortMode mode,int allopt);
static void IndexSpecial(int fd,char *name,SortMode mode,int allopt,int outgoing,int monitor,int lasttime,int lastout);

static void add_dir(char *name,SortMode mode);
static void add_file(char *name,SortMode mode);

static void dated_separator(int fd,int file,int *lastdays,int *lasthours);

static int sort_alpha(FileIndex **a,FileIndex **b);
static int sort_type(FileIndex **a,FileIndex **b);
static int sort_domain(FileIndex **a,FileIndex **b);
static int sort_time(FileIndex **a,FileIndex **b);
static int sort_random(FileIndex **a,FileIndex **b);


/*++++++++++++++++++++++++++++++++++++++
  Generate an index of the pages that are in the cache.

  int fd The file descriptor to write the output to.

  URL *Url The URL that specifies the path to generate the index for.
  ++++++++++++++++++++++++++++++++++++++*/

void IndexPage(int fd,URL *Url)
{
 char *newpath;
 char *proto=NULL,*host="";
 SortMode sort=NSortModes,s;
 int protocol=0,outgoing=0,monitor=0,lasttime=0,prevtime=0,lastout=0,prevout=0,mainpage=0;
 int delopt=0,refopt=0,monopt=0,allopt=0,confopt=0,infoopt=0;
 int i;

 if(!strcmp(Url->path,"/index/url"))
   {
    if(Url->args)
      {
       char *url=URLDecodeFormArgs(Url->args);
       URL *indexUrl=SplitURL(url);
       char *localurl=GetLocalURL();
       char *relocate=(char*)malloc(strlen(localurl)+strlen(indexUrl->proto)+strlen(indexUrl->hostport)+strlen(sorttype[Alpha])+16);

       sprintf(relocate,"%s/index/%s/%s?sort=%s",localurl,indexUrl->proto,indexUrl->hostport,sorttype[Alpha]);
       HTMLMessage(fd,302,"WWWOFFLE Index URL Redirect",relocate,"Redirect",
                   "location",relocate,
                   NULL);

       free(url);
       FreeURL(indexUrl);
       free(localurl);
       free(relocate);
      }
    else
       HTMLMessage(fd,404,"WWWOFFLE Illegal Index Page",NULL,"IndexIllegal",
                   "url",Url->pathp,
                   NULL);

    return;
   }

 newpath=(char*)malloc(strlen(Url->path)-6);
 strcpy(newpath,Url->path+7);

 if(*newpath && newpath[strlen(newpath)-1]=='/')
    newpath[strlen(newpath)-1]=0;

 for(i=0;newpath[i];i++)
    if(newpath[i]=='/')
      {
       newpath[i]=0;
       host=newpath+i+1;
       break;
      }

 proto=newpath;

 for(i=0;i<NProtocols;i++)
    if(!strcmp(proto,Protocols[i].name))
       protocol=i+1;

 outgoing=!strcmp(proto,"outgoing");
 monitor =!strcmp(proto,"monitor");
 lasttime=!strcmp(proto,"lasttime");
 prevtime=!strncmp(proto,"prevtime",(size_t)8) && atoi(proto+8)>=0 && atoi(proto+8)<=NUM_PREVTIME_DIR;
 lastout =!strcmp(proto,"lastout");
 prevout =!strncmp(proto,"prevout",(size_t)7) && atoi(proto+7)>=0 && atoi(proto+7)<=NUM_PREVTIME_DIR;
 mainpage=(!protocol && !outgoing && !monitor && !lasttime && !prevtime && !lastout && !prevout);

 if(Url->args)
   {
    char **args,**a;

    args=SplitFormArgs(Url->args);

    for(a=&args[0];*a;a++)
      {
       if(!strncmp(*a,"sort=",(size_t)5))
         {
          for(s=0;s<NSortModes;s++)
             if(!strcmp(*a+5,sorttype[s]))
               {sort=s;break;}
         }
       else if(!strcmp(*a,"delete"))
          delopt=1;
       else if(!strcmp(*a,"refresh"))
          refopt=1;
       else if(!strcmp(*a,"monitor"))
          monopt=1;
       else if(!strcmp(*a,"all"))
          allopt=1;
       else if(!strcmp(*a,"config"))
          confopt=1;
       else if(!strcmp(*a,"info"))
          infoopt=1;
       else
          PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",*a,Url->name);
      }

    free(args[0]);
    free(args);
   }

 files=NULL;
 nfiles=0;

 now=time(NULL);

 if((*host && (strchr(host,'/') || !strcmp(host,"..") || !strcmp(host,"."))) ||
    (mainpage && Url->path[7]) ||
    ((outgoing || monitor || lasttime || prevtime || lastout || prevout || mainpage) && *host))
    HTMLMessage(fd,404,"WWWOFFLE Illegal Index Page",NULL,"IndexIllegal",
                "url",Url->pathp,
                NULL);
 else if(sort==NSortModes)
   {
    char *localurl=GetLocalURL();
    char *relocate=(char*)malloc(strlen(localurl)+strlen(Url->path)+strlen(sorttype[0])+8);

    sprintf(relocate,"%s%s?sort=%s",localurl,Url->path,sorttype[0]);
    HTMLMessage(fd,302,"WWWOFFLE Unknown Sort Index Redirect",relocate,"Redirect",
                "location",relocate,
                NULL);

    free(relocate);
    free(localurl);
   }
 else
   {
    HTMLMessageHead(fd,200,"WWWOFFLE Index",
                    NULL);
    HTMLMessageBody(fd,"Index-Head",
                    "type",prevtime?"prevtime":prevout?"prevout":proto,
                    "subtype",prevtime?proto+8:prevout?proto+7:host,
                    "sort",sorttype[sort],
                    "delete" ,delopt?";delete":"",
                    "refresh",refopt?";refresh":"",
                    "monitor",monopt?";monitor":"",
                    "all"    ,allopt?";all":"",
                    "config" ,confopt?";config":"",
                    "info"   ,infoopt?";info":"",
                    NULL);

    if(outgoing)
       IndexSpecial(fd,"outgoing",sort,allopt,1,0,0,0);
    else if(monitor)
       IndexSpecial(fd,"monitor",sort,allopt,0,1,0,0);
    else if(lasttime || prevtime)
       IndexSpecial(fd,proto,sort,allopt,0,0,1,0);
    else if(lastout || prevout)
       IndexSpecial(fd,proto,sort,allopt,0,0,0,1);
    else if(protocol && !*host)
       IndexProtocol(fd,proto,sort,allopt);
    else if(protocol && *host)
       IndexHost(fd,proto,host,sort,allopt);
    else
       IndexRoot(fd);

    HTMLMessageBody(fd,"Index-Tail",
                    NULL);
   }

 free(newpath);
}


/*++++++++++++++++++++++++++++++++++++++
  Index the root of the cache.

  int fd The file descriptor to write to.
  ++++++++++++++++++++++++++++++++++++++*/

static void IndexRoot(int fd)
{
 HTMLMessageBody(fd,"IndexRoot-Body",NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Index the hosts for one protocol in the cache.

  int fd The file descriptor to write to.

  char *proto The protocol to index.

  SortMode mode The sort mode to use.

  int allopt The option to show all pages including unlisted ones.
  ++++++++++++++++++++++++++++++++++++++*/

static void IndexProtocol(int fd,char *proto,SortMode mode,/*@unused@*/ int allopt)
{
 DIR *dir;
 struct dirent* ent;
 int i;
 char total[MAX_INT_STR+1];
 int lastdays=0,lasthours=0;

 /* Open the spool directory. */

 if(chdir(proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not indexed.",proto);
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open spool directory '%s' [%!s]; index failed.",proto);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s' [%!s]; index failed.",proto);closedir(dir);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 /* Get all of the host sub-directories. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    add_dir(ent->d_name,mode);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 /* Sort the files. */

 if(files)
   {
    if(mode==MTime || mode==ATime || mode==Dated)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_time);
    else if(mode==Alpha || mode==Type)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_alpha);
    else if(mode==Domain)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_domain);
    else if(mode==Random)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_random);
   }

 /* Output the page. */

 sprintf(total,"%u",(unsigned)nfiles);

 HTMLMessageBody(fd,"IndexProtocol-Head",
                 "proto",proto,
                 "total",total,
                 NULL);

 for(i=0;i<nfiles;i++)
   {
    if(mode==Dated)
       dated_separator(fd,i,&lastdays,&lasthours);

    HTMLMessageBody(fd,"IndexProtocol-Body",
                    "host",files[i]->name,
                    NULL);

    free(files[i]->name);
    free(files[i]);
   }

 HTMLMessageBody(fd,"IndexProtocol-Tail",
                 NULL);

 /* Tidy up and exit */

 if(files)
    free(files);
 files=NULL;
 nfiles=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Create an index of the pages on a host.

  int fd The file descriptor to write to.

  char *proto The protocol to index.

  char *host The name of the subdirectory.

  SortMode mode The sort mode to use.

  int allopt The option to show all pages including unlisted ones.
  ++++++++++++++++++++++++++++++++++++++*/

static void IndexHost(int fd,char *proto,char *host,SortMode mode,int allopt)
{
 DIR *dir;
 struct dirent* ent;
 int i;
 size_t nindexed=0;
 char total[MAX_INT_STR+1],indexed[MAX_INT_STR+1],notindexed[MAX_INT_STR+1];
 int lastdays=0,lasthours=0;

 /* Open the spool subdirectory. */

 if(chdir(proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not indexed.",proto);
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 /* Open the spool subdirectory. */

#if defined(__CYGWIN__)
 for(i=0;host[i];i++)
    if(host[i]==':')
       host[i]='!';
#endif

 if(chdir(host))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s]; not indexed.",proto,host);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory '%s/%s' [%!s]; not indexed.",proto,host);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory '%s/%s' [%!s]; not indexed.",proto,host);closedir(dir);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

#if defined(__CYGWIN__)
 for(i=0;host[i];i++)
    if(host[i]=='!')
       host[i]=':';
#endif

 /* Add all of the file names. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    add_file(ent->d_name,mode);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 /* Sort the files. */

 if(files)
   {
    if(mode==MTime || mode==ATime || mode==Dated)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_time);
    else if(mode==Alpha || mode==Domain)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_alpha);
    else if(mode==Type)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_type);
    else if(mode==Random)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_random);
   }

 /* Output the page. */

 sprintf(total,"%u",(unsigned)nfiles);

 HTMLMessageBody(fd,"IndexHost-Head",
                 "proto",proto,
                 "host",host,
                 "total",total,
                 NULL);

 for(i=0;i<nfiles;i++)
   {
    int is_indexed=ConfigBooleanURL(IndexListAny,files[i]->url);

    if(is_indexed)
       is_indexed=ConfigBooleanURL(IndexListHost,files[i]->url);

    if(allopt || is_indexed)
      {
       if(mode==Dated)
          dated_separator(fd,i,&lastdays,&lasthours);

       HTMLMessageBody(fd,"IndexHost-Body",
                       "url",files[i]->url->name,
                       "link",ProxyableLink(files[i]->url),
                       "item",files[i]->url->pathp,
                       NULL);

       nindexed++;
      }

    FreeURL(files[i]->url);
    free(files[i]);
   }

 sprintf(indexed   ,"%u",(unsigned)nindexed);
 sprintf(notindexed,"%u",(unsigned)(nfiles-nindexed));

 HTMLMessageBody(fd,"IndexHost-Tail",
                 "indexed",indexed,
                 "notindexed",notindexed,
                 NULL);

 /* Tidy up and exit */

 if(files)
    free(files);
 files=NULL;
 nfiles=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Create an index of the requests or cached pages that are in one of the special directories.

  int fd The file descriptor to write to.

  char *name The name of the directory.

  SortMode mode The method to use to sort the names.

  int allopt The option to show all pages including unlisted ones.

  int outgoing Index the outgoing directory.

  int monitor Index the monitor directory.

  int lasttime Index a lasttime or prevtime directory.

  int lastout Index a lastout or prevout directory.
  ++++++++++++++++++++++++++++++++++++++*/

static void IndexSpecial(int fd,char *name,SortMode mode,int allopt,int outgoing,int monitor,int lasttime,int lastout)
{
 DIR *dir;
 struct dirent* ent;
 int i;
 size_t nindexed=0;
 char total[MAX_INT_STR+1],indexed[MAX_INT_STR+1],notindexed[MAX_INT_STR+1],*date;
 int lastdays=0,lasthours=0;
 struct stat buf;

 /* Open the named subdirectory. */

 if(chdir(name))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not indexed.",name);
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory '%s' [%!s]; not indexed.",name);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory '%s' [%!s]; not indexed.",name);closedir(dir);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 if(stat(".timestamp",&buf))
    date="?";
 else
    date=RFC822Date(buf.st_mtime,0);

 /* Add all of the file names. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    add_file(ent->d_name,mode);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 /* Sort the files. */

 if(files)
   {
    if(mode==MTime || mode==ATime || mode==Dated)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_time);
    else if(mode==Alpha)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_alpha);
    else if(mode==Type)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_type);
    else if(mode==Domain)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_domain);
    else if(mode==Random)
       qsort(files,nfiles,sizeof(FileIndex*),(int (*)(const void*,const void*))sort_random);
   }

 /* Output the page. */

 sprintf(total,"%u",(unsigned)nfiles);

 if(outgoing)
    HTMLMessageBody(fd,"IndexOutgoing-Head",
                    "total",total,
                    NULL);
 else if(monitor)
    HTMLMessageBody(fd,"IndexMonitor-Head",
                    "total",total,
                    NULL);
 else if(lasttime)
    HTMLMessageBody(fd,"IndexLastTime-Head",
                    "number",name+8,
                    "total",total,
                    "date",date,
                    NULL);
 else /* if(lastout) */
    HTMLMessageBody(fd,"IndexLastOut-Head",
                    "number",name+7,
                    "total",total,
                    "date",date,
                    NULL);

 for(i=0;i<nfiles;i++)
   {
    int is_indexed=ConfigBooleanURL(IndexListAny,files[i]->url);

    if(is_indexed)
      {
       if(outgoing)
          is_indexed=ConfigBooleanURL(IndexListOutgoing,files[i]->url);
       else if(monitor)
          is_indexed=ConfigBooleanURL(IndexListMonitor,files[i]->url);
       else if(lasttime)
          is_indexed=ConfigBooleanURL(IndexListLatest,files[i]->url);
       else /* if(lastout) */
          is_indexed=ConfigBooleanURL(IndexListLatest,files[i]->url);
      }

    if(allopt || is_indexed)
      {
       if(mode==Dated)
          dated_separator(fd,i,&lastdays,&lasthours);

       if(outgoing)
          HTMLMessageBody(fd,"IndexOutgoing-Body",
                          "url",files[i]->url->name,
                          "link",ProxyableLink(files[i]->url),
                          "item",files[i]->url->name,
                          NULL);
       else if(monitor)
         {
          int last,next;
          char laststr[MAX_INT_STR+3+1],nextstr[MAX_INT_STR+3+1];

          MonitorTimes(files[i]->url,&last,&next);

          if(last>=(366*24))
             sprintf(laststr,"365+");
          else
             sprintf(laststr,"%d:%d",last/24,last%24);
          if(next>=(366*24))
             sprintf(nextstr,"365+");
          else
             sprintf(nextstr,"%d:%d",next/24,next%24);

          HTMLMessageBody(fd,"IndexMonitor-Body",
                          "url",files[i]->url->name,
                          "link",ProxyableLink(files[i]->url),
                          "item",files[i]->url->name,
                          "last",laststr,
                          "next",nextstr,
                          NULL);
         }
       else if(lasttime)
          HTMLMessageBody(fd,"IndexLastTime-Body",
                          "url",files[i]->url->name,
                          "link",ProxyableLink(files[i]->url),
                          "item",files[i]->url->name,
                          NULL);
       else /* if(lastout) */
          HTMLMessageBody(fd,"IndexLastOut-Body",
                          "url",files[i]->url->name,
                          "link",ProxyableLink(files[i]->url),
                          "item",files[i]->url->name,
                          NULL);

       nindexed++;
      }

    FreeURL(files[i]->url);
    free(files[i]);
   }

 sprintf(indexed   ,"%u",(unsigned)nindexed);
 sprintf(notindexed,"%u",(unsigned)(nfiles-nindexed));

 if(outgoing)
    HTMLMessageBody(fd,"IndexOutgoing-Tail",
                    "indexed",indexed,
                    "notindexed",notindexed,
                    NULL);
 else if(monitor)
    HTMLMessageBody(fd,"IndexMonitor-Tail",
                    "indexed",indexed,
                    "notindexed",notindexed,
                    NULL);
 else if(lasttime)
    HTMLMessageBody(fd,"IndexLastTime-Tail",
                    "indexed",indexed,
                    "notindexed",notindexed,
                    NULL);
 else /* if(lastout) */
    HTMLMessageBody(fd,"IndexLastOut-Tail",
                    "indexed",indexed,
                    "notindexed",notindexed,
                    NULL);

 /* Tidy up and exit */

 if(files)
    free(files);
 files=NULL;
 nfiles=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Add a directory to the list.

  char *name The name of the directory.

  SortMode mode The sort mode.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_dir(char *name,SortMode mode)
{
#if defined(__CYGWIN__)
 int i;
#endif
 struct stat buf;

 if(stat(name,&buf))
   {PrintMessage(Inform,"Cannot stat directory '%s' [%!s]; race condition?",name);return;}
 else if(S_ISDIR(buf.st_mode))
   {
    if(!(nfiles%16))
      {
       if(!files)
          files=(FileIndex**)malloc(16*sizeof(FileIndex*));
       else
          files=(FileIndex**)realloc(files,(nfiles+16)*sizeof(FileIndex*));
      }
    files[nfiles]=(FileIndex*)malloc(sizeof(FileIndex));

#if defined(__CYGWIN__)
 for(i=0;name[i];i++)
    if(name[i]=='!')
       name[i]=':';
#endif

    files[nfiles]->name=(char*)malloc(strlen(name)+1);
    strcpy(files[nfiles]->name,name);

    if(mode==Domain)
       files[nfiles]->host=files[nfiles]->name;
    else if(mode==Type)
       files[nfiles]->type="";
    else if(mode==MTime || mode==Dated)
       files[nfiles]->time=buf.st_mtime;
    else if(mode==ATime)
       files[nfiles]->time=buf.st_atime;

    nfiles++;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Add a file to the list.

  char *name The name of the file.

  SortMode mode The sort mode.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_file(char *name,SortMode mode)
{
 URL *Url;
 struct stat buf;

 if((*name!='D' && *name!='O') || name[strlen(name)-1]=='~')
    return;

 Url=FileNameToURL(name);

 if(!Url)
    return;

 if(stat(name,&buf))
   {PrintMessage(Inform,"Cannot stat file '%s' [%!s]; race condition?",name);FreeURL(Url);return;}
 else if(S_ISREG(buf.st_mode))
   {
    if(!(nfiles%16))
      {
       if(!files)
          files=(FileIndex**)malloc(16*sizeof(FileIndex*));
       else
          files=(FileIndex**)realloc(files,(nfiles+16)*sizeof(FileIndex*));
      }
    files[nfiles]=(FileIndex*)malloc(sizeof(FileIndex));

    files[nfiles]->url=Url;
    files[nfiles]->name=files[nfiles]->url->name;

    if(mode==Domain)
       files[nfiles]->host=files[nfiles]->url->hostport;
    else if(mode==Type)
      {
       char *p=files[nfiles]->url->path+strlen(files[nfiles]->url->path)-1;

       files[nfiles]->type="";

       while(p>=files[nfiles]->url->path && *p!='/')
         {
          if(*p=='.')
            files[nfiles]->type=p;
          p--;
         }
      }
    else if(mode==MTime || mode==Dated)
       files[nfiles]->time=buf.st_mtime;
    else if(mode==ATime)
       files[nfiles]->time=buf.st_atime;
    else if(mode==Random)
       files[nfiles]->random=name[1]+256*(int)name[2]+65536*(int)name[3]+16777216*(int)name[4];

    nfiles++;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Write out a date separator for the dated format if needed.

  int fd The file to write to.

  int file The number of the file in the list.

  int *lastdays The age of the previous file in days.

  int *lasthours The age of the previous file in hours.
  ++++++++++++++++++++++++++++++++++++++*/

static void dated_separator(int fd,int file,int *lastdays,int *lasthours)
{
 long days=(now-files[file]->time)/(24*3600),hours=(now-files[file]->time)/3600;

 if(*lastdays!=-1 && *lastdays<days)
   {
    char daystr[MAX_INT_STR+1];
    sprintf(daystr,"%ld",days-*lastdays);

    HTMLMessageBody(fd,"IndexSeparator-Body",
                    "days",daystr,
                    NULL);
   }
 else if(*lasthours!=-1 && *lasthours<(hours-1))
   {
    HTMLMessageBody(fd,"IndexSeparator-Body",
                    "days","",
                    NULL);
   }

 *lastdays=(int)days;
 *lasthours=(int)hours;
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into alphabetical order.

  int sort_alpha Returns the comparison of the pointers to strings.

  FileIndex **a The first FileIndex.

  FileIndex **b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_alpha(FileIndex **a,FileIndex **b)
{
 char *an=(*a)->name;
 char *bn=(*b)->name;

 return(strcmp(an,bn));
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into type (file extension) order.

  int sort_type Returns the comparison of the pointers to strings.

  FileIndex **a The first FileIndex.

  FileIndex **b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_type(FileIndex **a,FileIndex **b)
{
 char *at=(*a)->type;
 char *bt=(*b)->type;
 int chosen;

 /* Compare the type */

 chosen=strcasecmp(at,bt);

 /* Fallback to alphabetical */

 if(!chosen)
    chosen=sort_alpha(a,b);

 return(chosen);
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into domain order.

  int sort_domain Returns the comparison of the pointers to strings.

  FileIndex **a The first FileIndex.

  FileIndex **b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_domain(FileIndex **a,FileIndex **b)
{
 char *ap=(*a)->host+strlen((*a)->host);
 char *bp=(*b)->host+strlen((*b)->host);
 int chosen=0;

 /* Compare longer and longer domain parts */

 do
   {
    ap--;
    bp--;

    while(ap>(*a)->host && *ap!='.' && *ap!='/')
       ap--;
    while(bp>(*b)->host && *bp!='.' && *bp!='/')
       bp--;

    chosen=strcmp(ap,bp);
   }
 while(!chosen && ap>(*a)->host && bp>(*b)->host);

 /* Fallback to alphabetical */

 if(!chosen)
    chosen=sort_alpha(a,b);

 return(chosen);
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into time order.

  int sort_time Returns the comparison of the times.

  FileIndex **a The first FileIndex.

  FileIndex **b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_time(FileIndex **a,FileIndex **b)
{
 long at=(*a)->time;
 long bt=(*b)->time;
 long chosen;

 /* Compare the time stamps */

 chosen=bt-at;

 /* Fallback to alphabetical */

 if(!chosen)
    chosen=sort_alpha(a,b);

 return((int)chosen);
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into random order.

  int sort_random Returns the comparison of the random numbers generated from the file name.

  FileIndex **a The first FileIndex.

  FileIndex **b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_random(FileIndex **a,FileIndex **b)
{
 int ar=(*a)->random;
 int br=(*b)->random;
 int chosen;

 chosen=br-ar;

 return(chosen);
}
