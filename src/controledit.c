/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/controledit.c,v 2.36 2007-12-05 18:50:34 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9d.
  Configuration file management via a web-page.
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
#include <ctype.h>

#include <sys/stat.h>
#include <unistd.h>
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


/*+ A type definition to contain the contents of a configuration file section. +*/

typedef struct _ControlEditSection
{
 /*@only@*/ char *comment;                 /*+ The comment outside the section. +*/
 /*@only@*/ char *name;                    /*+ The name of the section. +*/
 /*@only@*/ char *file;                    /*+ The filename of an included section. +*/
 /*@only@*/ char *content;                 /*+ The content of the section. +*/
}
*ControlEditSection;


/* Local functions */

static void ControlEditForms(int fd,ControlEditSection *sections);
static void ControlEditUpdate(int fd,char *section,ControlEditSection *sections);

static /*@null@*/ /*@only@*/ ControlEditSection *read_config_file(void);
static int write_config_file(ControlEditSection *sections);
static void free_sections(/*@only@*/ ControlEditSection *sections);


/*++++++++++++++++++++++++++++++++++++++
  The control page that allows editing of the configuration file.

  int fd The file descriptor to write the file to.

  char *request_args The arguments to the page.

  Body *request_body The body of the HTTP request for the page.
  ++++++++++++++++++++++++++++++++++++++*/

void ControlEditPage(int fd,char *request_args,Body *request_body)
{
 char *newargs=NULL;
 ControlEditSection *sections;

 if(request_args)
   {
    if(*request_args=='!' && strchr(request_args+1,'!'))
      {
       char *pling;
       newargs=(char*)malloc(strlen(request_args)+1);
       strcpy(newargs,request_args+1);
       pling=strchr(newargs,'!');
       *pling=0;
      }
    else if(*request_args!='!')
      {
       newargs=(char*)malloc(strlen(request_args)+1);
       strcpy(newargs,request_args);
      }
   }

 sections=read_config_file();

 if(!sections)
    HTMLMessage(fd,404,"WWWOFFLE Configuration Error",NULL,"ControlEditError",
                "section",NULL,
                "reason","ReadError",
                NULL);
 else if(newargs && *newargs)
   {
    int i=0;
    char *section=NULL;

    while(sections[i])
      {
       if(sections[i]->name && !strcmp(sections[i]->name,newargs))
         {section=newargs;break;}
       i++;
      }

    if(!section)
       HTMLMessage(fd,404,"WWWOFFLE Configuration Error",NULL,"ControlEditError",
                   "section",newargs,
                   "reason","BadSection",
                   NULL);
    else if(!request_body || strncmp(request_body->content,"value=",(size_t)6))
       HTMLMessage(fd,404,"WWWOFFLE Configuration Error",NULL,"ControlEditError",
                   "section",newargs,
                   "reason","BadBody",
                   NULL);
    else
      {
       char *old=sections[i]->content;
       char *new=URLDecodeFormArgs(request_body->content+6);

#if !defined(__CYGWIN__)
       char *p,*q;

       for(p=q=new;*p;p++)
          if(*p!='\r')
             *q++=*p;
       *q=0;
#endif

       sections[i]->content=new;

       ControlEditUpdate(fd,section,sections);

       free(old);
      }
   }
 else
    ControlEditForms(fd,sections);

 if(sections)
    free_sections(sections);

 if(newargs)
    free(newargs);
}


/*++++++++++++++++++++++++++++++++++++++
  The page that contains the forms making up the config file.

  int fd The file descriptor to write to.

  ControlEditSection *sections The sections of the file.
  ++++++++++++++++++++++++++++++++++++++*/

static void ControlEditForms(int fd,ControlEditSection *sections)
{
 int i=0;

 HTMLMessageHead(fd,200,"WWWOFFLE Configuration Edit Page",
                 NULL);

 HTMLMessageBody(fd,"ControlEditPage-Head",
                 NULL);

 while(sections[i])
   {
    char *htmlcomment=NULL,*htmlcontent=NULL;

    if(sections[i]->comment)
       htmlcomment=HTMLString(sections[i]->comment,0);
    if(sections[i]->content)
       htmlcontent=HTMLString(sections[i]->content,0);

    HTMLMessageBody(fd,"ControlEditPage-Body",
                    "section",sections[i]->name,
                    "comment",htmlcomment,
                    "content",htmlcontent,
                    NULL);

    if(htmlcomment)
       free(htmlcomment);
    if(htmlcontent)
       free(htmlcontent);

    i++;
   }

 HTMLMessageBody(fd,"ControlEditPage-Tail",
                 NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Update the configuration file.

  int fd The file descriptor to write the message to.

  char *section The section that was updated.

  ControlEditSection *sections The sections including the updated one.
  ++++++++++++++++++++++++++++++++++++++*/

static void ControlEditUpdate(int fd,char *section,ControlEditSection *sections)
{
 if(write_config_file(sections))
    HTMLMessage(fd,404,"WWWOFFLE Configuration Error",NULL,"ControlEditError",
                "section",section,
                "reason","WriteError",
                NULL);
 else
    HTMLMessage(fd,200,"WWWOFFLE Configuration Update",NULL,"ControlEditUpdate",
                "section",section,
                NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Read in the config file into a set of sections.

  ControlEditSection *read_config_file Returns the sections of the file as a NULL terminated list.
  ++++++++++++++++++++++++++++++++++++++*/

static ControlEditSection *read_config_file(void)
{
 int sec_num=0,state=0;
 int conf;
 ControlEditSection *sections;
 char *line=NULL;
 int line_num=0;

 conf=open(ConfigurationFileName(),O_RDONLY|O_BINARY);

 if(conf==-1)
   {PrintMessage(Warning,"Cannot open the config file '%s' for reading; [%!s].",ConfigurationFileName()); return(NULL);}

 init_io(conf);

 sections=(ControlEditSection*)calloc((size_t)1,sizeof(ControlEditSection));

 while((line=read_line(conf,line)))
   {
    char *l=line;
    char *r=line+strlen(line)-1;

    line_num++;

    while(isspace(*l))
       l++;

    if(state==0 && *l=='#')
      {
       state=1;
       sections=(ControlEditSection*)realloc((void*)sections,sizeof(ControlEditSection)*(sec_num+2));
       sections[sec_num]=(ControlEditSection)calloc((size_t)1,sizeof(struct _ControlEditSection));
       sections[++sec_num]=NULL;
       sections[sec_num-1]->comment=(char*)malloc(strlen(l)+1);
       strcpy(sections[sec_num-1]->comment,l);
      }
    else if((state==1 || state==2) && *l=='#')
      {
       sections[sec_num-1]->comment=(char*)realloc((void*)sections[sec_num-1]->comment,strlen(sections[sec_num-1]->comment)+strlen(l)+1);
       strcat(sections[sec_num-1]->comment,l);
      }
    else if(state==0 && !*l)
      ;
    else if(state==1 && !*l)
       state=0;
    else if((state==0 || state==1) && *l)
      {
       state=2;
       while(r>l && isspace(*r))
          *r--=0;
       if(sec_num==0 || sections[sec_num-1]->name)
         {
          sections=(ControlEditSection*)realloc((void*)sections,sizeof(ControlEditSection)*(sec_num+2));
          sections[sec_num]=(ControlEditSection)calloc((size_t)1,sizeof(struct _ControlEditSection));
          sections[++sec_num]=NULL;
         }
       sections[sec_num-1]->name=(char*)malloc(strlen(l)+1);
       strcpy(sections[sec_num-1]->name,l);
      }
    else if(state==2 && !*l)
      ;
    else if(state==2 && *l=='{')
      {
       state=3;
       sections[sec_num-1]->content=(char*)malloc((size_t)1);
       strcpy(sections[sec_num-1]->content,"");
      }
    else if(state==2 && *l=='[')
      {
       state=4;
      }
    else if(state==3 && *l=='}')
       state=0;
    else if(state==3)
      {
       sections[sec_num-1]->content=(char*)realloc((void*)sections[sec_num-1]->content,strlen(sections[sec_num-1]->content)+strlen(line)+1);
       strcat(sections[sec_num-1]->content,line);
      }
    else if(state==4 && *l)
      {
       state=5;
       if(strchr(l,'/'))
         {
          PrintMessage(Warning,"Error parsing config file, line %d, included file is not in same directory",line_num);
          free_sections(sections);
          free(line);
          return(NULL);
         }
       while(r>l && isspace(*r))
          *r--=0;
       sections[sec_num-1]->file=(char*)malloc(strlen(l)+1);
       strcpy(sections[sec_num-1]->file,l);
      }
    else if(state==5 && *l==']')
       state=0;
    else
      {
       line[strlen(line)-1]=0;
       PrintMessage(Warning,"Error parsing config file, line %d = '%s' [state=%d]",line_num,line,state);
       free_sections(sections);
       free(line);
       return(NULL);
      }
   }

 finish_io(conf);
 close(conf);

 for(sec_num=0;sections[sec_num];sec_num++)
    if(sections[sec_num]->name && sections[sec_num]->file)
      {
       char *name,*r,*old;

       sections[sec_num]->content=(char*)malloc((size_t)1);
       strcpy(sections[sec_num]->content,"");

       name=(char*)malloc(strlen(ConfigurationFileName())+strlen(sections[sec_num]->file)+1);

       strcpy(name,ConfigurationFileName());

       r=name+strlen(name)-1;
       while(r>name && *r!='/')
          r--;

       strcpy(r+1,sections[sec_num]->file);

       conf=open(name,O_RDONLY|O_BINARY);

       if(conf==-1)
         {PrintMessage(Warning,"Cannot open the config file '%s' for reading; [%!s].",name); free_sections(sections); free(name); return(NULL);}

       init_io(conf);

       old=sections[sec_num]->file;
       sections[sec_num]->file=name;
       free(old);

       while((line=read_line(conf,line)))
         {
          sections[sec_num]->content=(char*)realloc((void*)sections[sec_num]->content,strlen(sections[sec_num]->content)+strlen(line)+1);
          strcat(sections[sec_num]->content,line);
         }

       finish_io(conf);
       close(conf);
      }

 return(sections);
}


/*++++++++++++++++++++++++++++++++++++++
  Write out a set of sections to the config file.

  int write_config_file Returns 1 if in error.

  ControlEditSection *sections The sections to write out.
  ++++++++++++++++++++++++++++++++++++++*/

static int write_config_file(ControlEditSection *sections)
{
 char *conf_file_backup;
 char *conf_file=ConfigurationFileName();
 int renamed=0,i;
 struct stat buf;
 int conf;

 /* Rename the old file as a backup. */

 conf_file_backup=(char*)malloc(strlen(conf_file)+5);
 strcpy(conf_file_backup,conf_file);
 strcat(conf_file_backup,".bak");

 if(rename(conf_file,conf_file_backup))
    PrintMessage(Warning,"Cannot rename the config file '%s' to '%s'; [%!s].",conf_file,conf_file_backup);
 else if(stat(conf_file_backup,&buf))
    PrintMessage(Warning,"Cannot stat the config file '%s'; [%!s].",conf_file);
 else
    renamed=1;

 free(conf_file_backup);

 conf=open(conf_file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0600);

 if(conf==-1)
   {PrintMessage(Warning,"Cannot open the config file '%s' for writing; [%!s].",conf_file); return(1);}

 init_io(conf);

 if(renamed)
   {
    chown(conf_file,buf.st_uid,buf.st_gid);
    chmod(conf_file,buf.st_mode&(~S_IFMT));
   }

 for(i=0;sections[i];i++)
   {
    if(sections[i]->comment)
      {
       write_formatted(conf,"%s\n",sections[i]->comment);
      }
    if(sections[i]->name)
      {
       write_formatted(conf,"%s\n",sections[i]->name);

       if(sections[i]->file)
         {
          char *p=sections[i]->file+strlen(sections[i]->file)-1;
          while(p>sections[i]->file && *p!='/')
             p--;
          write_formatted(conf,"[\n%s\n]\n\n\n",p+1);
         }
       else
         {
          write_formatted(conf,"{\n");
          if(sections[i]->content)
             write_string(conf,sections[i]->content);
          if(sections[i]->content[strlen(sections[i]->content)-1]!='\n')
             write_string(conf,"\n");
          write_string(conf,"}\n\n\n");
         }
      }
   }

 finish_io(conf);
 close(conf);

 for(i=0;sections[i];i++)
    if(sections[i]->name && sections[i]->file)
      {
       conf_file_backup=(char*)malloc(strlen(sections[i]->file)+5);
       strcpy(conf_file_backup,sections[i]->file);
       strcat(conf_file_backup,".bak");

       if(rename(sections[i]->file,conf_file_backup))
          PrintMessage(Warning,"Cannot rename the config file '%s' to '%s'; [%!s].",sections[i]->file,conf_file_backup);
       else if(stat(conf_file_backup,&buf))
          PrintMessage(Warning,"Cannot stat the config file '%s'; [%!s].",sections[i]->file);
       else
          renamed=1;

       free(conf_file_backup);

       conf=open(sections[i]->file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0600);

       if(conf==-1)
         {PrintMessage(Warning,"Cannot open the config file '%s' for writing; [%!s].",sections[i]->file); return(1);}

       init_io(conf);

       if(renamed)
         {
          chown(sections[i]->file,buf.st_uid,buf.st_gid);
          chmod(sections[i]->file,buf.st_mode&(~S_IFMT));
         }

       write_string(conf,sections[i]->content);

       finish_io(conf);
       close(conf);
      }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Free up a set of sections.

  ControlEditSection *sections The sections that are to be freed up.
  ++++++++++++++++++++++++++++++++++++++*/

static void free_sections(ControlEditSection *sections)
{
 int i=0;

 while(sections[i])
   {
    if(sections[i]->comment)
       free(sections[i]->comment);
    if(sections[i]->name)
       free(sections[i]->name);
    if(sections[i]->file)
       free(sections[i]->file);
    if(sections[i]->content)
       free(sections[i]->content);
    free(sections[i]);
    i++;
   }

 free(sections);
}
