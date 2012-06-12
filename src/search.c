/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/search.c 1.39 2005/09/04 15:56:20 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Handle the interface to the ht://Dig, mnoGoSearch (UdmSearch), Namazu and Hyper Estraier search engines.
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

#include <utime.h>
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

#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "proto.h"


/* Local functions */

static void SearchIndex(int fd,URL *Url);
static void SearchIndexRoot(int fd);
static void SearchIndexProtocol(int fd,char *proto);
static void SearchIndexHost(int fd,char *proto,char *host);
static void SearchIndexLastTime(int fd);

static void HTSearch(int fd,char *args);
static void mnoGoSearch(int fd,char *args);
static void Namazu(int fd,char *args);
static void EstSeek(int fd,char *args);

static void SearchScript(int fd,char *args,char *name,char *script,char *path);


/*++++++++++++++++++++++++++++++++++++++
  Create a page for one of the search pages on the local server.

  int fd The file descriptor to write the output to.

  URL *Url The URL that specifies the path for the page.

  Header *request_head The head of the request.

  Body *request_body The body of the request that was made for this page.
  ++++++++++++++++++++++++++++++++++++++*/

void SearchPage(int fd,URL *Url,Header *request_head,Body *request_body)
{
 if(!strncmp(Url->path+8,"index/",(size_t)6))
    SearchIndex(fd,Url);
 else if(!strcmp(Url->path+8,"htdig/htsearch"))
    HTSearch(fd,Url->args);
 else if(!strcmp(Url->path+8,"mnogosearch/mnogosearch"))
    mnoGoSearch(fd,Url->args);
 else if(!strcmp(Url->path+8,"udmsearch/udmsearch"))
    mnoGoSearch(fd,Url->args);
 else if(!strcmp(Url->path+8,"namazu/namazu"))
    Namazu(fd,Url->args);
 else if(!strcmp(Url->path+8,"hyperestraier/estseek"))
    EstSeek(fd,Url->args);
 else if(!strncmp(Url->path+8,"htdig/",(size_t)6) && !strchr(Url->path+8+6,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else if(!strncmp(Url->path+8,"mnogosearch/",(size_t)12) && !strchr(Url->path+8+12,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else if(!strncmp(Url->path+8,"udmsearch/",(size_t)10) && !strchr(Url->path+8+10,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else if(!strncmp(Url->path+8,"namazu/",(size_t)7) && !strchr(Url->path+8+7,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else if(!strncmp(Url->path+8,"hyperestraier/",(size_t)14) && !strchr(Url->path+8+14,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else if(!strchr(Url->path+8,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else
    HTMLMessage(fd,404,"WWWOFFLE Illegal Search Page",NULL,"SearchIllegal",
                "url",Url->path,
                NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Produce one of the indexes for htdig or mnoGoSearch (UdmSearch) to search.

  int fd The file descriptor to write to.

  URL *Url The URL that was requested.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndex(int fd,URL *Url)
{
 char *proto=(char*)malloc(strlen(Url->path)-10),*host="";
 int lasttime;
 int i;

 strcpy(proto,Url->path+8+6);

 lasttime=!strcmp(proto,"lasttime");

 if(*proto && proto[strlen(proto)-1]=='/')
    proto[strlen(proto)-1]=0;

 for(i=0;proto[i];i++)
    if(proto[i]=='/')
      {
       proto[i]=0;
       host=proto+i+1;
       break;
      }

 if(*proto)
   {
    for(i=0;i<NProtocols;i++)
       if(!strcmp(Protocols[i].name,proto))
          break;
    if(i==NProtocols)
       *proto=0;
   }

 if(!lasttime &&
    ((*host && (strchr(host,'/') || !strcmp(host,"..") || !strcmp(host,"."))) ||
     (*proto && (!strcmp(host,"..") || !strcmp(host,".")))||
     (*host && !*proto) || (Url->path[8+6] && !*proto)))
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->name,
                NULL);
 else
   {
    HTMLMessageHead(fd,200,"WWWOFFLE Search Index",
                    NULL);
    write_string(fd,"<html>\n"
                    "<head>\n"
                    "<meta name=\"robots\" content=\"noindex\">\n"
                    "<title></title>\n"
                    "</head>\n"
                    "<body>\n");

    if(lasttime)
       SearchIndexLastTime(fd);
    else if(!*host && !*proto)
       SearchIndexRoot(fd);
    else if(!*host)
       SearchIndexProtocol(fd,proto);
    else
       SearchIndexHost(fd,proto,host);

    write_string(fd,"</body>\n"
                    "</html>\n");
   }

 free(proto);
}


/*++++++++++++++++++++++++++++++++++++++
  Index the root of the cache.

  int fd The file descriptor to write to.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexRoot(int fd)
{
 int i;

 for(i=0;i<NProtocols;i++)
    write_formatted(fd,"<a href=\"%s/\"> </a>\n",Protocols[i].name);
}


/*++++++++++++++++++++++++++++++++++++++
  Index the hosts for one protocol in the cache.

  int fd The file descriptor to write to.

  char *proto The protocol to index.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexProtocol(int fd,char *proto)
{
 DIR *dir;
 struct dirent* ent;

 /* Open the spool directory. */

 if(chdir(proto))
    return;

 dir=opendir(".");
 if(!dir)
   {ChangeBackToSpoolDir();return;}

 ent=readdir(dir);
 if(!ent)
   {closedir(dir);ChangeBackToSpoolDir();return;}

 /* Output all of the host sub-directories. */

 do
   {
    struct stat buf;

    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(!stat(ent->d_name,&buf) && S_ISDIR(buf.st_mode))
       write_formatted(fd,"<a href=\"%s/\"> </a>\n",ent->d_name);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Create an index of the pages on a host.

  int fd The file descriptor to write to.

  char *proto The protocol to index.

  char *host The name of the subdirectory.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexHost(int fd,char *proto,char *host)
{
 DIR *dir;
 struct dirent* ent;

 /* Open the spool subdirectory. */

 if(chdir(proto))
    return;

 /* Open the spool subdirectory. */

 if(chdir(host))
   {ChangeBackToSpoolDir();return;}

 dir=opendir(".");
 if(!dir)
   {ChangeBackToSpoolDir();return;}

 ent=readdir(dir);
 if(!ent)
   {closedir(dir);ChangeBackToSpoolDir();return;}

 /* Output all of the file names. */

 do
   {
    URL *Url;

    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(*ent->d_name=='D' && (Url=FileNameToURL(ent->d_name)))
      {
       if(!strcmp(Url->proto,"http"))
          write_formatted(fd,"<a href=\"%s\"> </a>\n",Url->name);
       else
          write_formatted(fd,"<a href=\"/%s/%s\"> </a>\n",Url->proto,Url->hostp);

       FreeURL(Url);
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Create an index of the lasttime accessed pages.

  int fd The file descriptor to write to.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexLastTime(int fd)
{
 DIR *dir;
 struct dirent* ent;

 /* Open the spool subdirectory. */

 if(chdir("lasttime"))
    return;

 dir=opendir(".");
 if(!dir)
   {ChangeBackToSpoolDir();return;}

 ent=readdir(dir);
 if(!ent)
   {closedir(dir);ChangeBackToSpoolDir();return;}

 /* Output all of the file names. */

 do
   {
    URL *Url;

    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(*ent->d_name=='D' && (Url=FileNameToURL(ent->d_name)))
      {
       if(!strcmp(Url->proto,"http"))
          write_formatted(fd,"<a href=\"%s\"> </a>\n",Url->name);
       else
          write_formatted(fd,"<a href=\"/%s/%s\"> </a>\n",Url->proto,Url->hostp);

       FreeURL(Url);
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Perform an htdig search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static void HTSearch(int fd,char *args)
{
 SearchScript(fd,args,"htdig","htsearch","search/htdig/scripts/wwwoffle-htsearch");
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a mnogosearch (udmsearch) search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static void mnoGoSearch(int fd,char *args)
{
 SearchScript(fd,args,"mnogosearch","mnogosearch","search/mnogosearch/scripts/wwwoffle-mnogosearch");
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a Namazu search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static void Namazu(int fd,char *args)
{
 SearchScript(fd,args,"namazu","namazu","search/namazu/scripts/wwwoffle-namazu");
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a Hyper Estraier search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static void EstSeek(int fd,char *args)
{
 SearchScript(fd,args,"hyperestraier","estseek","search/hyperestraier/scripts/wwwoffle-estseek");
}


/*++++++++++++++++++++++++++++++++++++++
  A macro definition that makes environment variable setting a little easier.

  variable The environment variable that is to be set.

  value The value of the environment variable.
  ++++++++++++++++++++++++++++++++++++++*/

#define putenv_var_val(variable,value) \
{ \
  char *envstr=(char *)malloc(sizeof(variable "=")+strlen(value)); \
  strcpy(envstr,variable "="); \
  strcpy(envstr+sizeof(variable),value); \
  if(putenv(envstr)==-1) \
     env_err=1; \
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a search using one of the three search methods.

  int fd The file descriptor to write to.

  char *args The arguments of the request.

  char *name The name of the search method.

  char* script The name of the script.

  char* path The path to the script to execute.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchScript(int fd,char *args,char *name,char *script,char *path)
{
 pid_t pid;

 if((pid=fork())==-1)
   {
    PrintMessage(Warning,"Cannot fork to call %s search script; [%!s].",name);

    lseek(fd,(off_t)0,SEEK_SET);
    ftruncate(fd,(off_t)0);
    reinit_io(fd);

    HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
                "error","Problem running search program.",
                NULL);
   }
 else if(pid) /* parent */
   {
    int status;

    wait(&status);

    if(WIFEXITED(status) && WEXITSTATUS(status)==0)
       return;
   }
 else /* child */
   {
    int cgi_fd;
    int env_err=0;

    /* Set up stdout (fd is the only file descriptor we *must* keep). */

    if(fd!=STDOUT_FILENO)
      {
       if(dup2(fd,STDOUT_FILENO)==-1)
          PrintMessage(Fatal,"Cannnot create standard output for %s search script [%!s].",name);
       finish_io(fd);
       close(fd);
       init_io(STDOUT_FILENO);
      }

    /* Set up stdin and stderr. */

    cgi_fd=open("/dev/null",O_RDONLY);
    if(cgi_fd!=STDIN_FILENO)
      {
       if(dup2(cgi_fd,STDIN_FILENO)==-1)
          PrintMessage(Fatal,"Cannnot create standard input for %s search script [%!s].",name);
       close(cgi_fd);
      }

    if(lseek(STDERR_FILENO,(off_t)0,SEEK_CUR)==-1 && errno==EBADF) /* stderr is not open */
      {
       cgi_fd=open("/dev/null",O_WRONLY);

       if(cgi_fd!=STDERR_FILENO)
         {
          if(dup2(cgi_fd,STDERR_FILENO)==-1)
             PrintMessage(Fatal,"Cannnot create standard error for %s search script [%!s].",name);
          close(cgi_fd);
         }
      }

    write_formatted(STDOUT_FILENO,"HTTP/1.0 200 %s search output\r\n",name);

    /* Set up the environment. */

    /*@-mustfreefresh@*/

    putenv_var_val("REQUEST_METHOD","GET");

    if(args)
       putenv_var_val("QUERY_STRING",args)
    else
       putenv("QUERY_STRING=");

    putenv_var_val("SCRIPT_NAME",script);

    if(env_err)
       PrintMessage(Fatal,"Failed to create environment for %s search script [%!s].",name);

    /*@=mustfreefresh@*/

    execl(path,path,NULL);
    PrintMessage(Warning,"Cannot exec %s search script %s [%!s]",name,path);
    exit(1);
   }
}
