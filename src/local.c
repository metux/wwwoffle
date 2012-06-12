/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/local.c 1.16 2006/07/21 17:38:50 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Serve the local web-pages and handle the language selection.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
#include <fcntl.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"


#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif


/* Local functions */

static char /*@null@*/ /*@observer@*/ **get_languages(int *ndirs);
static char /*@null@*/ *find_language_file(char* search);


/* Local variables */

/*+ The language header that the client sent. +*/
static char /*@null@*/ /*@only@*/ *accept_language=NULL;


/*++++++++++++++++++++++++++++++++++++++
  Output a local page.

  int fd The file descriptor to write to.

  URL *Url The URL for the local page.

  Header *request_head The request that was made for this page.

  Body *request_body The body of the request that was made for this page.
  ++++++++++++++++++++++++++++++++++++++*/

void LocalPage(int fd,URL *Url,Header *request_head,Body *request_body)
{
 int found=0;
 char *file,*path;

 /* Don't allow paths backwards */

 if(strstr(Url->path,"/../"))
   {
    PrintMessage(Warning,"Illegal path containing '/../' for the local page '%s'.",Url->path);
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->path,
                NULL);
    return;
   }

 /* Get the filename */

 path=URLDecodeGeneric(Url->path+1);

 if((file=find_language_file(path)))
   {
    struct stat buf;

    if(stat(file,&buf))
       ;
    else if(S_ISREG(buf.st_mode) && buf.st_mode&S_IROTH)
      {
       if(buf.st_mode&S_IXOTH && IsCGIAllowed(Url->path))
         {
          LocalCGI(fd,Url,file,request_head,request_body);
          found=1;
         }
       else
         {
          int htmlfd=open(file,O_RDONLY|O_BINARY);

          if(htmlfd==-1)
             PrintMessage(Warning,"Cannot open the local page '%s' [%!s].",file);
          else
            {
             char *ims=NULL;
             time_t since=0;

             init_io(htmlfd);

             PrintMessage(Debug,"Using the local page '%s'.",file);

             if((ims=GetHeader(request_head,"If-Modified-Since")))
                since=DateToTimeT(ims);

             if(since>=buf.st_mtime)
                HTMLMessageHead(fd,304,"WWWOFFLE Not Modified",
                                NULL);
             else
               {
                char buffer[IO_BUFFER_SIZE];
                int n;

                HTMLMessageHead(fd,200,"WWWOFFLE Local OK",
                                "Last-Modified",RFC822Date(buf.st_mtime,1),
                                "Content-Type",WhatMIMEType(file),
                                NULL);

                while((n=read_data(htmlfd,buffer,IO_BUFFER_SIZE))>0)
                   write_data(fd,buffer,n);
               }

             finish_io(htmlfd);
             close(htmlfd);

             found=1;
            }
         }
      }
    else if(S_ISDIR(buf.st_mode))
      {
       char *localurl=GetLocalURL();
       char *dir=(char*)malloc(strlen(Url->path)+strlen(localurl)+16);

       PrintMessage(Debug,"Using the local directory '%s'.",file);

       strcpy(dir,localurl);
       strcat(dir,Url->path);
       if(dir[strlen(dir)-1]!='/')
          strcat(dir,"/");
       strcat(dir,"index.html");
       HTMLMessage(fd,302,"WWWOFFLE Local Dir Redirect",dir,"Redirect",
                   "location",dir,
                   NULL);

       free(dir);
       free(localurl);

       found=1;
      }
    else
       PrintMessage(Warning,"Not a regular file or wrong permissions for the local page '%s'.",file);

    free(file);
   }

 free(path);

 if(!found)
   {
    PrintMessage(Warning,"Cannot find a local URL '%s'.",Url->path);
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->path,
                NULL);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Set the language that will be accepted for the mesages.

  char *accept The contents of the Accept-Language header.
  ++++++++++++++++++++++++++++++++++++++*/

void SetLanguage(char *accept)
{
 if(accept)
   {
    accept_language=(char*)malloc(strlen(accept)+1);
    strcpy(accept_language,accept);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Find the language specific message file.

  char *find_language_file Returns the file name or NULL.

  char* search The name of the file to search for (e.g. 'messages/foo.html' or 'local/bar.html').
  ++++++++++++++++++++++++++++++++++++++*/

static char *find_language_file(char* search)
{
 char *file=NULL;
 int dirn=0;
 char **dirs;
 int ndirs=0;

 /* Get the list of directories for languages. */

 dirs=get_languages(&ndirs);

 /* Find the file */

 if(!strncmp(search,"local/",(size_t)6))
    dirn=-1;

 while(dirn<(ndirs+2))
   {
    struct stat buf;
    char *dir="",*tryfile;

    if(dirn==-1)
       dir="./";                /* must include "local" at the start. */
    else if(dirn<ndirs)
       dir=dirs[dirn];          /* is formatted like "html/$LANG/" */
    else if(dirn==ndirs)
       dir="html/default/";
    else if(dirn==(ndirs+1))
       dir="html/en/";

    dirn++;

    tryfile=(char*)malloc(strlen(dir)+strlen(search)+1);
    strcpy(tryfile,dir);
    strcat(tryfile,search);

    if(!stat(tryfile,&buf))
      {
       file=tryfile;
       break;
      }

    free(tryfile);
   }

 return(file);
}


/*++++++++++++++++++++++++++++++++++++++
  Open the language specific message file.

  int OpenLanguageFile Returns the file descriptor or -1.

  char* search The name of the file to search for (e.g. 'messages/foo.html' or 'local/bar.html').
  ++++++++++++++++++++++++++++++++++++++*/

int OpenLanguageFile(char* search)
{
 int fd=-1;
 char *file;

 /* Find the file. */

 file=find_language_file(search);

 if(file)
   {
    fd=open(file,O_RDONLY|O_BINARY);

    free(file);
   }

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the language string and add the directories to the list.

  char **get_languages Returns the list of directories to try.

  int *ndirs Returns the number of directories in the list.
  ++++++++++++++++++++++++++++++++++++++*/

static char **get_languages(int *ndirs)
{
 static char **dirs=NULL;
 static int n_dirs=0;
 static int first=1;

 if(first)
   {
    first=0;

    if(accept_language)
      {
       int i;
       HeaderList *list=SplitHeaderList(accept_language);

       for(i=0;i<list->n;i++)
          if(list->item[i].qval>0)
            {
             char *p,*q;

             if(!isalpha(*list->item[i].val) || strchr(list->item[i].val,'/'))
                continue;

             if((n_dirs%8)==0)
                dirs=(char**)realloc((void*)dirs,(8+n_dirs)*sizeof(char*));
             dirs[n_dirs]=(char*)malloc(8+strlen(list->item[i].val)+1);

             strcpy(dirs[n_dirs],"html/");
             p=dirs[n_dirs]+sizeof("html");
             q=list->item[i].val;
             while(isalpha(*q))
               {*p++=tolower(*q); q++;}
             *p++='/';
             *p=0;

             n_dirs++;
            }

       FreeHeaderList(list);
      }
   }

 *ndirs=n_dirs;

 return(dirs);
}
