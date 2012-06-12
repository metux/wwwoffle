/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/headbody.c,v 1.26 2006-07-16 08:38:07 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Header and Body handling functions.
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

#include "misc.h"


static /*@only@*/ char **split_header_list(char *val);
static int sort_qval(const HeaderListItem *a,const HeaderListItem *b);


/*++++++++++++++++++++++++++++++++++++++
  Create a new Header structure.

  Header *CreateHeader Returns the new header structure.

  const char *line The top line in the original header.

  int type The type of header, request=1, reply=0;
  ++++++++++++++++++++++++++++++++++++++*/

Header *CreateHeader(const char *line,int type)
{
 Header *new=(Header*)malloc(sizeof(*new));
 char *p=(char*)malloc(strlen(line)+1),*oldp=p;
 char *method="",*url="",*note="",*version="";
 int status=0;

 /* Parse the original header. */

 strcpy(p,line);

 new->type=type;

 if(type==1)
   {
                                /* GET http://www/xxx HTTP/1.0\r\n */
    method=p;
    while(*p && !isspace(*p))   /*    ^                            */
       p++;
    if(!*p) goto eol_req;
    *p++=0;
    while(*p && isspace(*p))    /*     ^                           */
       p++;
    if(!*p) goto eol_req;
    url=p;
    while(*p && !isspace(*p))   /*                   ^             */
       p++;
    if(!*p) goto eol_req;
    *p++=0;
    while(*p && isspace(*p))    /*                    ^            */
       p++;
    if(!*p) goto eol_req;
    version=p;
    while(*p && !isspace(*p))   /*                            ^    */
       p++;
    *p=0;

   eol_req:

    if(!*version)
       version="HTTP/1.0";

    new->method=(char*)malloc(strlen(method)+1);
    strcpy(new->method,method);

    new->url=(char*)malloc(strlen(url)+1);
    strcpy(new->url,url);

    for(p=new->method;*p;p++)
       *p=toupper(*p);

    new->status=0;
    new->note=NULL;

    new->size=strlen(new->method)+strlen(new->url);
   }
 else
   {
                                /* HTTP/1.1 200 OK or something\r\n */
    version=p;
    while(*p && !isspace(*p))   /*         ^                        */
       p++;
    if(!*p) goto eol_rep;
    *p++=0;
    while(*p && isspace(*p))    /*          ^                       */
       p++;
    if(!*p) goto eol_rep;
    status=atoi(p);
    while(*p && isdigit(*p))    /*             ^                    */
       p++;
    if(!*p) goto eol_rep;
    *p++=0;
    while(*p && isspace(*p))    /*              ^                   */
       p++;
    if(!*p) goto eol_rep;
    note=p;
    while(*p && !iscntrl(*p))   /*                             ^    */
       p++;
    *p=0;

   eol_rep:

    new->method=NULL;
    new->url=NULL;

    new->status=status%1000;

    new->note=(char*)malloc(strlen(note)+1);
    strcpy(new->note,note);

    new->size=strlen(new->note)+3; /* 3 = strlen(status) */
   }

 new->version=(char*)malloc(strlen(version)+1);
 strcpy(new->version,version);

 for(p=new->version;*p;p++)
    *p=toupper(*p);

 new->size+=strlen(new->version)+4; /* 4 = 2*' ' + '\r\n' */

 new->size+=2; /* 2 = '\r\n' */

 new->n=0;
 new->key=(char**)malloc(sizeof(char*)*8);
 new->val=(char**)malloc(sizeof(char*)*8);

 free(oldp);

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Add a specified key and value to the header structure.

  Header *head The header structure to add to.

  const char *key The key to add or NULL.

  const char *val The value to add.
  ++++++++++++++++++++++++++++++++++++++*/

void AddToHeader(Header *head,const char *key,const char *val)
{
 if(key)
   {
    int i,match=-1;

    for(i=0;i<head->n;i++)
       if(!strcasecmp(head->key[i],key))
          match=i;

    if(match!=-1 && strcasecmp(key,"Set-Cookie") && strcasecmp(key,"Content-Type"))
      {
       /* Concatenate this value with an existing key. */

       head->val[match]=(char*)realloc((void*)head->val[match],strlen(head->val[match])+strlen(val)+3);
       strcat(head->val[match],",");
       strcat(head->val[match],val);

       head->size+=strlen(val)+1;
      }
    else
      {
       /* Add a new header line */

       if(head->n>=8)
         {
          head->key=(char**)realloc((void*)head->key,sizeof(char*)*(head->n+1));
          head->val=(char**)realloc((void*)head->val,sizeof(char*)*(head->n+1));
         }

       head->key[head->n]=(char*)malloc(strlen(key)+1);
       strcpy(head->key[head->n],key);

       head->val[head->n]=(char*)malloc(strlen(val)+1);
       strcpy(head->val[head->n],val);

       head->n++;

       head->size+=strlen(key)+strlen(val)+4;
      }
   }
 else
   {
    /* Append text to the last header line */

    if(head->n==0 || !head->key[head->n-1])
       return; /* weird: there must be a last header... */
    
    head->size+=strlen(val);
    head->val[head->n-1]=(char*)realloc((void*)head->val[head->n-1],strlen(head->val[head->n-1])+strlen(val)+1);
    if(*head->val[head->n-1])
       strcat(head->val[head->n-1],val);
    else
       strcat(head->val[head->n-1],val+1);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Add a raw line to a header.

  int AddToHeaderRaw Returns 1 if OK, -1 if malformed, else 0.

  Header *head The header to add the line to.

  char *line The raw line of data (modified in the process).
  ++++++++++++++++++++++++++++++++++++++*/

int AddToHeaderRaw(Header *head,char *line)
{
 char *key,*val,*r=line+strlen(line)-1;

 /* trim line */

 while(r>line && isspace(*r))
    *r--=0;

 /* last line */

 if(r==line)
    return(0);

 if(isspace(*line))
   {
    /* continuation of previous line - Wilmer van der Gaast <lintux@lintux.cx> */

    key=NULL;
    val=line;
    
    while(*val && isspace(*val))
       val++;
    *--val=' ';
   }
 else
   {
    /* split line */
   
    key=line;
    val=line;
   
    while(*val && *val!=':')
       val++;
   
    if(!*val)
       return(-1);				/* malformed header */

    r=val-1;
    while(r>line && isspace(*r))
       *r--=0;

    *val++=0;
    while(*val && isspace(*val))
       val++;
   }

 /* Add to the header */

 AddToHeader(head,key,val);

 return(1);
}


/*++++++++++++++++++++++++++++++++++++++
  Change the URL in the header.

  Header *head The header to change.

  const char *url The new URL.
  ++++++++++++++++++++++++++++++++++++++*/

void ChangeURLInHeader(Header *head,const char *url)
{
 head->size-=strlen(head->url);

 head->url=(char*)realloc((void*)head->url,strlen(url)+1);

 strcpy(head->url,url);

 head->size+=strlen(url);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the internal WWWOFFLE POST/PUT URL extensions.

  Header *head The header to remove the information from.

  const char *url A pointer to a string in the header.
  ++++++++++++++++++++++++++++++++++++++*/

void RemovePlingFromHeader(Header *head,const char *url)
{
 char *pling=strstr(url,"?!")+1;
 char *pling2=strchr(pling+1,'!');

 if(pling2)
    for(;pling<pling2;pling++)
       *pling=*(pling+1);

 head->size-=strlen(pling-1);
 *(pling-1)=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Change the note string in the header.

  Header *head The header to change.

  const char *note The new note.
  ++++++++++++++++++++++++++++++++++++++*/

void ChangeNoteInHeader(Header *head,const char *note)
{
 head->size-=strlen(head->note);

 head->note=(char*)realloc((void*)head->note,strlen(note)+1);

 strcpy(head->note,note);

 head->size+=strlen(note);
}


/*++++++++++++++++++++++++++++++++++++++
  Change the version string in the header.

  Header *head The header to change.

  const char *version The new version.
  ++++++++++++++++++++++++++++++++++++++*/

void ChangeVersionInHeader(Header *head,const char *version)
{
 head->size-=strlen(head->version);

 head->version=(char*)realloc((void*)head->version,strlen(version)+1);

 strcpy(head->version,version);

 head->size+=strlen(version);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the specified key and its values.

  Header *head The header to remove from.

  const char* key The key to look for and remove.
  ++++++++++++++++++++++++++++++++++++++*/

void RemoveFromHeader(Header *head,const char* key)
{
 int i,j;

 for(i=0,j=0;i<head->n;i++,j++)
    if(!strcasecmp(head->key[i],key))
      {
       head->size-=strlen(head->key[i])+strlen(head->val[i])+4;

       free(head->key[i]);
       free(head->val[i]);

       head->key[i]=NULL;
       head->val[i]=NULL;

       j--;
      }
    else if(i!=j)
      {
       head->key[j]=head->key[i];
       head->val[j]=head->val[i];

       head->key[i]=NULL;
       head->val[i]=NULL;
      }

 head->n=j;
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the specified key and value pair from a header structure.

  Header *head The header to remove from.

  const char* key The key to look for and remove.

  const char *val The value to look for and remove.
  ++++++++++++++++++++++++++++++++++++++*/

void RemoveFromHeader2(Header *head,const char* key,const char *val)
{
 int i;

 for(i=0;i<head->n;i++)
    if(!strcasecmp(head->key[i],key))
      {
       int match=0;
       char **list=split_header_list(head->val[i]);
       char *old=head->val[i],*new=(char*)malloc(strlen(head->val[i])+1);
       char *p=new,**l;

       *p=0;
          
       for(l=list;**l;l++)
          if(strncasecmp(*l,val,strlen(val)))
            {
             char *q=*l;

             if(p>new)
                *p++=',';

             while(q<*(l+1))
                *p++=*q++;

             *p=0;
             q--;

             while(p>new && (isspace(*q) || *q==','))
                *--p=0,q--;
            }
          else
             match++;

       if(match)
         {
          head->size+=strlen(new)-strlen(old);
          free(old);
          head->val[i]=new;

          if(!*new)
             RemoveFromHeader(head,key);
         }
       else
          free(new);

       free(list);
      }
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key.

  char *GetHeader Returns the value for the header key or NULL if none.

  const Header *head The header to search through.

  const char* key The key to look for.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeader(const Header *head,const char* key)
{
 int i;

 for(i=0;i<head->n;i++)
    if(!strcasecmp(head->key[i],key))
       return(head->val[i]);

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key and value pair.

  char *GetHeader2 Returns the value for the header key or NULL if none.

  const Header *head The header to search through.

  const char* key The key to look for.

  const char *val The value to look for (which may be in a list).
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeader2(const Header *head,const char* key,const char *val)
{
 char *retval=NULL;
 int i;

 for(i=0;i<head->n;i++)
    if(!strcasecmp(head->key[i],key))
      {
       char **list=split_header_list(head->val[i]),**l;

       for(l=list;**l;l++)
          if(!strncasecmp(*l,val,strlen(val)))
            {
             retval=*l;
             break;
            }

       free(list);

       break;
      }

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Return a string that contains the whole of the header.

  char *HeaderString Returns the header as a string.

  const Header *head The header structure to convert.
  ++++++++++++++++++++++++++++++++++++++*/

char *HeaderString(const Header *head)
{
 char *str,*p;
 int i;

 p=str=(char*)malloc(head->size+16);

 if(head->type==1)
   {
    strcpy(p,head->method);  p+=strlen(head->method);
    strcpy(p," ");           p++;
    strcpy(p,head->url);     p+=strlen(head->url);
    strcpy(p," ");           p++;
    strcpy(p,head->version); p+=strlen(head->version);
   }
 else
   {
    if(*head->version)
      {
       strcpy(p,head->version);       p+=strlen(head->version);
       strcpy(p," ");                 p++;
       if(head->status>=100 && head->status<1000)
          sprintf(p,"%3d",head->status);
       else
          strcpy(p,"200");
       p+=3;
       strcpy(p," ");                 p++;
       strcpy(p,head->note);          p+=strlen(head->note);
      }
    else
      {
       strcpy(p,"HTTP/1.0 200 OK"); p+=15;
      }
   }
 strcpy(p,"\r\n"); p+=2;

 for(i=0;i<head->n;i++)
   {
    strcpy(p,head->key[i]); p+=strlen(head->key[i]);
    strcpy(p,": ");         p+=2;
    strcpy(p,head->val[i]); p+=strlen(head->val[i]);
    strcpy(p,"\r\n");       p+=2;
   }

 strcpy(p,"\r\n");

 return(str);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a header structure.

  Header *head The header structure to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeHeader(Header *head)
{
 int i;

 if(head->type)
   {
    free(head->method);
    free(head->url);
   }
 else
    free(head->note);

 free(head->version);

 for(i=0;i<head->n;i++)
   {
    free(head->key[i]);
    free(head->val[i]);
   }

 free(head->key);
 free(head->val);

 free(head);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a new Body structure.

  Body *CreateBody Returns the new body structure.

  int length The length of the body;
  ++++++++++++++++++++++++++++++++++++++*/

Body *CreateBody(int length)
{
 Body *new=(Body*)malloc(sizeof(*new));

 new->length=length;

 new->content=malloc(length+3);

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a body structure.

  Body *body The body structure to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeBody(Body *body)
{
 if(body->content)
    free(body->content);

 free(body);
}


/*++++++++++++++++++++++++++++++++++++++
  Split a header that contains a list into a structure.

  HeaderList *SplitHeaderList Returns a structure containing a list of items and q values.

  char *val The header to split.
  ++++++++++++++++++++++++++++++++++++++*/

HeaderList *SplitHeaderList(char *val)
{
 char **list=split_header_list(val),**l;
 HeaderList *hlist=(HeaderList*)malloc(sizeof(HeaderList));

 hlist->n=0;
 hlist->item=(HeaderListItem*)malloc(8*sizeof(HeaderListItem));

 for(l=list;**l;l++)
   {
    char *p=*l,*q;
    float qval=1;

    while(p<*(l+1) && !isspace(*p) && *p!=',' && *p!=';')
       p++;

    q=p;

    while(p<*(l+1) && isspace(*p))
       p++;

    if(*p==';')
       sscanf(p+1," q=%f",&qval);

    if(hlist->n>=8)
       hlist->item=(HeaderListItem*)realloc((void*)hlist->item,sizeof(HeaderListItem)*(hlist->n+1));

    hlist->item[hlist->n].val=(char*)malloc((size_t)(q-*l+1));
    strncpy(hlist->item[hlist->n].val,*l,(size_t)(q-*l));
    hlist->item[hlist->n].val[q-*l]=0;
    hlist->item[hlist->n].qval=qval;
    hlist->n++;
   }

 qsort(hlist->item,(size_t)hlist->n,sizeof(HeaderListItem),(int (*)(const void*,const void*))sort_qval);

 free(list);

 return(hlist);
}


/*++++++++++++++++++++++++++++++++++++++
  Split a header that contains a list into items.

  char **split_header_list Returns an array of pointers into the header, terminated by a pointer to the NULL char.

  char *val The header to split.
  ++++++++++++++++++++++++++++++++++++++*/

static char **split_header_list(char *val)
{
 char *p=val;
 char **list=malloc(8*sizeof(char*));
 int nlist=0;

 while(*p)
   {
    while(*p && isspace(*p))
       p++;
    if(!*p)
       break;

    if(nlist>7)
       list=(char**)realloc((void*)list,(nlist+2)*sizeof(char*));

    list[nlist++]=p;

    while(*p && *p!=',')
       p++;

    if(*p==',')
       p++;
   }

 list[nlist]=p;

 return(list);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a header list.

  HeaderList *hlist The list to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeHeaderList(HeaderList *hlist)
{
 int i;

 for(i=0;i<hlist->n;i++)
    free(hlist->item[i].val);

 free(hlist->item);

 free(hlist);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the header list items to put the highest q value first.

  int sort_qval Returns the sort preference of a and b.

  const HeaderListItem *a The first header list item.

  const HeaderListItem *b The second header list item.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_qval(const HeaderListItem *a,const HeaderListItem *b)
{
 float aq=a->qval;
 float bq=b->qval;
 int chosen;

 chosen=1000*(bq-aq);

 return(chosen);
}
