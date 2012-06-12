/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/miscurl.c 2.108 2006/02/10 18:35:10 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Miscellaneous HTTP / HTML Url Handling functions.
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

#include "misc.h"
#include "config.h"
#include "proto.h"


/*++++++++++++++++++++++++++++++++++++++
  Split a URL into a protocol, hostname, path name and an argument list.

  URL *SplitURL Returns a URL structure containing the information.

  const char *url The name of the url to split.
  ++++++++++++++++++++++++++++++++++++++*/

URL *SplitURL(const char *url)
{
 URL *Url;
 char *proto,*user,*pass,*hostport,*path,*args;
 char *copyurl,*mallocurl=malloc(strlen(url)+2);
 char *colon,*slash,*at,*ques,*hash;

 copyurl=mallocurl;
 strcpy(copyurl,url);

 /* Remove any fragment identifiers */

 hash=strchr(copyurl,'#');
 if(hash)
    *hash=0;

 /* Protocol = Url->proto */

 colon=strchr(copyurl,':');
 slash=strchr(copyurl,'/');
 at   =strchr(copyurl,'@');

 if(slash==copyurl)                     /* /dir/... (local) */
   {
    proto="http";
   }
 else if(colon && slash && (colon+1)==slash) /* http:/[/]... */
   {
    *colon=0;
    proto=copyurl;

    copyurl=slash+1;
    if(*copyurl=='/')
       copyurl++;

    colon=strchr(copyurl,':');
    slash=strchr(copyurl,'/');
   }
 else if(colon && !isdigit(*(colon+1)) &&
         (!slash || colon<slash) &&
         (!at || (slash && at>slash)))  /* http:www.foo/...[:@]... */
   {
    *colon=0;
    proto=copyurl;

    copyurl=colon+1;

    colon=strchr(copyurl,':');
   }
 else                                   /* www.foo:80/... */
   {
    proto="http";
   }

 /* Username, Password = Url->user, Url->pass */

 if(at && at>copyurl && (!slash || slash>at))
   {
    char *at2;

    if(colon && at>colon)               /* user:pass@www.foo...[/]... */
      {
       *colon=0;
       user=copyurl;
       *at=0;
       pass=colon+1;

       copyurl=at+1;
      }
    else if(colon && (at2=strchr(at+1,'@')) && /* user@host:pass@www.foo...[/]... */
            at2>colon && at2<slash)            /* [not actually valid, but common]    */
      {
       *colon=0;
       user=copyurl;
       *at2=0;
       pass=colon+1;

       copyurl=at2+1;
      }
    else                               /* user@www.foo...[:/]... */
      {
       *at=0;
       user=copyurl;
       pass=NULL;

       copyurl=at+1;
      }
   }
 else
   {
    if(at==copyurl)             /* @www.foo... */
       copyurl++;

    user=NULL;
    pass=NULL;
   }

 /* Arguments = Url->args */

 ques=strchr(copyurl,'?');

 if(ques)                       /* ../path?... */
   {
    *ques++=0;
    args=ques;
   }
 else
    args=NULL;

 /* Pathname = Url->path */

 slash=strchr(copyurl,'/');

 if(slash)                       /* /path/... (local) */ /* www.foo/...[?]... */
   {
    path=(char*)malloc(strlen(slash)+1);
    strcpy(path,slash);
    *slash=0;
   }
 else                           /* www.foo[?]... */
    path="/";

 /* Hostname:port = Url->hostport */

 if(*copyurl)                   /* www.foo */
    hostport=copyurl;
 else                           /* /path/... (local) */
    hostport=GetLocalHostPort();

 /* Create the URL */

 Url=CreateURL(proto,hostport,path,args,user,pass);

 /* Tidy up */

 free(mallocurl);

 if(hostport!=copyurl)
    free(hostport);

 if(slash)
    free(path);

 return(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a URL data structure from a set of component parts.

  URL *CreateURL Returns a newly allocated URL.

  const char *proto The URL protocol.

  const char *hostport The URL host and port.

  const char *path The URL path.

  const char *args The URL args (may be NULL).

  const char *user The username (may be NULL);

  const char *pass The password (may be NULL);
  ++++++++++++++++++++++++++++++++++++++*/

URL *CreateURL(const char *proto,const char *hostport,const char *path,const char *args,const char *user,const char *pass)
{
 URL *Url=(URL*)malloc(sizeof(URL));
 int n=0,i;
 char *colon,*temp;

 /* proto = Url->proto */

 Url->proto=(char*)malloc(strlen(proto)+1);

 for(i=0;proto[i];i++)
    Url->proto[i]=tolower(proto[i]);
 Url->proto[i]=0;

 /* hostport = Url->hostport */

 temp=URLDecodeGeneric(hostport);

 for(i=0;temp[i];i++)
    if(isalpha(temp[i]))
       temp[i]=tolower(temp[i]);
    else if(!isalnum(temp[i]) && temp[i]!=':' && temp[i]!='-' &&
       temp[i]!='.' && temp[i]!='[' && temp[i]!=']')
      {temp[i]=0;break;}

 Url->hostport=CanonicaliseHost(temp);

 free(temp);

 /* path = Url->path */

 temp=URLDecodeGeneric(path);
 CanonicaliseName(temp);

 Url->path=URLEncodePath(temp);

 free(temp);

 /* args = Url->args */

 if(args && *args)
   {
    Url->args=URLRecodeFormArgs(args);
    URLReplaceAmp(Url->args);
   }
 else
    Url->args=NULL;

 /* user, pass = Url->user, Url->pass */

 if(user) /* allow empty usernames */
    Url->user=URLDecodeGeneric(user);
 else
    Url->user=NULL;

 if(pass && *pass)
    Url->pass=URLDecodeGeneric(pass);
 else
    Url->pass=NULL;

 /* Hostname, port = Url->host, Url->port */

 if(*Url->hostport=='[')
   {
    char *square=strchr(Url->hostport,']');
    colon=strchr(square,':');
   }
 else
    colon=strchr(Url->hostport,':');

 if(colon)
   {
    int defport=DefaultPort(Url);
    if(defport && atoi(colon+1)==defport)
       *colon=0;
   }

 if(colon && *colon)
   {
    Url->host=(char*)malloc(colon-Url->hostport+1);
    *colon=0;
    strcpy(Url->host,Url->hostport);
    *colon=':';
    Url->port=atoi(colon+1);
   }
 else
   {
    Url->host=Url->hostport;
    Url->port=-1;
   }

 /* Canonical URL = Url->name (and pointers Url->hostp, Url->pathp). */

 Url->name=(char*)malloc(strlen(Url->proto)+
                         strlen(Url->hostport)+
                         strlen(Url->path)+
                         (Url->args?strlen(Url->args):0)+
                         8);

 strcpy(Url->name,Url->proto);
 n=strlen(Url->proto);
 strcpy(Url->name+n,"://");
 n+=3;

 Url->hostp=Url->name+n;

 strcpy(Url->name+n,Url->hostport);
 n+=strlen(Url->hostport);

 Url->pathp=Url->name+n;

 strcpy(Url->name+n,Url->path);
 n+=strlen(Url->path);

 if(Url->args)
   {
    strcpy(Url->name+n,"?");
    strcpy(Url->name+n+1,Url->args);
   }

 /* File name = Url->file */

 if(Url->user)
   {
    char *encuserpass;

    Url->file=(char*)malloc(strlen(Url->name)+
                            3*strlen(Url->user)+
                            (Url->pass?3*strlen(Url->pass):0)+
                            8);

    n=Url->hostp-Url->name;
    strncpy(Url->file,Url->name,(size_t)n);

    encuserpass=URLEncodePassword(Url->user);

    strcpy(Url->file+n,encuserpass);
    n+=strlen(encuserpass);

    free(encuserpass);

    if(Url->pass)
      {
       encuserpass=URLEncodePassword(Url->pass);

       strcpy(Url->file+n,":");
       strcpy(Url->file+n+1,encuserpass);
       n+=strlen(encuserpass)+1;

       free(encuserpass);
      }

    strcpy(Url->file+n,"@");
    strcpy(Url->file+n+1,Url->hostp);
   }
 else
    Url->file=Url->name;

 /* Host directory = Url->private_dir - Private data */

 Url->private_dir=NULL;

 /* Cache filename = Url->private_file - Private data */

 Url->private_file=NULL;

 /* Proxyable link = Url->private_link - Private data */

 Url->private_link=NULL;

 return(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Free the memory in a URL.

  URL *Url The URL to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeURL(URL *Url)
{
 if(Url->private_dir && Url->private_dir!=Url->hostport)
    free(Url->private_dir);

 if(Url->private_file)
    free(Url->private_file);

 if(Url->private_link && Url->private_link!=Url->name)
    free(Url->private_link);

 if(Url->file!=Url->name)
    free(Url->file);

 free(Url->name);

 if(Url->host!=Url->hostport)
    free(Url->host);

 if(Url->proto)    free(Url->proto);
 if(Url->hostport) free(Url->hostport);
 if(Url->path)     free(Url->path);
 if(Url->args)     free(Url->args);

 if(Url->user)     free(Url->user);
 if(Url->pass)     free(Url->pass);

 free(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Add a password to an existing URL.

  URL *Url The URL to add the username and password to.

  const char *user The username.

  const char *pass The password.
  ++++++++++++++++++++++++++++++++++++++*/

void AddPasswordURL(URL *Url,const char *user,const char *pass)
{
 URL *new,*old;

 old=(URL*)malloc(sizeof(URL));
 *old=*Url;

 new=CreateURL(old->proto,old->hostport,old->path,old->args,user,pass);

 *Url=*new;

 FreeURL(old);
 free(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a url reference from a page to an absolute one.

  URL *LinkURL Returns a new URL that refers to the link.

  const URL *Url The page that we are looking at.

  const char *link The link from the page.
  ++++++++++++++++++++++++++++++++++++++*/

URL *LinkURL(const URL *Url,const char *link)
{
 URL *newUrl;
 char *new=NULL;
 char *colon=strchr(link,':');
 char *slash=strchr(link,'/');

 if(colon && slash && colon<slash)
    ;
 else if(*link=='#')
   {
    new=(char*)malloc(strlen(Url->name)+2);
    strcpy(new,Url->name);
   }
 else if(*link=='/' && *(link+1)=='/')
   {
    new=(char*)malloc(strlen(Url->proto)+strlen(link)+2);
    sprintf(new,"%s:%s",Url->proto,link);
   }
 else if(*link=='/')
   {
    new=(char*)malloc(strlen(Url->proto)+strlen(Url->hostport)+strlen(link)+4);
    sprintf(new,"%s://%s%s",Url->proto,Url->hostport,link);
   }
 else if(!strncasecmp(link,"mailto:",(size_t)7))
    /* "mailto:" doesn't follow the rule of ':' before '/'. */ ;
 else if(!strncasecmp(link,"javascript:",(size_t)11))
    /* "javascript:" doesn't follow the rule of ':' before '/'. */ ;
 else
   {
    int j;
    new=(char*)malloc(strlen(Url->proto)+strlen(Url->hostport)+strlen(Url->path)+strlen(link)+4);
    sprintf(new,"%s://%s%s",Url->proto,Url->hostport,Url->path);

    if(*link)
      {
       for(j=strlen(new)-1;j>0;j--)
          if(new[j]=='/')
             break;

       strcpy(new+j+1,link);

       CanonicaliseName(new+strlen(Url->proto)+3+strlen(Url->hostport));
      }
   }

 if(!new)
    newUrl=SplitURL(link);
 else
   {
    newUrl=SplitURL(new);
    free(new);
   }

 return(newUrl);
}


/*++++++++++++++++++++++++++++++++++++++
  Canonicalise a hostname by converting to lower case, filling in the zeros for IPv6 and converting decimal to dotted quad.

  char *CanonicaliseHost Returns the a newly allocated canonical string for the host.

  const char *host The original host address.
  ++++++++++++++++++++++++++++++++++++++*/

char *CanonicaliseHost(const char *host)
{
 char *newhost;
 int hasletter=0,hasdot=0,hascolon=0;
 int i;

 for(i=0;host[i];i++)
   {
    if(isalpha(host[i]))
       hasletter++;
    else if(host[i]=='.')
       hasdot++;
    else if(host[i]==':')
       hascolon++;
   }

 if(*host=='[' || hascolon>=2)
   {
    unsigned int ipv6[8],port=0;
    int cs=0,ce=7;
    const char *ps,*pe;
    newhost=(char*)malloc((size_t)48);

    ps=host;
    if(*host=='[')
       ps++;

    while(*ps && *ps!=':' && *ps!=']')
      {
       unsigned int ipv4[4];
       ipv4[0]=ipv4[1]=ipv4[2]=ipv4[3]=0;

       if(cs<=6 && sscanf(ps,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3])==4)
         {
          ipv6[cs++]=ipv4[0]*256+ipv4[1];
          ipv6[cs++]=ipv4[2]*256+ipv4[3];
         }
       else if(cs<=7 && sscanf(ps,"%x",&ipv6[cs])==1)
          cs++;
       else if(*(ps+1)==':' || *(ps+1)==']')
          break;

       while(*ps && *ps!=':' && *ps!=']')
          ps++;
       ps++;
      }

    pe=host+strlen(host)-1;

    if(*host=='[')
      {
       while(pe>host && *pe!=']')
          pe--;
       if(*(pe+1)==':')
          port=strtoul(pe+2,NULL,0);
       pe--;
      }

    if(*pe!=':')
       do
         {
          unsigned int ipv4[4];
          ipv4[0]=ipv4[1]=ipv4[2]=ipv4[3]=0;

          while(pe>host && *pe!=':')
             pe--;

          if(ce>=1 && sscanf(pe+1,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3])==4)
            {
             ipv6[ce--]=ipv4[2]*256+ipv4[3];
             ipv6[ce--]=ipv4[0]*256+ipv4[1];
            }
          else if(ce>=0 && sscanf(pe+1,"%x",&ipv6[ce])==1)
             ce--;
          else
             break;

          pe--;
         }
       while(pe>host && *pe!=':');

    for(;cs<=ce;cs++)
       ipv6[cs]=0;

    for(cs=0;cs<8;cs++)
       ipv6[cs]&=0xffff;

    if(port)
       sprintf(newhost,"[%x:%x:%x:%x:%x:%x:%x:%x]:%u",
               ipv6[0],ipv6[1],ipv6[2],ipv6[3],ipv6[4],ipv6[5],ipv6[6],ipv6[7],port&0xffff);
    else
       sprintf(newhost,"[%x:%x:%x:%x:%x:%x:%x:%x]",
               ipv6[0],ipv6[1],ipv6[2],ipv6[3],ipv6[4],ipv6[5],ipv6[6],ipv6[7]);
   }
 else if(hasletter)
   {
    newhost=(char*)malloc(strlen(host)+1);

    for(i=0;host[i];i++)
       newhost[i]=tolower(host[i]);

    newhost[i]=0;
   }
 else
   {
    unsigned int ipv4[4],port=0;
    char *colon=strchr(host,':');

    newhost=(char*)malloc((size_t)24);

    if(hasdot==0)
      {
       unsigned long decimal=strtoul(host,NULL,0);

       ipv4[3]=decimal&0xff; decimal>>=8;
       ipv4[2]=decimal&0xff; decimal>>=8;
       ipv4[1]=decimal&0xff; decimal>>=8;
       ipv4[0]=decimal&0xff;
      }
    else
      {
       ipv4[0]=ipv4[1]=ipv4[2]=ipv4[3]=0;
       sscanf(host,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3]);

       ipv4[0]&=0xff;
       ipv4[1]&=0xff;
       ipv4[2]&=0xff;
       ipv4[3]&=0xff;
      }

    if(colon && *(colon+1))
       port=strtoul(colon+1,NULL,0);

    if(port)
       sprintf(newhost,"%u.%u.%u.%u:%u",ipv4[0],ipv4[1],ipv4[2],ipv4[3],port&0xffff);
    else
       sprintf(newhost,"%u.%u.%u.%u",ipv4[0],ipv4[1],ipv4[2],ipv4[3]);
   }

 return(newhost);
}


/*++++++++++++++++++++++++++++++++++++++
  Canonicalise a file name by removing '/../', '/./' and '//' references.

  char *name The original name, returned modified inplace.

  The same function is used in WWWOFFLE and cxref with changes for files or URLs.
  ++++++++++++++++++++++++++++++++++++++*/

void CanonicaliseName(char *name)
{
 char *match,*name2;

 match=name;
 while((match=strstr(match,"/./")) || !strncmp(match=name,"./",(size_t)2))
   {
    char *prev=match, *next=match+2;
    while((*prev++=*next++));
   }

#if 0 /* as used in cxref */

 match=name;
 while((match=strstr(match,"//")))
   {
    char *prev=match, *next=match+1;
    while((*prev++=*next++));
   }

#endif

 match=name2=name;
 while((match=strstr(match,"/../")))
   {
    char *prev=match, *next=match+4;
    if((prev-name2)==2 && !strncmp(name2,"../",(size_t)3))
      {name2+=3;match++;continue;}
    while(prev>name2 && *--prev!='/');
    match=prev;
    if(*prev=='/')prev++;
    while((*prev++=*next++));
   }

 match=&name[strlen(name)-2];
 if(match>=name && !strcmp(match,"/."))
    *match=0;

 match=&name[strlen(name)-3];
 if(match>=name && !strcmp(match,"/.."))
   {
    if(match==name)
       *++match=0;
    else
       while(match>name && *--match!='/')
          *match=0;
   }

#if 0 /* as used in cxref */

 match=&name[strlen(name)-1];
 if(match>name && !strcmp(match,"/"))
    *match=0;

 if(!*name)
    *name='.',*(name+1)=0;

#else /* as used in wwwoffle */

 if(!*name || !strncmp(name,"../",(size_t)3))
    *name='/',*(name+1)=0;

#endif
}
