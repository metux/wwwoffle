/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configfunc.c 1.42 2006/07/21 17:37:34 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Configuration item checking functions.
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

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "misc.h"
#include "errors.h"
#include "configpriv.h"
#include "config.h"
#include "sockets.h"


/*+ The local HTTP (or HTTPS) port number. +*/
static int localport=-1;

/*+ The local port protocol (HTTP or HTTPS). +*/
static char *localproto=NULL;


/*++++++++++++++++++++++++++++++++++++++
  Determine an email address to use as the FTP password.

  char *DefaultFTPPassWord Returns a best-guess password.
  ++++++++++++++++++++++++++++++++++++++*/

char *DefaultFTPPassWord(void)
{
 struct passwd *pwd;
 char *username,*fqdn,*password;

 pwd=getpwuid(getuid());

 if(!pwd)
    username="root";
 else
    username=pwd->pw_name;

 fqdn=GetFQDN();

 if(!fqdn)
    fqdn="";

 password=(char*)malloc(strlen(username)+strlen(fqdn)+4);
 sprintf(password,"%s@%s",username,fqdn);

 return(password);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the specified host and port number is allowed for SSL.

  int IsSSLAllowed Returns true if it is allowed.

  URL *Url The URL of the host (no path) to match.

  int cached A flag to indicate that caching is proposed.
  ++++++++++++++++++++++++++++++++++++++*/

int IsSSLAllowed(URL *Url,int cached)
{
 int isit=0;
 int i;
 char *hostport=Url->hostport;

 if(Url->port==-1)
   {
    hostport=(char*)malloc(strlen(Url->hostport)+1+3+1);
    strcpy(hostport,Url->hostport);
    strcat(hostport,":443");
   }

 if(cached)
   {
    if(SSLAllowCache)
       for(i=0;i<SSLAllowCache->nentries;i++)
          if(WildcardMatch(hostport,SSLAllowCache->val[i].string,0))
            {isit=1;break;}

    if(SSLDisallowCache && isit)
       for(i=0;i<SSLDisallowCache->nentries;i++)
          if(WildcardMatch(hostport,SSLDisallowCache->val[i].string,0))
            {isit=0;break;}
   }
 else
   {
    if(SSLAllowTunnel)
       for(i=0;i<SSLAllowTunnel->nentries;i++)
          if(WildcardMatch(hostport,SSLAllowTunnel->val[i].string,0))
            {isit=1;break;}

    if(SSLDisallowTunnel && isit)
       for(i=0;i<SSLDisallowTunnel->nentries;i++)
          if(WildcardMatch(hostport,SSLDisallowTunnel->val[i].string,0))
            {isit=0;break;}
   }

 if(Url->port==-1)
    free(hostport);

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the specified path is allowed to be used for CGIs.

  int IsCGIAllowed Returns true if it is allowed.

  const char *path The pathname to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsCGIAllowed(const char *path)
{
 int isit=0;
 int i;

 if(ExecCGI)
    for(i=0;i<ExecCGI->nentries;i++)
       if(WildcardMatch(path,ExecCGI->val[i].string,0))
         {isit=1;break;}

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Set the local port number so that it can be used in local URLs.

  int port The port number.
  ++++++++++++++++++++++++++++++++++++++*/

void SetLocalPort(int port)
{
 localport=port;

 if(localport==ConfigInteger(HTTPS_Port))
    localproto="https";
 else
    localproto="http";
}


/*++++++++++++++++++++++++++++++++++++++
  Get the name of the first specified server in the Localhost section.

  char *GetLocalHost Returns the first named localhost.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetLocalHost(void)
{
 char *localhost,*ret;

 if(LocalHost && LocalHost->nentries)
    localhost=LocalHost->key[0].string;
 else
    localhost=DEF_LOCALHOST;

 ret=(char*)malloc(strlen(localhost)+1);

 strcpy(ret,localhost);

 return(ret);
}


/*++++++++++++++++++++++++++++++++++++++
  Get the name and port number of the first specified server in the Localhost section.

  char *GetLocalHostPort Returns the first named localhost and port number.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetLocalHostPort(void)
{
 char *localhost,*ret;

 if(LocalHost && LocalHost->nentries)
    localhost=LocalHost->key[0].string;
 else
    localhost=DEF_LOCALHOST;

 ret=(char*)malloc(strlen(localhost)+3+MAX_INT_STR+1);

 if(strchr(localhost,':'))      /* IPv6 */
    sprintf(ret,"[%s]:%d",localhost,localport);
 else
    sprintf(ret,"%s:%d",localhost,localport);

 return(ret);
}


/*++++++++++++++++++++++++++++++++++++++
  Get a URL for the first specified server in the Localhost section.

  char *GetLocalURL Returns a URL for the first named localhost and port number.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetLocalURL(void)
{
 char *localhost,*ret;

 if(LocalHost && LocalHost->nentries)
    localhost=LocalHost->key[0].string;
 else
    localhost=DEF_LOCALHOST;

 ret=(char*)malloc(strlen(localproto)+strlen(localhost)+6+MAX_INT_STR+1);

 if(strchr(localhost,':'))      /* IPv6 */
    sprintf(ret,"%s://[%s]:%d",localproto,localhost,localport);
 else
    sprintf(ret,"%s://%s:%d",localproto,localhost,localport);

 return(ret);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is the localhost.

  int IsLocalHost Return true if the host is the local host.

  const URL *Url The URL that has been requested.
  ++++++++++++++++++++++++++++++++++++++*/

int IsLocalHost(const URL *Url)
{
 int isit=0;
 int http_port,https_port;
 int i;

 http_port=ConfigInteger(HTTP_Port);
 https_port=ConfigInteger(HTTPS_Port);

 if(Url->port!=http_port && Url->port!=https_port && (Url->port!=-1 || http_port!=80 || https_port!=443))
    return(0);

 if(LocalHost)
    for(i=0;i<LocalHost->nentries;i++)
       if(!strcmp(LocalHost->key[i].string,Url->host))
         {isit=1;break;}

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is in the local network.

  int IsLocalNetHost Return true if the host is on the local network.

  const char *host The name of the host to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

int IsLocalNetHost(const char *host)
{
 int isit=0;
 int i;

 if(LocalHost)
    for(i=0;i<LocalHost->nentries;i++)
       if(!strcmp(LocalHost->key[i].string,host))
         {isit=1;break;}

 if(LocalNet && !isit)
   {
    for(i=0;i<LocalNet->nentries;i++)
       if(*LocalNet->key[i].string=='!')
         {
          if(WildcardMatch(host,LocalNet->key[i].string+1,0))
            {isit=0;break;}
         }
       else
         {
          if(WildcardMatch(host,LocalNet->key[i].string,0))
            {isit=1;break;}
         }
   }

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is allowed to connect.

  int IsAllowedConnectHost Return true if it is allowed to connect.

  const char *host The name of the host to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

int IsAllowedConnectHost(const char *host)
{
 int isit=0;
 int i;

 if(LocalHost)
    for(i=0;i<LocalHost->nentries;i++)
       if(!strcmp(LocalHost->key[i].string,host))
         {isit=1;break;}

 if(AllowedConnectHosts && !isit)
   {
    for(i=0;i<AllowedConnectHosts->nentries;i++)
       if(*AllowedConnectHosts->key[i].string=='!')
         {
          if(WildcardMatch(host,AllowedConnectHosts->key[i].string+1,0))
            {isit=0;break;}
         }
       else
         {
          if(WildcardMatch(host,AllowedConnectHosts->key[i].string,0))
            {isit=1;break;}
         }
   }

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified username and password is allowed to connect.

  char *IsAllowedConnectUser Return the username if it is allowed to connect.

  const char *userpass The encoded username and password of the user to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

char *IsAllowedConnectUser(const char *userpass)
{
 char *isit;
 int i;

 if(AllowedConnectUsers)
    isit=NULL;
 else
    isit="anybody";

 if(AllowedConnectUsers && userpass)
   {
    const char *up=userpass;

    while(*up!=' ') up++;
    while(*up==' ') up++;

    for(i=0;i<AllowedConnectUsers->nentries;i++)
       if(!strcmp(AllowedConnectUsers->key[i].string,up))
         {
          char *colon;
          size_t l;
          isit=Base64Decode(AllowedConnectUsers->key[i].string,&l);
          if((colon=strchr(isit,':')))
             *colon=0;
          break;
         }
   }

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the data for a URL can be compressed or not based on the MIME type or file extension.

  int NotCompressed Return 1 if the data is not to be compressed.

  const char *mime_type The MIME type of the data (may be NULL).

  const char *path The path of the URL (may be NULL).
  ++++++++++++++++++++++++++++++++++++++*/

int NotCompressed(const char *mime_type,const char *path)
{
 int retval=0;
 int i;

 if(mime_type && DontCompressMIME)
    for(i=0;i<DontCompressMIME->nentries;i++)
       if(!strcmp(DontCompressMIME->val[i].string,mime_type))
         {retval=1;break;}

 if(path && DontCompressExt)
    for(i=0;i<DontCompressExt->nentries;i++)
       if(strlen(path)>strlen(DontCompressExt->val[i].string) &&
          !strcmp(DontCompressExt->val[i].string,path+strlen(path)-strlen(DontCompressExt->val[i].string)))
         {retval=1;break;}

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the header line is to be sent to the server/client.

  char *CensoredHeader Returns the value to be inserted or NULL if it is to be removed.

  const URL *Url the URL that the request or reply is for/from.

  const char *key The key to check.

  char *val The default value to use.
  ++++++++++++++++++++++++++++++++++++++*/

char *CensoredHeader(const URL *Url,const char *key,char *val)
{
 char *new=val;
 int i;

 if(CensorHeader)
    for(i=0;i<CensorHeader->nentries;i++)
       if(!strcasecmp(CensorHeader->key[i].string,key))
          if(!CensorHeader->url[i] || MatchUrlSpecification(CensorHeader->url[i],Url->proto,Url->host,Url->port,Url->path,Url->args))
            {
             if(!CensorHeader->val[i].string)
                new=NULL;
             else if(!strcmp(CensorHeader->val[i].string,"yes"))
                new=NULL;
             else if(!strcmp(CensorHeader->val[i].string,"no"))
                ;
             else if(strcmp(CensorHeader->val[i].string,val))
               {
                new=(char*)malloc(strlen(CensorHeader->val[i].string)+1);
                strcpy(new,CensorHeader->val[i].string);
               }
             break;
            }

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide what mime type to apply for a given file.

  char *WhatMIMEType Returns the MIME Type.

  const char *path The path of the file.
  ++++++++++++++++++++++++++++++++++++++*/

char *WhatMIMEType(const char *path)
{
 char *mimetype=ConfigString(DefaultMIMEType);
 unsigned maxlen=0;
 int i;

 if(MIMETypes)
    for(i=0;i<MIMETypes->nentries;i++)
       if(strlen(path)>strlen(MIMETypes->key[i].string) &&
          strlen(MIMETypes->key[i].string)>maxlen &&
          !strcmp(MIMETypes->key[i].string,path+strlen(path)-strlen(MIMETypes->key[i].string)))
         {mimetype=MIMETypes->val[i].string;maxlen=strlen(mimetype);}

 return(mimetype);
}


/*++++++++++++++++++++++++++++++++++++++
  Get the URL that the current URL is aliased to.

  URL *GetAliasURL Returns a pointer to a new URL if the specified URL is aliased.

  const URL *Url The URL to check.
  ++++++++++++++++++++++++++++++++++++++*/

URL *GetAliasURL(const URL *Url)
{
 URL *newUrl=NULL;
 int i;

 if(Aliases)
    for(i=0;i<Aliases->nentries;i++)
      {
       char *alias_path=HasUrlSpecPath(Aliases->key[i].urlspec)?UrlSpecPath(Aliases->key[i].urlspec):"";

       if(MatchUrlSpecification(Aliases->key[i].urlspec,Url->proto,Url->host,Url->port,alias_path,NULL) &&
          !strncmp(alias_path,Url->path,strlen(alias_path)))
         {
          char *new_proto=NULL,*new_hostport=NULL,*new_path=NULL,*new_args=NULL;

          /* Sort out the aliased protocol. */

          if(HasUrlSpecProto(Aliases->val[i].urlspec))
             new_proto=UrlSpecProto(Aliases->val[i].urlspec);
          else
             new_proto=Url->proto;

          /* Sort out the aliased host. */

          if(HasUrlSpecHost(Aliases->val[i].urlspec))
            {
             new_hostport=(char*)malloc(strlen(UrlSpecHost(Aliases->val[i].urlspec))+1+MAX_INT_STR+1);
             strcpy(new_hostport,UrlSpecHost(Aliases->val[i].urlspec));
             if(UrlSpecPort(Aliases->val[i].urlspec)>0)
                sprintf(new_hostport+strlen(new_hostport),":%d",UrlSpecPort(Aliases->val[i].urlspec));
            }
          else
             new_hostport=Url->hostport;

          /* Sort out the aliased path. */

          if(HasUrlSpecPath(Aliases->val[i].urlspec))
            {
             int oldlen=strlen(alias_path);
             int newlen=strlen(UrlSpecPath(Aliases->val[i].urlspec));

             new_path=(char*)malloc(newlen-oldlen+strlen(Url->path)+2);
             strcpy(new_path,UrlSpecPath(Aliases->val[i].urlspec));

             if(*(UrlSpecPath(Aliases->val[i].urlspec)+newlen-1)!='/' && alias_path[oldlen-1]=='/')
                strcat(new_path,"/");
             if(*(UrlSpecPath(Aliases->val[i].urlspec)+newlen-1)=='/' && alias_path[oldlen-1]!='/')
                new_path[newlen-1]=0;

             strcat(new_path,Url->path+oldlen);
            }
          else
             new_path=Url->path;

          /* Sort out the aliased arguments. */

          if(Url->args && *Url->args=='!')
            {
             char *pling2=strchr(Url->args+1,'!');

             if(pling2)
                new_args=pling2+1;
             else
                new_args=NULL;
            }
          else
             new_args=Url->args;

          /* Create the new alias. */

          newUrl=CreateURL(new_proto,new_hostport,new_path,new_args,Url->user,Url->pass);

          if(new_hostport!=Url->hostport) free(new_hostport);
          if(new_path    !=Url->path)     free(new_path);

          break;
         }
     }

 return(newUrl);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the integer value that applies to a ConfigItem.

  int ConfigInteger Returns the integer value.

  ConfigItem item The configuration item to check.
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigInteger(ConfigItem item)
{
 if(!item)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries>1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries==1)
    return(item->val[0].integer);
 else
    return(item->def_val->integer);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the string value that applies to a ConfigItem.

  char *ConfigString Returns the string value.

  ConfigItem item The configuration item to check.
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigString(ConfigItem item)
{
 if(!item)
    return(NULL);

 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries>1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries==1)
    return(item->val[0].string);
 else
    return(item->def_val->string);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a ConfigItem to find an integer value that applies to the specified URL.

  int ConfigIntegerURL Returns the integer value.

  ConfigItem item The configuration item to check.

  const URL *Url the URL to check for a match against (or NULL to get the match for all URLs).
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigIntegerURL(ConfigItem item,const URL *Url)
{
 int i;

 if(!item)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->url_type!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 for(i=0;i<item->nentries;i++)
   {
    if(!item->url[i])
       return(item->val[i].integer);
    else if(Url && MatchUrlSpecification(item->url[i],Url->proto,Url->host,Url->port,Url->path,Url->args))
       return(item->val[i].integer);
   }

 return(item->def_val->integer);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a ConfigItem to find an string value that applies to the specified URL.

  char *ConfigStringURL Returns the string value.

  ConfigItem item The configuration item to check.

  const URL *Url the URL to check for a match against (or NULL to get the match for all URLs).
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigStringURL(ConfigItem item,const URL *Url)
{
 int i;

 if(!item)
    return(NULL);

 if(item->itemdef->url_type!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 for(i=0;i<item->nentries;i++)
   {
    if(!item->url[i])
       return(item->val[i].string);
    else if(Url && MatchUrlSpecification(item->url[i],Url->proto,Url->host,Url->port,Url->path,Url->args))
       return(item->val[i].string);
   }

 if(item->def_val)
    return(item->def_val->string);
 else
    return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified URL is listed in this configuration item.

  int ConfigBooleanMatchURL Return true if it is in the list.

  ConfigItem item The configuration item to match.

  const URL *Url The URL to search the list for.
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigBooleanMatchURL(ConfigItem item,const URL *Url)
{
 int i;

 if(!item)
    return(0);

 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 for(i=0;i<item->nentries;i++)
    if(MatchUrlSpecification(item->key[i].urlspec,Url->proto,Url->host,Url->port,Url->path,Url->args))
       return(!item->key[i].urlspec->negated);

 return(0);
}
