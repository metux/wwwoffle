/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/misc.h,v 2.63 2010-09-19 10:24:19 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9g.
  Miscellaneous HTTP / HTML functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997-2010 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef MISC_H
#define MISC_H    /*+ To stop multiple inclusions. +*/

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


/*+ The longest string needed to contain an integer. +*/
#define MAX_INT_STR 20  /* to hold a 64-bit unsigned long: 18446744073709551615ULL */


/*+ A URL data type. +*/
typedef struct _URL
{
 char *original_name;           /*+ The original URL that was split to create this structure. +*/

 char *original_pathp;          /*+ A pointer to the path in the original URL. +*/

 char *original_path;           /*+ The original path. +*/
 char *original_args;           /*+ The orignal arguments (also known as the query in RFCs). +*/

 char *name;                    /*+ The canonical URL for the object without the username/password. +*/

 char *file;                    /*+ The URL that is used for generating the filename with the username/password (may point to name). +*/

 char *hostp;                   /*+ A pointer to the host in the URL. +*/
 char *pathp;                   /*+ A pointer to the path in the URL. +*/

 char *proto;                   /*+ The protocol. +*/
 char *hostport;                /*+ The host name and port number. +*/
 char *path;                    /*+ The path. +*/
 char *args;                    /*+ The arguments (also known as the query in RFCs). +*/

 char *user;                    /*+ The username if supplied (or else NULL). +*/
 char *pass;                    /*+ The password if supplied (or else NULL). +*/

 char *host;                    /*+ The host only part without the port (may point to hostport). +*/
 int port;                      /*+ The port number if supplied (or else -1). +*/

 char *private_link;            /*+ A local URL for non-proxyable protocols (may point to name).  Private data. +*/

 char *private_dir;             /*+ The directory name for the host to avoid using ':' on Win32 (may point to hostport).  Private data. +*/
 char *private_file;            /*+ The hashed filename for the URL.  Private data. +*/
}
URL;


/*+ A request or reply header type. +*/
typedef struct _Header
{
 int type;                      /*+ The type of header, request=1 or reply=0. +*/

 char *method;                  /*+ The request method used. +*/
 char *url;                     /*+ The requested URL. +*/
 int status;                    /*+ The reply status. +*/
 char *note;                    /*+ The reply string. +*/
 char *version;                 /*+ The HTTP version. +*/

 int n;                         /*+ The number of header entries. +*/
 char **key;                    /*+ The name of the header line. +*/
 char **val;                    /*+ The value of the header line. +*/

 size_t size;                   /*+ The size of the header as read from the file/socket. +*/
}
Header;


/*+ A request or reply body type. +*/
typedef struct _Body
{
 size_t length;                 /*+ The length of the content. +*/

 char *content;                 /*+ The content itself. +*/
}
Body;


/*+ A header list item. +*/
typedef struct _HeaderListItem
{
 char *val;                     /*+ The string value. +*/
 float qval;                    /*+ The quality value. +*/
}
HeaderListItem;


/*+ A header value split into a list. +*/
typedef struct _HeaderList
{
 int n;                         /*+ The number of items in the list. +*/

 HeaderListItem *item;          /*+ The individual items (sorted into q value preference order). +*/
}
HeaderList;


/* in miscurl.c */

URL *SplitURL(const char *url);
URL /*@special@*/ *CreateURL(const char *proto,const char *hostport,const char *path,/*@null@*/ const char *args,/*@null@*/ const char *user,/*@null@*/ const char *pass) /*@allocates result@*/ /*@defines result->name,result->link,result->dir,result->file,result->hostp,result->pathp,result->proto,result->hostport,result->path,result->args,result->user,result->pass,result->host,result->port,result->private_link,result->private_dir,result->private_file@*/;
void FreeURL(/*@special@*/ URL *Url) /*@releases Url@*/;

void AddPasswordURL(URL *Url,/*@null@*/ const char *user,/*@null@*/ const char *pass);
URL /*@special@*/ *LinkURL(const URL *Url,const char *link) /*@allocates result@*/ /*@defines result->name,result->link,result->dir,result->file,result->hostp,result->pathp,result->proto,result->hostport,result->path,result->args,result->user,result->pass,result->host,result->port,result->private_link,result->private_dir,result->private_file@*/;

char /*@special@*/ *CanonicaliseHost(const char *host) /*@allocates result@*/;
void CanonicaliseName(char *name);


/* In miscencdec.c */

char /*@only@*/ *URLDecodeGeneric(const char *str);
char /*@only@*/ *URLDecodeFormArgs(const char *str);

char /*@only@*/ *URLRecodeFormArgs(const char *str);

char /*@only@*/ *URLEncodePath(const char *str);
char /*@only@*/ *URLEncodeFormArgs(const char *str);
char /*@only@*/ *URLEncodePassword(const char *str);

char /*@only@*/ **SplitFormArgs(const char *str);

char *TrimArgs(/*@returned@*/ char *str);

char /*@only@*/ *MakeHash(const char *args);

char /*@observer@*/ *RFC822Date(time_t t,int utc);
long DateToTimeT(const char *date);
char /*@observer@*/ *DurationToString(const time_t duration);

char /*@only@*/ *Base64Decode(const char *str,/*@out@*/ size_t *l);
char /*@only@*/ *Base64Encode(const char *str,size_t l);

char /*@only@*/ *FixHTMLLinkURL(const char *str);

char /*@only@*/* HTMLString(const char* str,int nbsp);


/* In headbody.c */

Header /*@special@*/ *CreateHeader(const char *line,int type) /*@allocates result,result->version,result->key,result->val@*/ /*@defines result->type,result->method,result->url,result->status,result->note,result->version,result->n,result->key,result->val,result->size@*/;

void AddToHeader(Header *head,/*@null@*/ const char *key,const char *val);
int AddToHeaderRaw(Header *head,char *line);

void ChangeURLInHeader(Header *head,const char *url);
void ChangeNoteInHeader(Header *head,const char *note);
void RemovePlingFromHeader(Header *head,const char *url);
void ChangeVersionInHeader(Header *head,const char *version);

void RemoveFromHeader(Header *head,const char* key);
void RemoveFromHeader2(Header *head,const char* key,const char *val);

char /*@null@*/ /*@observer@*/ *GetHeader(const Header *head,const char* key);
char /*@null@*/ /*@observer@*/ *GetHeader2(const Header *head,const char* key,const char *val);

char /*@only@*/ *HeaderString(const Header *head);

void FreeHeader(/*@special@*/ Header *head) /*@releases head@*/;

Body /*@only@*/ *CreateBody(int length);
void FreeBody(/*@special@*/ Body *body) /*@releases body@*/;

HeaderList /*@only@*/ *SplitHeaderList(char *val);
void FreeHeaderList(/*@special@*/ HeaderList *hlist) /*@releases hlist@*/;


#endif /* MISC_H */
