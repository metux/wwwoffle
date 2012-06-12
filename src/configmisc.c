/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/configmisc.c,v 1.33 2007-05-27 11:24:14 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  Configuration file data management functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <pwd.h>
#include <grp.h>

#include "misc.h"
#include "errors.h"
#include "configpriv.h"


/* Local functions */

static /*@null@*/ char* sprintf_key_or_value(ConfigType type,KeyOrValue key_or_val);
static /*@null@*/ char* sprintf_url_spec(const UrlSpec *urlspec);
static /*@null@*/ char *strstrn(const char *phaystack, const char *pneedle, size_t n, int nocase);


/*+ The backup version of the config file. +*/
static ConfigFile BackupConfig;


/*++++++++++++++++++++++++++++++++++++++
  Set the configuration file default values.
  ++++++++++++++++++++++++++++++++++++++*/

void DefaultConfigFile(void)
{
 int s,i;
 char *errmsg;

 for(s=0;s<CurrentConfig.nsections;s++)
    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;i++)
       if(CurrentConfig.sections[s]->itemdefs[i].def_val)
         {
          ConfigItem *item=CurrentConfig.sections[s]->itemdefs[i].item;

          *item=(ConfigItem)malloc(sizeof(struct _ConfigItem));
          (*item)->itemdef=&CurrentConfig.sections[s]->itemdefs[i];
          (*item)->nentries=0;
          (*item)->url=NULL;
          (*item)->key=NULL;
          (*item)->val=NULL;
          (*item)->def_val=(KeyOrValue*)malloc(sizeof(KeyOrValue));

          if(CurrentConfig.sections[s]->itemdefs[i].key_type!=Fixed)
             PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

          if((errmsg=ParseKeyOrValue(CurrentConfig.sections[s]->itemdefs[i].def_val,CurrentConfig.sections[s]->itemdefs[i].val_type,(*item)->def_val)))
             PrintMessage(Fatal,"Configuration file error at %s:%d; %s",__FILE__,__LINE__,errmsg);
         }
}


/*++++++++++++++++++++++++++++++++++++++
  Save the old values in case the re-read of the file fails.
  ++++++++++++++++++++++++++++++++++++++*/

void CreateBackupConfigFile(void)
{
 int s,i;

 /* Create a backup of all of the sections. */

 BackupConfig=CurrentConfig;
 BackupConfig.sections=(ConfigSection**)malloc(CurrentConfig.nsections*sizeof(ConfigSection*));

 for(s=0;s<CurrentConfig.nsections;s++)
   {
    BackupConfig.sections[s]=(ConfigSection*)malloc(sizeof(ConfigSection));
    *BackupConfig.sections[s]=*CurrentConfig.sections[s];
    BackupConfig.sections[s]->itemdefs=(ConfigItemDef*)malloc(CurrentConfig.sections[s]->nitemdefs*sizeof(ConfigItemDef));

    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;i++)
      {
       BackupConfig.sections[s]->itemdefs[i]=CurrentConfig.sections[s]->itemdefs[i];
       BackupConfig.sections[s]->itemdefs[i].item=(ConfigItem*)malloc(sizeof(ConfigItem));

       *BackupConfig.sections[s]->itemdefs[i].item=*CurrentConfig.sections[s]->itemdefs[i].item;
       *CurrentConfig.sections[s]->itemdefs[i].item=NULL;
      }
   }

 /* Restore the default values */

 DefaultConfigFile();
}


/*++++++++++++++++++++++++++++++++++++++
  Restore the old values if the re-read of the file failed.
  ++++++++++++++++++++++++++++++++++++++*/

void RestoreBackupConfigFile(void)
{
 int s,i;

 /* Restore all of the sections. */

 for(s=0;s<CurrentConfig.nsections;s++)
   {
    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;i++)
      {
       FreeConfigItem(*CurrentConfig.sections[s]->itemdefs[i].item);

       *CurrentConfig.sections[s]->itemdefs[i].item=*BackupConfig.sections[s]->itemdefs[i].item;

       free(BackupConfig.sections[s]->itemdefs[i].item);
      }

    free(BackupConfig.sections[s]->itemdefs);
    free(BackupConfig.sections[s]);
   }

 free(BackupConfig.sections);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the old values if the re-read of the file succeeded.

  int restore_startup Set to true if the StartUp section is to be restored.
  ++++++++++++++++++++++++++++++++++++++*/

void PurgeBackupConfigFile(int restore_startup)
{
 int s,i;

 /* Purge all of the sections and restore StartUp if needed. */

 for(s=0;s<BackupConfig.nsections;s++)
   {
    for(i=0;i<BackupConfig.sections[s]->nitemdefs;i++)
      {
       if(s==0 && restore_startup)
         {
          FreeConfigItem(*CurrentConfig.sections[s]->itemdefs[i].item);

          *CurrentConfig.sections[s]->itemdefs[i].item=*BackupConfig.sections[s]->itemdefs[i].item;
         }
       else
          FreeConfigItem(*BackupConfig.sections[s]->itemdefs[i].item);

       free(BackupConfig.sections[s]->itemdefs[i].item);
      }

    free(BackupConfig.sections[s]->itemdefs);
    free(BackupConfig.sections[s]);
   }

 free(BackupConfig.sections);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the values in the config file.
  ++++++++++++++++++++++++++++++++++++++*/

void PurgeConfigFile(void)
{
 int s,i;

 /* Purge all of the sections. */

 for(s=0;s<CurrentConfig.nsections;s++)
   {
    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;i++)
      {
       FreeConfigItem(*CurrentConfig.sections[s]->itemdefs[i].item);
      }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Free a ConfigItem list.

  ConfigItem item The item to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeConfigItem(ConfigItem item)
{
 int i;

 if(!item)
    return;

 for(i=0;i<item->nentries;i++)
   {
    if(item->url)
       FreeKeyOrValue((KeyOrValue*)&item->url[i],UrlSpecification);

    FreeKeyOrValue(&item->key[i],item->itemdef->key_type);

    if(item->val)
       FreeKeyOrValue(&item->val[i],item->itemdef->val_type);
   }

 if(item->nentries)
   {
    if(item->url)
       free(item->url);
    free(item->key);
    if(item->val)
       free(item->val);
   }

 if(item->def_val)
   {
    FreeKeyOrValue(item->def_val,item->itemdef->val_type);
    free(item->def_val);
   }

 free(item);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a Key or Value.

  KeyOrValue *keyval The key or value to free.

  ConfigType type The type of key or value.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeKeyOrValue(KeyOrValue *keyval,ConfigType type)
{
 switch(type)
   {
    /* None or Fixed */

   case Fixed:
   case None:
    break;

    /* Integer */

   case CfgMaxServers:
   case CfgMaxFetchServers:
   case CfgLogLevel:
   case Boolean:
   case PortNumber:
   case AgeDays:
   case TimeSecs:
   case CacheSize:
   case FileSize:
   case Percentage:
   case UserId:
   case GroupId:
   case FileMode:
    break;

    /* String */

   case String:
   case PathName:
   case FileExt:
   case MIMEType:
   case Host:
   case HostOrNone:
   case HostAndPort:
   case HostAndPortOrNone:
   case HostWild:
   case HostAndPortWild:
   case UserPass:
   case Url:
   case UrlWild:
    if(keyval->string)
       free(keyval->string);
    break;

   case UrlSpecification:
    if(keyval->urlspec)
       free(keyval->urlspec);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Check if a protocol, host, path and args match a URL-SPECIFICATION in the config file.

  int MatchUrlSpecification Return the matching length if true else 0.

  const UrlSpec *spec The URL-SPECIFICATION.

  const char *proto The protocol.

  const char *host The host.

  int port The port number (or -1).

  const char *path The path.

  const char *args The arguments.
  ++++++++++++++++++++++++++++++++++++++*/

int MatchUrlSpecification(const UrlSpec *spec,const char *proto,const char *host,int port,const char *path,const char *args)
{
 int match=0;

 if((!spec->proto || !strcmp(UrlSpecProto(spec),proto)) &&
    (!spec->host || WildcardMatch(host,UrlSpecHost(spec),0)) &&
    (spec->port==-1 || (port==-1 && spec->port==0) || (port!=-1 && port==spec->port)) &&
    (!spec->path || WildcardMatch(path,UrlSpecPath(spec),spec->nocase) || 
     ((!strncmp(UrlSpecPath(spec),"/*/",(size_t)3) && WildcardMatch(path,UrlSpecPath(spec)+2,spec->nocase)))) &&
    (!spec->args || (args && WildcardMatch(args,UrlSpecArgs(spec),spec->nocase)) || (!args && *UrlSpecArgs(spec)==0)))
   {
    match=(spec->proto?strlen(UrlSpecProto(spec)):0)+
          (spec->host ?strlen(UrlSpecHost(spec) ):0)+
          (spec->path ?strlen(UrlSpecPath(spec) ):0)+
          (spec->args ?strlen(UrlSpecArgs(spec) ):0)+1;
   }

 return(match);
}


/*++++++++++++++++++++++++++++++++++++++
  Do a match using a wildcard specified with '*' in it.

  int WildcardMatch returns 1 if there is a match.

  const char *string The fixed string that is being matched.

  const char *pattern The pattern to match against.

  int nocase A flag that if set to 1 ignores the case of the string.

  By Paul A. Rombouts <p.a.rombouts@home.nl>, handles more than two '*' using simpler algorithm than previously.

  See also the strstrn() function at the bottom of this file.
  ++++++++++++++++++++++++++++++++++++++*/

int WildcardMatch(const char *string,const char *pattern,int nocase)
{
 size_t len_beg;
 const char *midstr, *endstr;
 const char *pattstr, *starp=strchr(pattern,'*');

 if(!starp)
    return(( nocase && !strcasecmp(string,pattern)) ||
           (!nocase && !strcmp(string,pattern)));

 len_beg=starp-pattern;
 if(( nocase && strncasecmp(string,pattern,len_beg)) ||
    (!nocase && strncmp(string,pattern,len_beg)))
    return(0);

 midstr=string+len_beg;

 while(pattstr=starp+1,starp=strchr(pattstr,'*'))
   {
    size_t len_patt=starp-pattstr;
    char *match=strstrn(midstr,pattstr,len_patt,nocase);

    if(!match)
       return(0);

    midstr=match+len_patt;
   }

 endstr=midstr+strlen(midstr)-strlen(pattstr);

 if(midstr>endstr)
    return(0);

 return(( nocase && !strcasecmp(endstr,pattstr)) ||
        (!nocase && !strcmp(endstr,pattstr)));
}


/*++++++++++++++++++++++++++++++++++++++
  Return the string that represents the Configuration type.

  char *ConfigTypeString Returns a static string.

  ConfigType type The configuration type.
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigTypeString(ConfigType type)
{
 switch(type)
   {
   case Fixed:
    return "Fixed";              /* key */
   case None:
    return "None";                         /* val */
   case CfgMaxServers:
    return "CfgMaxServers";                /* val */
   case CfgMaxFetchServers:
    return "CfgMaxFetchServers";           /* val */
   case CfgLogLevel:
    return "CfgLogLevel";                  /* val */
   case Boolean:
    return "Boolean";                      /* val */
   case PortNumber:
    return "PortNumber";                   /* val */
   case AgeDays:
    return "AgeDays";                      /* val */
   case TimeSecs:
    return "TimeSecs";                     /* val */
   case CacheSize:
    return "CacheSize";                    /* val */
   case FileSize:
    return "FileSize";                     /* val */
   case Percentage:
    return "Percentage";                   /* val */
   case UserId:
    return "UserId";                       /* val */
   case GroupId:
    return "GroupId";                      /* val */
   case String:
    return "String";             /* key */ /* val */
   case PathName:
    return "PathName";                     /* val */
   case FileExt:
    return "FileExt";            /* key */ /* val */
   case FileMode:
    return "FileMode";                     /* val */
   case MIMEType:
    return "MIMEType";                     /* val */
   case Host:
    return "Host";               /* key */
   case HostOrNone:
    return "HostOrNone";                   /* val */
   case HostAndPort:
    return "HostAndPort";
   case HostAndPortOrNone:
    return "HostAndPortOrNone";            /* val */
   case HostWild:
    return "HostWild";                     /* val */
   case HostAndPortWild:
    return "HostAndPortWild";              /* val */
   case UserPass:
    return "UserPass";           /* key */
   case Url:
    return "Url";                          /* val */
   case UrlWild:
    return "UrlWild";                      /* val */
   case UrlSpecification:
    return "UrlSpecification";   /* key */ /* val */
   }

 /*@notreached@*/

 return("??Unknown??");
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a Configuration entry into a canonical printable string.

  char *ConfigEntryString Returns a malloced string.

  ConfigItem item The configuration item.

  int which Which particular entry in the ConfigItem to print.
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigEntryString(ConfigItem item,int which)
{
 char *url=NULL,*key=NULL,*val=NULL;
 char *string;

 /* Get the sub-strings */

 ConfigEntryStrings(item,which,&url,&key,&val);

 /* Create the string */

 string=MakeConfigEntryString(item->itemdef,url,key,val);

 /* Send the results back */

 if(url) free(url);
 if(key) free(key);
 if(val) free(val);

 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a Configuration entry into a canonical printable string.

  ConfigItem item The configuration item.

  int which Which particular entry in the ConfigItem to print.

  char **url Returns the URL string.

  char **key Returns the key string.

  char **val Returns the value string.
  ++++++++++++++++++++++++++++++++++++++*/

void ConfigEntryStrings(ConfigItem item,int which,char **url,char **key,char **val)
{
 /* Handle the URL */

 if(item->url && item->url[which])
    *url=sprintf_url_spec(item->url[which]);
 else
    *url=NULL;

 /* Handle the key */

 *key=sprintf_key_or_value(item->itemdef->key_type,item->key[which]);

 /* Handle the value */

 if(item->itemdef->val_type!=None)
    *val=sprintf_key_or_value(item->itemdef->val_type,item->val[which]);
 else
    *val=NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Make a Configuration entry string from supplied arguments.

  char *MakeConfigEntryString Returns a malloced string.

  const ConfigItemDef *itemdef The configuration item definition.

  const char *url Specifies the URL string.

  const char *key Specifies the key string.

  const char *val Specifies the val string.
  ++++++++++++++++++++++++++++++++++++++*/

char *MakeConfigEntryString(const ConfigItemDef *itemdef,const char *url,const char *key,const char *val)
{
 int strpos=0;
 char *string=(char*)calloc((size_t)8,sizeof(char));

 /* Handle the URL */

 if(url)
   {
    string=(char*)realloc((void*)string,strpos+1+3+strlen(url));
    sprintf(string+strpos,"<%s> ",url);
    strpos+=3+strlen(url);
   }

 /* Handle the key */

 if(key)
   {
    string=(char*)realloc((void*)string,strpos+1+strlen(key));
    sprintf(string+strpos,"%s",key);
    strpos+=strlen(key);
   }

 /* Handle the value */

 if(itemdef->val_type!=None)
   {
    string=(char*)realloc((void*)string,strpos+1+3);
    sprintf(string+strpos," = ");
    strpos+=3;

    if(val)
      {
       string=(char*)realloc((void*)string,strpos+1+strlen(val));
       sprintf(string+strpos,"%s",val);
       strpos+=strlen(val);
      }
   }

 /* Send the result back */

 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a KeyOrValue type into a string.

  char* sprintf_key_or_value Return the newly malloced string.

  ConfigType type The type of the KeyOrValue.

  KeyOrValue key_or_val The KeyOrValue.
  ++++++++++++++++++++++++++++++++++++++*/

static char* sprintf_key_or_value(ConfigType type,KeyOrValue key_or_val)
{
 char *string=NULL;

 switch(type)
   {
    /* None or Fixed */

   case Fixed:
    string=(char*)malloc(1+strlen(key_or_val.string));
    strcpy(string,key_or_val.string);
    break;

   case None:
    break;

    /* Integer */

   case Boolean:
    string=(char*)malloc((size_t)4);
    strcpy(string,key_or_val.integer?"yes":"no");
    break;

   case CfgMaxServers:
   case CfgMaxFetchServers:
   case PortNumber:
   case CacheSize:
   case FileSize:
   case Percentage:
    string=(char*)malloc((size_t)(MAX_INT_STR+1));
    sprintf(string,"%d",key_or_val.integer);
    break;

   case CfgLogLevel:
    string=(char*)malloc((size_t)17);
    if(key_or_val.integer==Debug)
       strcpy(string,"debug");
    if(key_or_val.integer==Inform)
       strcpy(string,"info");
    if(key_or_val.integer==Important)
       strcpy(string,"important");
    if(key_or_val.integer==Warning)
       strcpy(string,"debug");
    if(key_or_val.integer==Fatal)
       strcpy(string,"fatal");
    break;

   case UserId:
    {
     struct passwd *pwd=getpwuid((uid_t)key_or_val.integer);
     if(pwd)
       {
        string=(char*)malloc(1+strlen(pwd->pw_name));
        strcpy(string,pwd->pw_name);
       }
     else
       {
        string=(char*)malloc((size_t)(MAX_INT_STR+1));
        sprintf(string,"%d",key_or_val.integer);
       }
    }
   break;

   case GroupId:
    {
     struct group *grp=getgrgid((gid_t)key_or_val.integer);
     if(grp)
       {
        string=(char*)malloc(1+strlen(grp->gr_name));
        strcpy(string,grp->gr_name);
       }
     else
       {
        string=(char*)malloc((size_t)(MAX_INT_STR+1));
        sprintf(string,"%d",key_or_val.integer);
       }
    }
   break;

   case FileMode:
    string=(char*)malloc((size_t)(1+MAX_INT_STR+1));
    sprintf(string,"0%o",(unsigned)key_or_val.integer);
    break;

   case AgeDays:
    {
     int weeks,months,years,days=key_or_val.integer;

     string=(char*)malloc((size_t)(MAX_INT_STR+1+1));

     years=days/365;
     if(years*365==days)
        sprintf(string,"%dy",years);
     else
       {
        months=days/30;
        if(months*30==days)
           sprintf(string,"%dm",months);
        else
          {
           weeks=days/7;
           if(weeks*7==days)
              sprintf(string,"%dw",weeks);
           else
              sprintf(string,"%d",days);
          }
       }
    }
   break;

   case TimeSecs:
    {
     int weeks,days,hours,minutes,seconds=key_or_val.integer;

     string=(char*)malloc((size_t)(MAX_INT_STR+1+1));

     weeks=seconds/(3600*24*7);
     if(weeks*(3600*24*7)==seconds)
        sprintf(string,"%dw",weeks);
     else
       {
        days=seconds/(3600*24);
        if(days*(3600*24)==seconds)
           sprintf(string,"%dd",days);
        else
          {
           hours=seconds/(3600);
           if(hours*(3600)==seconds)
              sprintf(string,"%dh",hours);
           else
             {
              minutes=seconds/(60);
              if(minutes*(60)==seconds)
                 sprintf(string,"%dm",minutes);
              else
                 sprintf(string,"%d",seconds);
             }
          }
       }
    }
    break;

    /* String */

   case String:
   case PathName:
   case FileExt:
   case MIMEType:
   case Host:
   case HostOrNone:
   case HostAndPort:
   case HostAndPortOrNone:
   case HostWild:
   case HostAndPortWild:
   case UserPass:
   case Url:
   case UrlWild:
    if(key_or_val.string)
      {
       string=(char*)malloc(1+strlen(key_or_val.string));
       strcpy(string,key_or_val.string);
      }
    else
      {
       string=(char*)malloc((size_t)1);
       *string=0;
      }
    break;

    /* Url Specification */

   case UrlSpecification:
    string=sprintf_url_spec(key_or_val.urlspec);
   }

 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a URL-SPECIFICATION into a string.

  char* sprintf_url_spec Return the new string.

  const UrlSpec *urlspec The URL-SPECIFICATION to convert.
  ++++++++++++++++++++++++++++++++++++++*/

static char* sprintf_url_spec(const UrlSpec *urlspec)
{
 char *string=NULL;
 int strpos=0;
 size_t newlen;

 if(!urlspec)
    return(NULL);

 newlen=1+8;
 if(HasUrlSpecProto(urlspec)) newlen+=strlen(UrlSpecProto(urlspec));
 if(HasUrlSpecHost (urlspec)) newlen+=strlen(UrlSpecHost (urlspec));
 if(UrlSpecPort(urlspec)>0)   newlen+=MAX_INT_STR;
 if(HasUrlSpecPath (urlspec)) newlen+=strlen(UrlSpecPath (urlspec));
 if(HasUrlSpecArgs (urlspec)) newlen+=strlen(UrlSpecArgs (urlspec));

 string=(char*)malloc(newlen);
 *string=0;

 if(urlspec->negated)
   {
    sprintf(string+strpos,"%s","!");
    strpos+=strlen(string+strpos);
   }

 if(urlspec->nocase)
   {
    sprintf(string+strpos,"%s","~");
    strpos+=strlen(string+strpos);
   }

 sprintf(string+strpos,"%s://",HasUrlSpecProto(urlspec)?UrlSpecProto(urlspec):"*");
 strpos+=strlen(string+strpos);

 sprintf(string+strpos,"%s",HasUrlSpecHost(urlspec)?UrlSpecHost(urlspec):"*");
 strpos+=strlen(string+strpos);

 if(UrlSpecPort(urlspec)==0)
    sprintf(string+strpos,":");
 else if(UrlSpecPort(urlspec)!=-1)
    sprintf(string+strpos,":%d",UrlSpecPort(urlspec));
 strpos+=strlen(string+strpos);

 sprintf(string+strpos,"%s",HasUrlSpecPath(urlspec)?UrlSpecPath(urlspec):"/*");
 strpos+=strlen(string+strpos);

 if(HasUrlSpecArgs(urlspec))
   {
    sprintf(string+strpos,"?%s",*UrlSpecArgs(urlspec)?UrlSpecArgs(urlspec):"*");
    strpos+=strlen(string+strpos);
   }

 return(string);
}



/* Return the offset of one string within another.
   Copyright (C) 1994, 1996, 1997, 2000 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/*
 * My personal strstr() implementation that beats most other algorithms.
 * Until someone tells me otherwise, I assume that this is the
 * fastest implementation of strstr() in C.
 * I deliberately chose not to comment it.  You should have at least
 * as much fun trying to understand it, as I had to write it :-).
 *
 * Stephen R. van den Berg, berg@pool.informatik.rwth-aachen.de	*/

/* strstrn() is a variation of strstr() that only tries to find
   the first n characters of "needle" in "haystack".
   If strlen(needle) happens to be less than n, strstrn() behaves 
   exactly like strstr().
   Modifications made by Paul Rombouts <p.a.rombouts@home.nl>.
*/

/* Added a parameter "nocase" to select case insensitive test (value 1)
   or case sensitive (0).
   Modification by Marc Boucher.
*/

typedef unsigned chartype;

static char *strstrn(const char *phaystack, const char *pneedle, size_t n, int nocase)
{
  register const unsigned char *haystack, *needle;
  register chartype b, c;
  const unsigned char *needle_end;

  char *lowhaystack=NULL;
  char *lowneedle=NULL;

  if (nocase) {
     int i;
     lowhaystack=malloc(strlen(phaystack)+1);
     lowneedle=malloc(strlen(pneedle)+1);

     for(i=0;phaystack[i];i++) { lowhaystack[i]=tolower(phaystack[i]); }
     lowhaystack[i]=0;
     for(i=0;pneedle[i];i++) { lowneedle[i]=tolower(pneedle[i]); }
     lowneedle[i]=0;
     
     phaystack=lowhaystack;
     pneedle=lowneedle;
     }

  haystack = (const unsigned char *) phaystack;
  needle = (const unsigned char *) pneedle;
  needle_end = needle+n;

  if (needle != needle_end && (b = *needle) != '\0' )
    {
      haystack--;                               /* possible ANSI violation */
      do
        {
          c = *++haystack;
          if (c == '\0')
            goto ret0;
        }
      while (c != b);

      if (++needle == needle_end || (c = *needle) == '\0')
        goto foundneedle;
      ++needle;
      goto jin;

      for (;;)
        {
          register chartype a;
          register const unsigned char *rhaystack, *rneedle;

          do
            {
              a = *++haystack;
              if (a == '\0')
                goto ret0;
              if (a == b)
                break;
              a = *++haystack;
              if (a == '\0')
                goto ret0;
shloop:
              ;
            }
          while (a != b);

jin:      a = *++haystack;
          if (a == '\0')
            goto ret0;

          if (a != c)
            goto shloop;

          rhaystack = haystack-- + 1;
          if(needle == needle_end) goto foundneedle;
          rneedle = needle;
          a = *rneedle;

          if (*rhaystack == a)
            do
              {
                if (a == '\0')
                  goto foundneedle;
                ++rhaystack;
                if(++needle == needle_end) goto foundneedle;
                a = *needle;
                if (*rhaystack != a)
                  break;
                if (a == '\0')
                  goto foundneedle;
                ++rhaystack;
                if(++needle == needle_end) goto foundneedle;
                a = *needle;
              }
            while (*rhaystack == a);

          needle = rneedle;             /* took the register-poor approach */

          if (a == '\0')
            break;
        }
    }
foundneedle:
  if (nocase) {
     if (lowhaystack) free(lowhaystack);
     if (lowneedle) free(lowneedle);
     }
  return (char*) haystack;
ret0:
  if (nocase) {
     if (lowhaystack) free(lowhaystack);
     if (lowneedle) free(lowneedle);
     }
  return 0;
}
