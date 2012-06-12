/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/parse.c 2.135 2007/09/08 18:56:08 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions to parse the HTTP requests.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
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

#include "wwwoffle.h"
#include "version.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"


/*+ The time that the program went online. +*/
time_t OnlineTime=0;

/*+ The time that the program went offline. +*/
time_t OfflineTime=0;

/*+ Headers from a request that can be re-used in automatically generated requests. +*/
static const char* const reusable_headers[]={"User-Agent",
                                             "Accept",
                                             "Accept-Charset",
                                             "Accept-Language",
                                             "From",
                                             "Proxy-Authorization"};

/*+ Headers that we do not allow the users to censor. +*/
static const char* const non_censored_headers[]={"Host",
                                                 "Connection",
                                                 "Proxy-Connection",
                                                 "Authorization"};

/*+ Headers that we cannot allow to be passed through WWWOFFLE. +*/
static const char* const deleted_http_headers[]={"If-Match",
                                                 "If-Range",
                                                 "Range",
                                                 "Upgrade",
                                                 "Keep-Alive",
                                                 "Accept-Encoding",
                                                 "TE"};

/*+ The headers from the request that are re-usable. +*/
static /*@only@*/ Header *reusable_header;


/*++++++++++++++++++++++++++++++++++++++
  Parse the request to the server.

  URL *ParseRequest Returns the URL or NULL if it failed.

  int fd The file descriptor to read the request from.

  Header **request_head Return the header of the request.

  Body **request_body Return the body of the request.
  ++++++++++++++++++++++++++++++++++++++*/

URL *ParseRequest(int fd,Header **request_head,Body **request_body)
{
 URL *Url=NULL;
 char *line=NULL,*val;
 unsigned i;

 *request_head=NULL;
 *request_body=NULL;

 reusable_header=CreateHeader("GET reusable HTTP/1.0\r\n",1);

 while((line=read_line(fd,line)))
   {
    if(!*request_head) /* first line */
      {
       *request_head=CreateHeader(line,1);

       if(!*(*request_head)->url)
          return(NULL);

       Url=SplitURL((*request_head)->url);

       continue;
      }

    if(!AddToHeaderRaw(*request_head,line))
       break;
   }

 /* Timeout or Connection lost? */
 
 if(!line || !Url || !*request_head)
   {PrintMessage(Warning,"Nothing to read from the wwwoffle proxy socket; timed out or connection lost? [%!s]."); return(NULL);}
 else
    free(line);
 
 /* Find re-usable headers (for recursive requests) */

 for(i=0;i<sizeof(reusable_headers)/sizeof(char*);i++)
    if((val=GetHeader(*request_head,reusable_headers[i])))
       AddToHeader(reusable_header,reusable_headers[i],val);

 /* Check for firewall operation. */

 if((val=GetHeader(*request_head,"Host")) && strcasecmp((*request_head)->method,"CONNECT"))
   {
    URL *oldUrl=Url;
    Url=CreateURL(oldUrl->proto,val,oldUrl->path,oldUrl->args,oldUrl->user,oldUrl->pass);
    FreeURL(oldUrl);
   }

 /* Check for passwords */

 if((val=GetHeader(*request_head,"Authorization")))
   {
    char *p,*userpass,*user,*pass;
    size_t l;

    p=val;
    while(*p && *p!=' ') p++;
    while(*p && *p==' ') p++;

    pass=user=userpass=Base64Decode(p,&l);
    while(*pass && *pass!=':') pass++;
    if(*pass)
       *pass++=0;

    AddPasswordURL(Url,user,pass);

    free(userpass);
   }

 if(!strcmp("POST",(*request_head)->method) ||
    !strcmp("PUT",(*request_head)->method))
   {
    URL *oldUrl;
    char *args,*hash;

    if((val=GetHeader(*request_head,"Content-Length")))
      {
       int length=atoi(val);

       if(length<0)
         {
          PrintMessage(Warning,"POST or PUT request must have a positive Content-Length header.");
          FreeURL(Url);
          return(NULL);
         }

       *request_body=CreateBody(length);

       if(length)
         {
          int m,l=length;

          do
            {
             m=read_data(fd,&(*request_body)->content[length-l],l);
            }
          while(m>0 && (l-=m));

          if(l)
            {
             PrintMessage(Warning,"POST or PUT request must have same data length as specified in Content-Length header (%d compared to %d).",length,length-l);
             FreeURL(Url);
             return(NULL);
            }
         }

       (*request_body)->content[length]=0;
      }
    else if(GetHeader2(*request_head,"Transfer-Encoding","chunked"))
      {
       int length=0,m=0;

       PrintMessage(Debug,"Client has used chunked encoding.");
       configure_io_chunked(fd,1,-1);

       *request_body=CreateBody(0);

       do
         {
          length+=m;
          (*request_body)->length=length+IO_BUFFER_SIZE;
          (*request_body)->content=(char*)realloc((void*)(*request_body)->content,(size_t)((*request_body)->length+3));
          m=read_data(fd,&(*request_body)->content[length],IO_BUFFER_SIZE);
         }
       while(m>0);

       (*request_body)->content[length]=0;
       (*request_body)->length=length;
      }
    else
      {
       PrintMessage(Warning,"POST or PUT request must have Content-Length header or use chunked encoding.");
       FreeURL(Url);
       return(NULL);
      }

    hash=MakeHash((*request_body)->content);
    args=(char*)malloc(strlen((*request_head)->method)+strlen(hash)+12);
    sprintf(args,"!%s:%s.%08lx",(*request_head)->method,hash,(long)time(NULL));
    free(hash);

    if(Url->args)
      {
       char *newargs=(char*)malloc(strlen(args)+strlen(Url->args)+2);
       strcpy(newargs,"!");
       strcat(newargs,Url->args);
       strcat(newargs,args);
       free(args);
       args=newargs;
      }

    oldUrl=Url;
    Url=CreateURL(oldUrl->proto,oldUrl->hostport,oldUrl->path,args,oldUrl->user,oldUrl->pass);
    FreeURL(oldUrl);

    free(args);
   }

 return(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if a forced refresh of the URL is required based on the header from the client.

  int RequireForced Returns 1 if the page should be refreshed.

  const Header *request_head The head of the HTTP request to modify.

  const URL *Url The URL that is being requested.

  int online The online or offline status.
  ++++++++++++++++++++++++++++++++++++++*/

int RequireForced(const Header *request_head,const URL *Url,int online)
{
 int retval=0;

 if(online)
   {
    if(ConfigBooleanURL(PragmaNoCacheOnline,Url) &&
       GetHeader2(request_head,"Pragma","no-cache"))
      {
       PrintMessage(Debug,"Requesting URL (Pragma: no-cache).");
       retval=1;
      }
    else if(ConfigBooleanURL(CacheControlNoCacheOnline,Url) &&
       GetHeader2(request_head,"Cache-Control","no-cache"))
      {
       PrintMessage(Debug,"Requesting URL (Cache-Control: no-cache).");
       retval=1;
      }
    else if(ConfigBooleanURL(CacheControlMaxAge0Online,Url) &&
       GetHeader2(request_head,"Cache-Control","max-age=0"))
      {
       PrintMessage(Debug,"Requesting URL (Cache-Control: max-age=0).");
       retval=1;
      }
    if(ConfigBooleanURL(CookiesForceRefreshOnline,Url) &&
       GetHeader(request_head,"Cookie"))
      {
       PrintMessage(Debug,"Requesting URL (Cookie:).");
       retval=1;
      }
   }
 else
   {
    if(ConfigBooleanURL(PragmaNoCacheOffline,Url) &&
       GetHeader2(request_head,"Pragma","no-cache"))
      {
       PrintMessage(Debug,"Requesting URL (Pragma: no-cache).");
       retval=1;
      }
    else if(ConfigBooleanURL(CacheControlNoCacheOffline,Url) &&
       GetHeader2(request_head,"Cache-Control","no-cache"))
      {
       PrintMessage(Debug,"Requesting URL (Cache-Control: no-cache).");
       retval=1;
      }
    else if(ConfigBooleanURL(CacheControlMaxAge0Offline,Url) &&
       GetHeader2(request_head,"Cache-Control","max-age=0"))
      {
       PrintMessage(Debug,"Requesting URL (Cache-Control: max-age=0).");
       retval=1;
      }
   }

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Modify the request to ask for changes since the spooled file.

  int RequireChanges Returns 1 if the file needs changes made, 0 if not.

  int fd The file descriptor of the spooled file.

  Header *request_head The head of the HTTP request to modify.

  const URL *Url The URL that is being requested.
  ++++++++++++++++++++++++++++++++++++++*/

int RequireChanges(int fd,Header *request_head,const URL *Url)
{
 struct stat buf;
 int status,retval=0;
 Header *spooled_head=NULL;
 time_t now=time(NULL);

 status=ParseReply(fd,&spooled_head);

 if(status==0 || fstat(fd,&buf))
   {
    PrintMessage(Debug,"Requesting URL (Empty or no status).");
    retval=1;
   }
 else if(status<200 || status>=400)
   {
    PrintMessage(Debug,"Requesting URL (Error status (%d)).",status);
    retval=1;
   }
 else
   {
    if(ConfigBooleanURL(RequestExpired,Url))
      {
       char *expires,*cachecontrol,*date;

       if((cachecontrol=GetHeader2(spooled_head,"Cache-Control","max-age")) &&
          (date=GetHeader(spooled_head,"Date")))
          {
           time_t then=DateToTimeT(date);
           long maxage;

           while(*cachecontrol && *cachecontrol!='=')
              cachecontrol++;
           cachecontrol++;

           maxage=atol(cachecontrol);

           if((now-then)>maxage)
             {
              PrintMessage(Debug,"Requesting URL (Cache-Control expiry time of %s from '%s').",DurationToString(maxage),date);
              retval=1;
             }
          }
       else if((expires=GetHeader(spooled_head,"Expires")))
         {
          time_t when=DateToTimeT(expires);

          if(when<=now)
            {
             PrintMessage(Debug,"Requesting URL (Expiry date of '%s').",expires);
             retval=1;
            }
         }
      }

    if(retval==0 && (status==302 || status==303 || status==307) && ConfigBooleanURL(RequestRedirection,Url))
      {
       PrintMessage(Debug,"Requesting URL (Redirection status %d).",status);
       retval=1;
      }

    if(retval==0 && ConfigBooleanURL(RequestNoCache,Url))
      {
       char *head,*val;

       if(GetHeader2(spooled_head,head="Pragma"       ,val="no-cache") ||
          GetHeader2(spooled_head,head="Cache-Control",val="no-cache"))
         {
          PrintMessage(Debug,"Requesting URL (No cache header '%s: %s').",head,val);
          retval=1;
         }
      }

    if(retval==0)
      {
       time_t requestchanged=ConfigIntegerURL(RequestChanged,Url);

       if(ConfigBooleanURL(RequestChangedOnce,Url) && buf.st_mtime>OnlineTime)
         {
          PrintMessage(Debug,"Not requesting URL (Only once per online session).");
          retval=0;
         }
       else if(requestchanged<0 || (now-buf.st_mtime)<requestchanged)
         {
          PrintMessage(Debug,"Not requesting URL (Last changed %s ago, config is %s).",
                       DurationToString(now-buf.st_mtime),DurationToString(requestchanged));
          retval=0;
         }
       else if(!ConfigBooleanURL(RequestConditional,Url))
          retval=1;
       else
         {
          char *etag,*lastmodified;

          if(ConfigBooleanURL(ValidateWithEtag,Url) && (etag=GetHeader(spooled_head,"Etag")))
            {
             AddToHeader(request_head,"If-None-Match",etag);

             PrintMessage(Debug,"Requesting URL (Conditional request with If-None-Match).");
            }

          if((lastmodified=GetHeader(spooled_head,"Last-Modified")))
            {
             AddToHeader(request_head,"If-Modified-Since",lastmodified);

             PrintMessage(Debug,"Requesting URL (Conditional request with If-Modified-Since).");
            }
          else
            {
             AddToHeader(request_head,"If-Modified-Since",RFC822Date(buf.st_mtime,1));

             PrintMessage(Debug,"Requesting URL (Conditional request with If-Modified-Since).");
            }

          retval=1;
         }
      }
   }

 if(spooled_head)
    FreeHeader(spooled_head);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the spooled page has been modified for a conditional request.

  int IsModified Returns 1 if the file has been modified, 0 if not.

  int fd The file descriptor of the spooled file.

  const Header *request_head The head of the HTTP request to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsModified(int fd,const Header *request_head)
{
 int is_modified=1;
 Header *spooled_head=NULL;

 ParseReply(fd,&spooled_head);

 if(spooled_head)
   {
    int check_time=1;

    /* Check the entity tags */

    char *inm_val=GetHeader(request_head,"If-None-Match");
    char *etag_val=GetHeader(spooled_head,"Etag");

    if(etag_val && inm_val)
      {
       HeaderList *list=SplitHeaderList(inm_val);
       int i;

       check_time=0;

       for(i=0;i<list->n;i++)
         {
          if(*list->item[i].val=='*')
            {is_modified=0;check_time=1;}
          else if(!strcmp(etag_val,list->item[i].val))
            {is_modified=0;check_time=1;}
         }

       FreeHeaderList(list);
      }

    /* Check the If-Modified-Since header if there are no matching Etags */

    if(check_time)
      {
       char *ims_val=GetHeader(request_head,"If-Modified-Since");

       if(ims_val)
         {
          time_t since=DateToTimeT(ims_val);
          char *modified=GetHeader(spooled_head,"Last-Modified");

          if(modified)
            {
             time_t modtime=DateToTimeT(modified);

             if(since>=modtime && modtime)
                is_modified=0;
             else
                is_modified=1;
            }
          else
            {
             struct stat buf;

             if(!fstat(fd,&buf))
               {
                time_t modtime=buf.st_mtime;

                if(since>=modtime)
                   is_modified=0;
                else
                   is_modified=1;
               }
            }
         }
      }

    FreeHeader(spooled_head);
   }

 return(is_modified);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the location that the URL has been moved to.

  URL *MovedLocation Returns the new URL.

  const URL *Url The original URL.

  const Header *reply_head The head of the original HTTP reply.
  ++++++++++++++++++++++++++++++++++++++*/

URL *MovedLocation(const URL *Url,const Header *reply_head)
{
 char *location;
 URL *new;

 location=GetHeader(reply_head,"Location");

 if(!location)
    return(NULL);

 new=LinkURL(Url,location);

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a new request for a page.

  Header *RequestURL Ask for a page.

  const URL *Url The URL to get.

  const URL *refererUrl The Refering URL or NULL if none.
  ++++++++++++++++++++++++++++++++++++++*/

Header *RequestURL(const URL *Url,const URL *refererUrl)
{
 char *top=(char*)malloc(strlen(Url->name)+32);
 Header *new;
 int i;

 sprintf(top,"GET %s HTTP/1.0\r\n",Url->name);
 new=CreateHeader(top,1);
 free(top);

 if(Url->user)
   {
    char *userpass=(char*)malloc(strlen(Url->user)+(Url->pass?strlen(Url->pass):0)+2);
    char *auth=(char*)malloc((strlen(Url->user)+(Url->pass?strlen(Url->pass):0))*2+16);

    strcpy(userpass,Url->user);
    strcat(userpass,":");
    if(Url->pass)
       strcat(userpass,Url->pass);

    sprintf(auth,"Basic %s",Base64Encode(userpass,strlen(userpass)));
    AddToHeader(new,"Authorization",auth);

    free(userpass);
    free(auth);
   }

 if(refererUrl)
    AddToHeader(new,"Referer",refererUrl->name);

 if(reusable_header->n)
    for(i=0;i<reusable_header->n;i++)
       AddToHeader(new,reusable_header->key[i],reusable_header->val[i]);

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Finish with the reusable headers.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishParse(void)
{
 if(reusable_header)
    FreeHeader(reusable_header);
}


/*++++++++++++++++++++++++++++++++++++++
  Modify the request taking into account censoring of header and modified URL.

  const URL *Url The actual URL.

  Header *request_head The head of the HTTP request possibly with a different URL.
  ++++++++++++++++++++++++++++++++++++++*/

void ModifyRequest(const URL *Url,Header *request_head)
{
 int i;
 unsigned j;
 char *referer=NULL;

 /* Modify the top line of the header. */

 ChangeURLInHeader(request_head,Url->name);

 /* Remove the false arguments from POST/PUT URLs. */

 if(!strcmp(request_head->method,"POST") ||
    !strcmp(request_head->method,"PUT"))
    RemovePlingFromHeader(request_head,request_head->url);

 /* Add a host header */

 RemoveFromHeader(request_head,"Host");

 AddToHeader(request_head,"Host",Url->hostport);

 /* Add a Connection / Proxy-Connection header */

 RemoveFromHeader(request_head,"Connection");
 RemoveFromHeader(request_head,"Proxy-Connection");

 if(!strcmp(request_head->version,"HTTP/1.1"))
    strcpy(request_head->version,"HTTP/1.0");

 AddToHeader(request_head,"Connection","close");
 AddToHeader(request_head,"Proxy-Connection","close");

 /* Check the authorisation header. */

 RemoveFromHeader(request_head,"Authorization");

 if(Url->user)
   {
    char *userpass=(char*)malloc(strlen(Url->user)+(Url->pass?strlen(Url->pass):0)+2);
    char *auth=(char*)malloc((strlen(Url->user)+(Url->pass?strlen(Url->pass):0))*2+16);

    strcpy(userpass,Url->user);
    strcat(userpass,":");
    if(Url->pass)
       strcat(userpass,Url->pass);

    sprintf(auth,"Basic %s",Base64Encode(userpass,strlen(userpass)));
    AddToHeader(request_head,"Authorization",auth);

    free(userpass);
    free(auth);
   }

 /* Remove some headers */

 for(j=0;j<sizeof(deleted_http_headers)/sizeof(char*);j++)
    RemoveFromHeader(request_head,deleted_http_headers[j]);

 RemoveFromHeader2(request_head,"Pragma","wwwoffle");

 /* Fix the Referer header */

 if((referer=GetHeader(request_head,"Referer")))
    if(strstr(referer,"?!"))
       RemovePlingFromHeader(request_head,referer);

 if(ConfigBooleanURL(RefererSelfDir,Url))
   {
    char *urldir=(char*)malloc(strlen(Url->name)+1),*p;

    strcpy(urldir,Url->name);

    if(!(p=strchr(urldir,'?')))
       p=urldir+strlen(Url->name)-1;

    while(*p!='/')
       p--;
    *(p+1)=0;

    PrintMessage(Debug,"CensorRequestHeader (RefererSelfDir) replaced '%s' by '%s'.",referer?referer:"(none)",urldir);
    RemoveFromHeader(request_head,"Referer");
    AddToHeader(request_head,"Referer",urldir);

    free(urldir);
   }
 else if(ConfigBooleanURL(RefererSelf,Url))
   {
    PrintMessage(Debug,"CensorRequestHeader (RefererSelf) replaced '%s' by '%s'.",referer?referer:"(none)",Url->name);
    RemoveFromHeader(request_head,"Referer");
    AddToHeader(request_head,"Referer",Url->name);
   }

 if((referer=GetHeader(request_head,"Referer")))
   {
    URL *refurl=SplitURL(referer);

    if(ConfigBooleanURL(RefererFrom,refurl))
      {
       PrintMessage(Debug,"CensorRequestHeader (RefererFrom) removed '%s'.",referer);
       RemoveFromHeader(request_head,"Referer");
      }

    FreeURL(refurl);
   }

 /* Force the insertion of a User-Agent header */

 if(ConfigBooleanURL(ForceUserAgent,Url) && !GetHeader(request_head,"User-Agent"))
   {
    PrintMessage(Debug,"CensorRequestHeader (ForceUserAgent) inserted '%s'.","WWWOFFLE/" WWWOFFLE_VERSION);
    AddToHeader(request_head,"User-Agent","WWWOFFLE/" WWWOFFLE_VERSION);
   }

 /* Censor the header */

 for(i=0;i<request_head->n;i++)
   {
    char *censor;

    for(j=0;j<sizeof(non_censored_headers)/sizeof(char*);j++)
       if(!strcasecmp(non_censored_headers[j],request_head->key[i]))
          break;

    if(j!=sizeof(non_censored_headers)/sizeof(char*))
       continue;

    if((censor=CensoredHeader(Url,request_head->key[i],request_head->val[i])))
      {
       if(censor!=request_head->val[i])
         {
          PrintMessage(Debug,"CensorRequestHeader replaced '%s: %s' by '%s: %s'.",request_head->key[i],request_head->val[i],request_head->key[i],censor);
          request_head->size+=strlen(censor)-strlen(request_head->val[i]);
          free(request_head->val[i]);
          request_head->val[i]=censor;
         }
      }
    else
      {
       PrintMessage(Debug,"CensorRequestHeader removed '%s: %s'.",request_head->key[i],request_head->val[i]);
       RemoveFromHeader(request_head,request_head->key[i]);
       i--;
      }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Change the request to one that contains an authorisation string if required.

  const URL *proxyUrl The URL of the proxy.

  Header *request_head The HTTP request head.
  ++++++++++++++++++++++++++++++++++++++*/

void MakeRequestProxyAuthorised(const URL *proxyUrl,Header *request_head)
{
 RemoveFromHeader(request_head,"Proxy-Authorization");

 if(ProxyAuthUser && ProxyAuthPass)
   {
    char *user,*pass;

    user=ConfigStringURL(ProxyAuthUser,proxyUrl);
    pass=ConfigStringURL(ProxyAuthPass,proxyUrl);

    if(user && pass)
      {
       char *userpass=(char*)malloc(strlen(user)+strlen(pass)+2);
       char *auth=(char*)malloc(2*strlen(user)+2*strlen(pass)+8);

       sprintf(userpass,"%s:%s",user,pass);
       sprintf(auth,"Basic %s",Base64Encode(userpass,strlen(userpass)));
       AddToHeader(request_head,"Proxy-Authorization",auth);

       free(userpass);
       free(auth);
      }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Change the request from one to a proxy to a normal one.

  const URL *Url The URL of the request.

  Header *request_head The head of the HTTP request.
  ++++++++++++++++++++++++++++++++++++++*/

void MakeRequestNonProxy(const URL *Url,Header *request_head)
{
 /* Remove the full URL and replace it with just the path and args. */

 ChangeURLInHeader(request_head,Url->pathp);

 /* Remove the false arguments from POST/PUT URLs. */

 if(!strcmp(request_head->method,"POST") ||
    !strcmp(request_head->method,"PUT"))
    RemovePlingFromHeader(request_head,request_head->url);

 /* Remove the proxy connection & authorization headers. */

 RemoveFromHeader(request_head,"Proxy-Connection");
 RemoveFromHeader(request_head,"Proxy-Authorization");
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the reply from the server.

  int ParseReply Return the numeric status of the reply.

  int fd The file descriptor to read from.

  Header **reply_head Return the head of the HTTP reply.
  ++++++++++++++++++++++++++++++++++++++*/

int ParseReply(int fd,Header **reply_head)
{
 char *line=NULL;

 *reply_head=NULL;

 while((line=read_line(fd,line)))
   {
    if(!*reply_head)   /* first line */
      {
       *reply_head=CreateHeader(line,0);

       if(!*(*reply_head)->version)
          break;
       continue;
      }

    if(!AddToHeaderRaw(*reply_head,line))
       break;
   }

 if(!line)
   return(0);

 free(line);

 if(*reply_head)
    return((*reply_head)->status);
 else
    return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Find the status of a spooled page.

  int SpooledPageStatus Returns the status number.

  URL *Url The URL to check.

  int backup A flag to indicate that the backup file is to be used.
  ++++++++++++++++++++++++++++++++++++++*/

int SpooledPageStatus(URL *Url,int backup)
{
 int status=0;
 int spool;

 if(backup)
    spool=OpenBackupWebpageSpoolFile(Url);
 else
    spool=OpenWebpageSpoolFile(1,Url);

 if(spool!=-1)
   {
    char *reply;

    init_io(spool);

    reply=read_line(spool,NULL);

    if(reply)
      {
       sscanf(reply,"%*s %d",&status);
       free(reply);
      }

    finish_io(spool);
    close(spool);
   }

 return(status);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide which compression that we can use for the reply to the client.

  int WhichCompression Returns the compression method, 1 for deflate and 2 for gzip.

  char *content_encoding The string representing the content encoding the client/server used.
  ++++++++++++++++++++++++++++++++++++++*/

int WhichCompression(char *content_encoding)
{
 int retval=0;
 HeaderList *list=SplitHeaderList(content_encoding);
 float q_deflate=0,q_gzip=0,q_identity=1;
 int i;

 for(i=0;i<list->n;i++)
    if(list->item[i].qval>0)
      {
       if(!strcmp(list->item[i].val,"deflate") || !strcmp(list->item[i].val,"x-deflate"))
          q_deflate=list->item[i].qval;
       else if(!strcmp(list->item[i].val,"gzip") || !strcmp(list->item[i].val,"x-gzip"))
          q_gzip=list->item[i].qval;
       else if(!strcmp(list->item[i].val,"identity"))
          q_identity=list->item[i].qval;
      }

 FreeHeaderList(list);

 /* Deflate is a last resort, see comment in iozlib.c. */

 if(q_identity>q_gzip && q_identity>q_deflate)
    retval=0;
 else if(q_gzip>0)
    retval=2;
 else if(q_deflate>0)
    retval=1;

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Modify the reply taking into account censoring of the header.

  const URL *Url The URL that this reply comes from.

  Header *reply_head The head of the HTTP reply.
  ++++++++++++++++++++++++++++++++++++++*/

void ModifyReply(const URL *Url,Header *reply_head)
{
 int i;

 /* Add a Connection / Proxy-Connection header */

 RemoveFromHeader(reply_head,"Connection");
 RemoveFromHeader(reply_head,"Proxy-Connection");

 if(!strcmp(reply_head->version,"HTTP/1.1"))
    strcpy(reply_head->version,"HTTP/1.0");

 AddToHeader(reply_head,"Connection","close");
 AddToHeader(reply_head,"Proxy-Connection","close");

 /* Send errors instead when we see Location headers that send the client to a blocked page. */
 
 if(ConfigBooleanURL(DontGetLocation,Url))
   {
    char *location;

    if((location=GetHeader(reply_head,"Location")))
      {
       URL *locUrl=SplitURL(location);
 
       if(ConfigBooleanMatchURL(DontGet,locUrl))
         {
          reply_head->status=404;
          RemoveFromHeader(reply_head,"Location");
         }
 
       FreeURL(locUrl);
      }
   }
 
 /* Censor the header */

 for(i=0;i<reply_head->n;i++)
   {
    int j;
    char *censor;

    for(j=0;j<sizeof(non_censored_headers)/sizeof(char*);j++)
       if(!strcasecmp(non_censored_headers[j],reply_head->key[i]))
          break;

    if(j!=sizeof(non_censored_headers)/sizeof(char*))
       continue;

    if((censor=CensoredHeader(Url,reply_head->key[i],reply_head->val[i])))
      {
       if(censor!=reply_head->val[i])
         {
          PrintMessage(Debug,"CensorReplyHeader replaced '%s: %s' by '%s: %s'.",reply_head->key[i],reply_head->val[i],reply_head->key[i],censor);
          reply_head->size+=strlen(censor)-strlen(reply_head->val[i]);
          free(reply_head->val[i]);
          reply_head->val[i]=censor;
         }
      }
    else
      {
       PrintMessage(Debug,"CensorReplyHeader removed '%s: %s'.",reply_head->key[i],reply_head->val[i]);
       RemoveFromHeader(reply_head,reply_head->key[i]);
       i--;
      }
   }
}
