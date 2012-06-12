/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configedit.c 1.41 2005/10/11 18:34:15 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  The HTML interactive configuration editing pages.
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

#include <unistd.h>

#include "wwwoffle.h"
#include "errors.h"
#include "io.h"
#include "misc.h"
#include "configpriv.h"
#include "config.h"


static void ConfigurationIndexPage(int fd,/*@null@*/ char *url);
static void ConfigurationSectionPage(int fd,int section,/*@null@*/ char *url);
static void ConfigurationItemPage(int fd,int section,int item,URL *Url,/*@null@*/ char *url,/*@null@*/ Body *request_body);
static void ConfigurationEditURLPage(int fd,URL *Url,/*@null@*/ Body *request_body);
static void ConfigurationURLPage(int fd,char *url);
static void ConfigurationAuthFail(int fd,char *url);


/*+ The amount to rewind a file before starting to read forward again. +*/
#define REWIND_STEP 2048


/*++++++++++++++++++++++++++++++++++++++
  Send to the client one of the pages to configure WWWOFFLE using HTML.

  int fd The file descriptor of the client.

  URL *Url The Url that was requested.

  Body *request_body The body of the HTTP request.
  ++++++++++++++++++++++++++++++++++++++*/

void ConfigurationPage(int fd,URL *Url,Body *request_body)
{
 int s,i;
 char *url=NULL;

 /* Check the authorisation. */

 if(ConfigString(PassWord))
   {
    if(!Url->pass)
      {
       ConfigurationAuthFail(fd,Url->pathp);
       return;
      }
    else if(strcmp(Url->pass,ConfigString(PassWord)))
      {
       ConfigurationAuthFail(fd,Url->pathp);
       return;
      }
   }

 /* Extract the URL argument if any (ignoring all posted forms) */

 if(Url->args && *Url->args!='!')
    url=URLDecodeFormArgs(Url->args);

 /* Determine the page to show. */

 if(Url->path[15]==0)
   {
    ConfigurationIndexPage(fd,url);
    if(url) free(url);
    return;
   }

 if(!strcmp(&Url->path[15],"editurl"))
   {
    ConfigurationEditURLPage(fd,Url,request_body);
    if(url) free(url);
    return;
   }

 if(!strcmp(&Url->path[15],"url") && url)
   {
    ConfigurationURLPage(fd,url);
    if(url) free(url);
    return;
   }

 for(s=0;s<CurrentConfig.nsections;s++)
   {
    size_t len=strlen(CurrentConfig.sections[s]->name);

    if(!strncmp(CurrentConfig.sections[s]->name,Url->path+15,len))
      {
       if(*(Url->path+15+len)==0)
         {
          ConfigurationSectionPage(fd,s,url);
          if(url) free(url);
          return;
         }
       else if(*(Url->path+15+len)=='/')
          for(i=0;i<CurrentConfig.sections[s]->nitemdefs;i++)
             if(!strcmp(CurrentConfig.sections[s]->itemdefs[i].name,Url->path+16+len) ||
                (!strcmp(Url->path+16+len,"no-name") && *CurrentConfig.sections[s]->itemdefs[i].name==0))
               {
                ConfigurationItemPage(fd,s,i,Url,url,request_body);
                if(url) free(url);
                return;
               }
      }
   }

 HTMLMessage(fd,404,"WWWOFFLE Illegal Configuration Page",NULL,"ConfigurationIllegal",
             "url",Url->pathp,
             NULL);
 if(url) free(url);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page index that lists the sections.

  int fd The file descriptor to write to.

  char *url The URL specification from the URL argument.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationIndexPage(int fd,char *url)
{
 char *line=NULL;
 int file;
 off_t seekpos=0;
 int s;

 file=OpenLanguageFile("messages/README.CONF.txt");

 if(file==-1)
   {
    HTMLMessage(fd,500,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                "error","Cannot open README.CONF.txt",
                NULL);
    return;
   }

 init_io(file);

 line=read_line(file,line); /* TITLE ... */
 line=read_line(file,line); /* HEAD */
 line=read_line(file,line); /* comment */

 HTMLMessageHead(fd,200,"WWWOFFLE Configuration Page",
                 NULL);
 HTMLMessageBody(fd,"ConfigurationPage-Head",
                 "description",line,
                 NULL);

 for(s=0;s<CurrentConfig.nsections;s++)
   {
    char *description=NULL;

    lseek(file,(off_t)0,SEEK_SET);
    reinit_io(file);

    while((line=read_line(file,line)))
      {
       line[strlen(line)-1]=0;

       if(!strncmp(line,"SECTION",(size_t)7) && !strcmp(line+8,CurrentConfig.sections[s]->name))
         {
          line=read_line(file,line);
          if(line)
             line[strlen(line)-1]=0;
          description=line;
          seekpos=lseek(file,(off_t)0,SEEK_CUR);
          if(seekpos>REWIND_STEP)
             seekpos-=REWIND_STEP;
          else
             seekpos=0;
          break;
         }
      }

    HTMLMessageBody(fd,"ConfigurationPage-Body",
                    "section",CurrentConfig.sections[s]->name,
                    "description",description,
                    "url",url,
                    NULL);
   }

 lseek(file,seekpos,SEEK_SET);  /* go back only to the start of the last section */
 reinit_io(file);

 while((line=read_line(file,line)))
   {
    if(!strncmp(line,"TAIL",(size_t)4))
       break;
   }

 line=read_line(file,line); /* comment */

 HTMLMessageBody(fd,"ConfigurationPage-Tail",
                 "description",line,
                 NULL);

 if(line)
    free(line);

 finish_io(file);
 close(file);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page that handles a section.

  int fd The file descriptor to write to.

  int section The section in the configuration file.

  char *url The URL specification from the URL argument.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationSectionPage(int fd,int section,char *url)
{
 int i;
 char *line1=NULL,*line2=NULL,*description=NULL;
 int file;
 off_t seekpos=0;

 file=OpenLanguageFile("messages/README.CONF.txt");

 if(file==-1)
   {
    HTMLMessage(fd,500,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                "error","Cannot open README.CONF.txt",
                NULL);
    return;
   }

 init_io(file);

 HTMLMessageHead(fd,200,"WWWOFFLE Configuration Section Page",
                 NULL);

 while((line1=read_line(file,line1)))
   {
    line1[strlen(line1)-1]=0;

    if(!strncmp(line1,"SECTION",(size_t)7) && !strcmp(line1+8,CurrentConfig.sections[section]->name))
      {
       line1=read_line(file,line1);
       if(line1)
          line1[strlen(line1)-1]=0;
       description=line1;
       seekpos=lseek(file,(off_t)0,SEEK_CUR);
       if(seekpos>REWIND_STEP)
          seekpos-=REWIND_STEP;
       else
          seekpos=0;
       break;
      }
   }

 HTMLMessageBody(fd,"ConfigurationSection-Head",
                 "section",CurrentConfig.sections[section]->name,
                 "description",description,
                 "nextsection",section==CurrentConfig.nsections-1?
                               CurrentConfig.sections[0]->name:
                               CurrentConfig.sections[section+1]->name,
                 "prevsection",section==0?
                               CurrentConfig.sections[CurrentConfig.nsections-1]->name:
                               CurrentConfig.sections[section-1]->name,
                 "url",url,
                 NULL);

 for(i=0;i<CurrentConfig.sections[section]->nitemdefs;i++)
   {
    char *template=NULL;
    description=NULL;

    lseek(file,seekpos,SEEK_SET);  /* go back only to the start of the required section */
    reinit_io(file);

    while((line1=read_line(file,line1)))
      {
       line1[strlen(line1)-1]=0;

       if(!strncmp(line1,"SECTION",(size_t)7) && !strcmp(line1+8,CurrentConfig.sections[section]->name))
          break;
      }

    while((line1=read_line(file,line1)))
      {
       line1[strlen(line1)-1]=0;

       if(!strncmp(line1,"SECTION",(size_t)7))
          break;
       if(!strncmp(line1,"ITEM",(size_t)4) && !strcmp(line1+5,CurrentConfig.sections[section]->itemdefs[i].name))
         {
          line1=read_line(file,line1);
          if(line1)
             line1[strlen(line1)-1]=0;
          line2=read_line(file,line2);
          if(line2)
             line2[strlen(line2)-1]=0;
          template=line1;
          description=line2;
          break;
         }
      }

    HTMLMessageBody(fd,"ConfigurationSection-Body",
                    "item",CurrentConfig.sections[section]->itemdefs[i].name,
                    "template",template,
                    "description",description,
                    NULL);
   }

 HTMLMessageBody(fd,"ConfigurationSection-Tail",
                 NULL);

 if(line1)
    free(line1);
 if(line2)
    free(line2);

 finish_io(file);
 close(file);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page that handles an item in a section.

  int fd The file descriptor to write to.

  int section The section in the configuration file.

  int item The item within the section.

  URL *Url The URL of the page that is being requested.

  char *url The URL specification from the URL argument.

  Body *request_body The body of the POST request containing the information.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationItemPage(int fd,int section,int item,URL *Url,char *url,Body *request_body)
{
 char *action=NULL,*entry=NULL;
 char *key=NULL,*val=NULL;
 int i;

 /* Parse the arguments if any */

 if(request_body)
   {
    char *form=URLRecodeFormArgs(request_body->content);
    char **args=SplitFormArgs(form);

    for(i=0;args[i];i++)
      {
       if(!strncmp("url=",args[i],(size_t)4) && args[i][4])
          url=TrimArgs(URLDecodeFormArgs(args[i]+4));
       else if(!strncmp("key=",args[i],(size_t)4) && args[i][4])
          key=TrimArgs(URLDecodeFormArgs(args[i]+4));
       else if(!strncmp("val=",args[i],(size_t)4) && args[i][4])
          val=TrimArgs(URLDecodeFormArgs(args[i]+4));
       else if(!strncmp("action=",args[i],(size_t)7))
          action=TrimArgs(URLDecodeFormArgs(args[i]+7));
       else if(!strncmp("entry=",args[i],(size_t)6))
          entry=TrimArgs(URLDecodeFormArgs(args[i]+6));
       else
          PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",args[i],Url->name);
      }

    free(args[0]);
    free(args);
    free(form);
   }

 /* Display the page to edit the parameters */

 if(!action || !strcmp(action,"edit"))
   {
    char *line1=NULL,*line2=NULL,*template=NULL,*description=NULL;
    int file;
    char nentries[MAX_INT_STR+1],nallowed[8];
    char *entry_url=NULL,*entry_key=NULL,*entry_val=NULL;
    int edit=0;

    if(action && entry)
      {
       edit=1;

       for(i=0;i<(*CurrentConfig.sections[section]->itemdefs[item].item)->nentries;i++)
         {
          char *thisentry=ConfigEntryString(*CurrentConfig.sections[section]->itemdefs[item].item,i);

          if(!strcmp(thisentry,entry))
            {
             ConfigEntryStrings(*CurrentConfig.sections[section]->itemdefs[item].item,i,&entry_url,&entry_key,&entry_val);
             free(thisentry);
             break;
            }

          free(thisentry);
         }
      }

    /* Display the HTML */

    file=OpenLanguageFile("messages/README.CONF.txt");

    if(file==-1)
      {
       HTMLMessage(fd,500,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                   "error","Cannot open README.CONF.txt",
                   NULL);

       if(key)    free(key);
       if(val)    free(val);
       if(action) free(action);
       if(entry)  free(entry);
       return;
      }

    init_io(file);

    HTMLMessageHead(fd,200,"WWWOFFLE Configuration Item Page",
                    "Cache-Control","no-cache",
                    "Expires","0",
                    NULL);

    while((line1=read_line(file,line1)))
      {
       line1[strlen(line1)-1]=0;

       if(!strncmp(line1,"SECTION",(size_t)7) && !strcmp(line1+8,CurrentConfig.sections[section]->name))
          break;
      }

    while((line1=read_line(file,line1)))
      {
       line1[strlen(line1)-1]=0;

       if(!strncmp(line1,"SECTION",(size_t)7))
          break;
       if(!strncmp(line1,"ITEM",(size_t)4) && !strcmp(line1+5,CurrentConfig.sections[section]->itemdefs[item].name))
         {
          line1=read_line(file,line1);
          if(line1)
             line1[strlen(line1)-1]=0;
          line2=read_line(file,line2);
          if(line2)
             line2[strlen(line2)-1]=0;
          template=line1;
          description=line2;
          break;
         }
      }

    if(*CurrentConfig.sections[section]->itemdefs[item].item)
       sprintf(nentries,"%d",(*CurrentConfig.sections[section]->itemdefs[item].item)->nentries);
    else
       strcpy(nentries,"0");

    if(CurrentConfig.sections[section]->itemdefs[item].same_key==0 &&
       CurrentConfig.sections[section]->itemdefs[item].url_type==0)
       strcpy(nallowed,"1");
    else
       strcpy(nallowed,"any");

    HTMLMessageBody(fd,"ConfigurationItem-Head",
                    "section",CurrentConfig.sections[section]->name,
                    "item",CurrentConfig.sections[section]->itemdefs[item].name,
                    "template",template,
                    "description",description,
                    "nextitem",item==CurrentConfig.sections[section]->nitemdefs-1?
                               CurrentConfig.sections[section]->itemdefs[0].name:
                               CurrentConfig.sections[section]->itemdefs[item+1].name,
                    "previtem",item==0?
                               CurrentConfig.sections[section]->itemdefs[CurrentConfig.sections[section]->nitemdefs-1].name:
                               CurrentConfig.sections[section]->itemdefs[item-1].name,

                    "url_type",CurrentConfig.sections[section]->itemdefs[item].url_type?"yes":"",
                    "key_type",ConfigTypeString(CurrentConfig.sections[section]->itemdefs[item].key_type),
                    "val_type",ConfigTypeString(CurrentConfig.sections[section]->itemdefs[item].val_type),

                    "def_key",CurrentConfig.sections[section]->itemdefs[item].key_type==Fixed?
                              CurrentConfig.sections[section]->itemdefs[item].name:"",
                    "def_val",CurrentConfig.sections[section]->itemdefs[item].def_val,

                    "url",edit?entry_url:url,
                    "key",edit?entry_key:(!CurrentConfig.sections[section]->itemdefs[item].url_type &&
                                          CurrentConfig.sections[section]->itemdefs[item].key_type==UrlSpecification &&
                                          CurrentConfig.sections[section]->itemdefs[item].val_type==None)?url:key,
                    "val",edit?entry_val:val,

                    "entry",entry,

                    "nentries",nentries,
                    "nallowed",nallowed,
                    NULL);

    /* No items present => no list */

    if(!*CurrentConfig.sections[section]->itemdefs[item].item ||
       (*CurrentConfig.sections[section]->itemdefs[item].item)->nentries==0)
       ;

    /* Only one entry allowed => one item */

    else if(CurrentConfig.sections[section]->itemdefs[item].same_key==0 &&
            CurrentConfig.sections[section]->itemdefs[item].url_type==0)
      {
       char *string=ConfigEntryString(*CurrentConfig.sections[section]->itemdefs[item].item,0);

       HTMLMessageBody(fd,"ConfigurationItem-Body",
                       "thisentry",string,
                       "entry",string,
                       NULL);

       free(string);
      }

    /* Several entries allowed => list */

    else
      {
       for(i=0;i<(*CurrentConfig.sections[section]->itemdefs[item].item)->nentries;i++)
         {
          char *string=ConfigEntryString(*CurrentConfig.sections[section]->itemdefs[item].item,i);

          HTMLMessageBody(fd,"ConfigurationItem-Body",
                          "thisentry",string,
                          NULL);

          free(string);
         }
      }

    HTMLMessageBody(fd,"ConfigurationItem-Tail",
                    NULL);

    if(entry_url) free(entry_url);
    if(entry_key) free(entry_key);
    if(entry_val) free(entry_val);

    if(line1)
       free(line1);
    if(line2)
       free(line2);

    finish_io(file);
    close(file);
   }

    /* Display the page with the results of editing the parameters. */

 else
   {
    char *errmsg=NULL;
    char *newentry=NULL;

    if(url || key || val)
       newentry=MakeConfigEntryString(&CurrentConfig.sections[section]->itemdefs[item],url,key,val);

    if(!strcmp(action,"insert"))
       errmsg=ModifyConfigFile(section,item,newentry,NULL ,NULL ,NULL );
    else if(!strcmp(action,"before") && entry)
       errmsg=ModifyConfigFile(section,item,newentry,NULL ,NULL ,entry);
    else if(!strcmp(action,"replace") && entry)
       errmsg=ModifyConfigFile(section,item,newentry,NULL ,entry,NULL );
    else if(!strcmp(action,"after") && entry)
       errmsg=ModifyConfigFile(section,item,newentry,entry,NULL ,NULL );
    else if(!strcmp(action,"delete") && entry)
       errmsg=ModifyConfigFile(section,item,NULL    ,NULL ,entry,NULL );
    else
      {
       errmsg=(char*)malloc((size_t)96);
       strcpy(errmsg,"The specified form action was invalid or an existing entry parameter was missing.");
      }

    /* Display the result */

    HTMLMessage(fd,200,"WWWOFFLE Configuration Change Page",NULL,"ConfigurationChange",
                "section",CurrentConfig.sections[section]->name,
                "item",CurrentConfig.sections[section]->itemdefs[item].name,
                "action",action,
                "oldentry",entry,
                "newentry",newentry,
                "errmsg",errmsg,
                NULL);

    if(errmsg) free(errmsg);

    if(newentry) free(newentry);
   }

 if(key)    free(key);
 if(val)    free(val);
 if(action) free(action);
 if(entry)  free(entry);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page to edit the URL-SPECIFICATION and redirect to the real page.

  int fd The file descriptor to write to.

  URL *Url The URL of the page that is being requested.

  Body *request_body The body of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationEditURLPage(int fd,URL *Url,Body *request_body)
{
 char *url=NULL,*encurl;
 int urllen;
 char *proto=NULL,*host=NULL,*port=NULL,*path=NULL,*args=NULL;
 char *proto_other=NULL,*host_other=NULL,*port_other=NULL,*path_other=NULL,*args_other=NULL;
 char *localurl,*relocate;

 /* Parse the arguments. */

 if(request_body)
   {
    char *form=URLRecodeFormArgs(request_body->content);
    char **arglist=SplitFormArgs(form);
    int i;

    for(i=0;arglist[i];i++)
      {
       if(!strncmp("proto=",arglist[i],(size_t)6) && arglist[i][6])
          proto=TrimArgs(URLDecodeFormArgs(arglist[i]+6));
       else if(!strncmp("host=",arglist[i],(size_t)5) && arglist[i][5])
          host=TrimArgs(URLDecodeFormArgs(arglist[i]+5));
       else if(!strncmp("port=",arglist[i],(size_t)5) && arglist[i][5])
          port=TrimArgs(URLDecodeFormArgs(arglist[i]+5));
       else if(!strncmp("path=",arglist[i],(size_t)5) && arglist[i][5])
          path=TrimArgs(URLDecodeFormArgs(arglist[i]+5));
       else if(!strncmp("args=",arglist[i],(size_t)5) && arglist[i][5])
          args=TrimArgs(URLDecodeFormArgs(arglist[i]+5));
       else if(!strncmp("proto_other=",arglist[i],(size_t)12) && arglist[i][12])
          proto_other=TrimArgs(URLDecodeFormArgs(arglist[i]+12));
       else if(!strncmp("host_other=",arglist[i],(size_t)11) && arglist[i][11])
          host_other=TrimArgs(URLDecodeFormArgs(arglist[i]+11));
       else if(!strncmp("port_other=",arglist[i],(size_t)11) && arglist[i][11])
          port_other=TrimArgs(URLDecodeFormArgs(arglist[i]+11));
       else if(!strncmp("path_other=",arglist[i],(size_t)11) && arglist[i][11])
          path_other=TrimArgs(URLDecodeFormArgs(arglist[i]+11));
       else if(!strncmp("args_other=",arglist[i],(size_t)11) && arglist[i][11])
          args_other=TrimArgs(URLDecodeFormArgs(arglist[i]+11));
       else
          PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",arglist[i],Url->name);
      }

    free(arglist[0]);
    free(arglist);
    free(form);
   }

 /* Sort out a URL from the mess of arguments. */

 if(proto && !strcmp(proto,"OTHER") && proto_other)
   {free(proto); proto=proto_other;}
 else
   {if(proto_other) free(proto_other);}

 if(host && !strcmp(host,"OTHER") && host_other)
   {free(host); host=host_other;}
 else
   {if(host_other) free(host_other);}

 if(port && !strcmp(port,"OTHER") && port_other)
   {free(port); port=(char*)malloc(strlen(port_other)+2); strcpy(port,":"); strcat(port,port_other);}
 else
   {if(port_other) free(port_other);}

 if(path && !strcmp(path,"OTHER") && path_other)
   {free(path); path=path_other;}
 else
   {if(path_other) free(path_other);}

 if(args && !strcmp(args,"OTHER") && args_other)
   {free(args); args=(char*)malloc(strlen(args_other)+2); strcpy(args,"?"); strcat(args,args_other);}
 else
   {if(args_other) free(args_other);}

 if(!proto)
    proto=strcpy(malloc((size_t)2),"*");

 if(!host)
    host=strcpy(malloc((size_t)2),"*");

 if(!path)
    path=strcpy(malloc((size_t)2),"*");

 /* Create the URL */

 urllen=4;
 urllen+=strlen(proto);
 urllen+=strlen(host);
 if(port)
    urllen+=strlen(port);
 urllen+=strlen(path);
 if(args)
    urllen+=strlen(args);

 url=(char*)malloc(urllen);

 strcpy(url,proto);
 strcat(url,"://");
 strcat(url,host);
 if(port)
    strcat(url,port);
 strcat(url,path);
 if(args)
    strcat(url,args);

 if(proto)free(proto);
 if(host) free(host);
 if(port) free(port);
 if(path) free(path);
 if(args) free(args);

 /* Redirect the client to it */

 localurl=GetLocalURL();
 encurl=URLEncodeFormArgs(url);
 relocate=(char*)malloc(strlen(localurl)+strlen(encurl)+24);

 sprintf(relocate,"%s/configuration/url?%s",localurl,encurl);

 HTMLMessage(fd,302,"WWWOFFLE Configuration Edit URL Redirect",relocate,"Redirect",
             "location",relocate,
             NULL);

 free(url);
 free(encurl);
 free(localurl);
 free(relocate);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page for a specific URL.

  int fd The file descriptor to write to.

  char *url The URL specification from the URL argument.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationURLPage(int fd,char *url)
{
 char *proto=NULL,*host=NULL,*port=NULL,*path=NULL,*args=NULL;
 char *copy,*colon,*slash,*ques;
 int wildcard=0,file,s,i;
 off_t seekpos=0;
 char *line1=NULL,*line2=NULL;

 /* Assume a "well-formed" URL, from the function above or a WWWOFFLE index.
    If it isn't then try and make something useful from it, at least don't crash. */

 copy=(char*)malloc(strlen(url)+1);
 strcpy(copy,url);

 colon=strchr(copy,':');
 slash=strchr(copy,'/');

 proto=copy;

 host=slash+2;

 if(colon && slash && slash==colon+1 && *(slash+1)=='/')
   {
    *colon=0;

    colon=strchr(slash+2,':');
    slash=strchr(slash+2,'/');
   }

 if(colon && slash && colon<slash)
   {
    *colon=0;
    if(slash==colon+1)
       port=":";
    else
       port=colon+1;
   }

 if(slash)
   {
    *slash=0;

    path=slash+1;
   }
 else
    path="";

 ques=strchr(path,'?');

 if(ques)
   {
    if(*(ques+1)==0)
       args="?";
    else
       args=ques+1;

    *ques=0;
   }

 if(strchr(url,'*') || (port && *port==':') || (args && *args=='?'))
    wildcard=1;

 /* Display the HTML */

 file=OpenLanguageFile("messages/README.CONF.txt");

 if(file==-1)
   {
    HTMLMessage(fd,500,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                "error","Cannot open README.CONF.txt",
                NULL);

    free(copy);

    return;
   }

 init_io(file);

 HTMLMessageHead(fd,200,"WWWOFFLE Configuration URL Page",
                 NULL);

 HTMLMessageBody(fd,"ConfigurationUrl-Head",
                 "wildcard",wildcard?"yes":"",
                 "url",url,
                 "proto",proto,
                 "host",host,
                 "port",port,
                 "path",path,
                 "args",args,
                 NULL);

 free(copy);

 /* Loop through all of the ConfigItems and find those that take a URL argument. */

 for(s=0;s<CurrentConfig.nsections;s++)
   {
    lseek(file,(off_t)0,SEEK_SET);  /* go back to the start of the file */
    reinit_io(file);

    while((line1=read_line(file,line1)))
      {
       line1[strlen(line1)-1]=0;

       if(!strncmp(line1,"SECTION",(size_t)7) && !strcmp(line1+8,CurrentConfig.sections[s]->name))
         {
          seekpos=lseek(file,(off_t)0,SEEK_CUR);
          if(seekpos>REWIND_STEP)
             seekpos-=REWIND_STEP;
          else
             seekpos=0;
          break;
         }
      }

    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;i++)
       if(CurrentConfig.sections[s]->itemdefs[i].url_type ||
          CurrentConfig.sections[s]->itemdefs[i].key_type==UrlSpecification)
         {
          char *template=NULL,*description=NULL,*current=NULL;

          lseek(file,seekpos,SEEK_SET);  /* go back only to the start of the current section */
          reinit_io(file);

          while((line1=read_line(file,line1)))
            {
             line1[strlen(line1)-1]=0;

             if(!strncmp(line1,"SECTION",(size_t)7) && !strcmp(line1+8,CurrentConfig.sections[s]->name))
                break;
            }

          while((line1=read_line(file,line1)))
            {
             line1[strlen(line1)-1]=0;

             if(!strncmp(line1,"SECTION",(size_t)7))
                break;
             if(!strncmp(line1,"ITEM",(size_t)4) && !strcmp(line1+5,CurrentConfig.sections[s]->itemdefs[i].name))
               {
                line1=read_line(file,line1);
                if(line1)
                   line1[strlen(line1)-1]=0;
                line2=read_line(file,line2);
                if(line2)
                   line2[strlen(line2)-1]=0;
                template=line1;
                description=line2;
                break;
               }
            }

          if(!wildcard)
            {
             URL *Url=SplitURL(url);

             if(*CurrentConfig.sections[s]->itemdefs[i].item &&
                CurrentConfig.sections[s]->itemdefs[i].key_type!=String)
               {
                int j;

                for(j=0;j<(*CurrentConfig.sections[s]->itemdefs[i].item)->nentries;j++)
                   if(CurrentConfig.sections[s]->itemdefs[i].url_type)
                     {
                      if(!(*CurrentConfig.sections[s]->itemdefs[i].item)->url[j])
                         break;
                      else if(MatchUrlSpecification((*CurrentConfig.sections[s]->itemdefs[i].item)->url[j],Url->proto,Url->host,Url->port,Url->path,Url->args))
                         break;
                     }
                   else
                     {
                      if(MatchUrlSpecification((*CurrentConfig.sections[s]->itemdefs[i].item)->key[j].urlspec,Url->proto,Url->host,Url->port,Url->path,Url->args))
                         break;
                     }

                if(j!=(*CurrentConfig.sections[s]->itemdefs[i].item)->nentries)
                   current=ConfigEntryString(*CurrentConfig.sections[s]->itemdefs[i].item,j);
               }

             FreeURL(Url);
            }

          HTMLMessageBody(fd,"ConfigurationUrl-Body",
                          "section",CurrentConfig.sections[s]->name,
                          "item",CurrentConfig.sections[s]->itemdefs[i].name,
                          "template",template,
                          "description",description,
                          "current",current,
                          NULL);

          if(current)
             free(current);
         }
   }

 HTMLMessageBody(fd,"ConfigurationUrl-Tail",
                 NULL);

 if(line1)
    free(line1);
 if(line2)
    free(line2);

 finish_io(file);
 close(file);
}


/*++++++++++++++++++++++++++++++++++++++
  Inform the user that the authorisation failed.

  int fd The file descriptor to write to.

  char *url The specified path.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationAuthFail(int fd,char *url)
{
 HTMLMessageHead(fd,401,"WWWOFFLE Authorisation Failed",
                 "WWW-Authenticate","Basic realm=\"control\"",
                 NULL);
 HTMLMessageBody(fd,"ConfigurationAuthFail",
                 "url",url,
                 NULL);
}
