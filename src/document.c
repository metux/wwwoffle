/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/document.c 1.28 2007/03/25 11:06:03 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Document parsing functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02,03,04,05 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "wwwoffle.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "proto.h"
#include "document.h"


/*+ The list of links to references. +*/
static char **reference_links[NRefTypes];

/*+ The list of URL references. +*/
static URL **reference_Urls[NRefTypes];

/*+ The number of references. +*/
static int reference_num[NRefTypes];

/*+ The base URL from which references are related. +*/
static URL *baseUrl;

/*+ A flag to indicate that all references are to be added. +*/
static int add_all=0;


static DocType GetDocumentType(const char *mimetype);
static /*@only@*/ char *GetMIMEType(int fd);


/*++++++++++++++++++++++++++++++++++++++
  Parse a document.

  DocType ParseDocument Return 1 if there was anything that could be parsed.

  int fd The file descriptor to read the document from.

  URL *Url The URL of the document.

  int all Set to 1 to match all references, not just the protocols that WWWOFFLE knows.
  ++++++++++++++++++++++++++++++++++++++*/

DocType ParseDocument(int fd,URL *Url,int all)
{
 char *mimetype,*getmimetype;
 DocType doctype=DocUnknown;

 baseUrl=Url;
 add_all=all;

 /* Choose the DocType based on the file's MIME-Type header */

 getmimetype=GetMIMEType(fd);

 if(getmimetype!=NULL)
   {
    mimetype=getmimetype;
    doctype=GetDocumentType(mimetype);
   }

 /* Choose the DocType based on the file's extension. */

 if(doctype==DocUnknown)
   {
    /* Get MIME-Type from extension. */
    mimetype=WhatMIMEType(Url->path);
    doctype=GetDocumentType(mimetype);
   }

 /* Parse the document if we do know the DocType. */

 if(doctype!=DocUnknown)
   {
    PrintMessage(Debug,"Parsing document of MIME Type '%s'.",mimetype);

    /* Free previous references. */

    ResetReferences();

    if(doctype==DocHTML)
       ParseHTML(fd,Url);
    else if(doctype==DocCSS)
       ParseCSS(fd,Url);
    else if(doctype==DocJavaClass)
       InspectJavaClass(fd,Url);
    else if(doctype==DocXML)
       ParseXML(fd,Url);
    else if(doctype==DocVRML)
       ParseVRML(fd,Url);

    /* Put a trailing NULL on the references. */
 
    FinishReferences();
   }

 if(getmimetype)
    free(getmimetype);

 return(doctype);
}

static const struct {
  char *mimetype;
  DocType doctype;
} docTypeList[] = {
	{"text/html",DocHTML},
	{"application/xhtml+xml",DocHTML},
	{"text/css",DocCSS},
	{"application/java",DocJavaClass},
	{"text/xml",DocXML},
	{"application/xml",DocXML},
	{"x-world/x-vrml",DocVRML},
	{"model/vrml",DocVRML},
	{"",DocUnknown}
};


/*++++++++++++++++++++++++++++++++++++++
  Decide the current document type based on the mime type.

  DocType GetDocumentType Returns the document type.

  const char *mimetype The mime type to be tested.
  ++++++++++++++++++++++++++++++++++++++*/

static DocType GetDocumentType(const char *mimetype)
{
 unsigned i;

 for(i = 0; i < sizeof(docTypeList)/sizeof(docTypeList[0]); i++)
    if(!strcmp(mimetype,docTypeList[i].mimetype))
       return(docTypeList[i].doctype);

 return(DocUnknown);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide the mime type of a document based on the header.

  char *GetMIMEType Returns the mime type.

  int fd The file descriptor to read the header from.
  ++++++++++++++++++++++++++++++++++++++*/

static char *GetMIMEType(int fd)
{
 Header *doc_header;
 char *contenttype,*mimetype=NULL;

 /* Get the header and examine it. */

 ParseReply(fd,&doc_header);

 contenttype=GetHeader(doc_header,"Content-Type");

 if(contenttype)
   {
    char *p;

    mimetype=(char*)malloc(strlen(contenttype)+1);
    strcpy(mimetype,contenttype);

    p=mimetype;
    while(*p && !isspace(*p) && *p!=';')
       p++;
    *p=0;
   }

 FreeHeader(doc_header);
 
 return(mimetype);
}
 

/*++++++++++++++++++++++++++++++++++++++
  A function to add a reference to a list.

  char* name The name to add.

  RefType type The type of reference.
  ++++++++++++++++++++++++++++++++++++++*/

void AddReference(char* name,RefType type)
{
 /* Special case for baseUrl. */

 if(type==RefBaseUrl && name)
   {
    baseUrl=SplitURL(name);
    return;
   }

 /* Check for badly formatted URLs */

 if(name)
   {
    char *p=name+strlen(name)-1;
    int onlychars=1;

    while(*name && isspace(*name))
       name++;

    if(*name=='#')
       return;

    while(p>name && isspace(*p))
       *p--=0;

    for(p=name;*p;p++)
       if(*p==':' && onlychars && !add_all)
         {
          int i;
          for(i=0;i<NProtocols;i++)
             if(!strncasecmp(Protocols[i].name,name,(size_t)(p-name)))
                break;

          if(i && i==NProtocols)
             return;
         }
       else if(onlychars && !isalpha(*p))
          onlychars=0;
   }

 /* Add it to the list. */

 if(name || reference_links[type])
   {
    if(reference_num[type]==0)
       reference_links[type]=(char**)malloc(16*sizeof(char*));
    else if((reference_num[type]%16)==0)
       reference_links[type]=(char**)realloc(reference_links[type],(reference_num[type]+16)*sizeof(char*));

    if(name)
      {
       reference_links[type][reference_num[type]]=(char*)malloc(strlen(name)+1);
       strcpy(reference_links[type][reference_num[type]],name);
       URLReplaceAmp(reference_links[type][reference_num[type]]);
      }
    else
       reference_links[type][reference_num[type]]=NULL;

    reference_num[type]++;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Finish the list of references and set the base URL if changed.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishReferences(void)
{
 RefType i;

 for(i=0;i<NRefTypes;i++)
    AddReference(NULL,i);
}


/*++++++++++++++++++++++++++++++++++++++
  Get the reference of the specified type.

  URL *GetReferences Returns the URL.

  RefType type The type of URL that is required.
  ++++++++++++++++++++++++++++++++++++++*/

URL *GetReference(RefType type)
{
 if(type==RefBaseUrl)
    return(baseUrl);
 else if(type==RefMetaRefresh)
   {
    URL *meta_refresh_Url;

    if(!reference_links[type])
       return(NULL);

    meta_refresh_Url=LinkURL(baseUrl,reference_links[type][0]);

    return(meta_refresh_Url);
   }
 else
    return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Get a list of the references of the specified type.

  URL **GetReferences Returns the list of URLs.

  RefType type The type of list that is required.
  ++++++++++++++++++++++++++++++++++++++*/

URL **GetReferences(RefType type)
{
 int i,j;

 if(!reference_links[type])
    return(NULL);

 if(type==RefBaseUrl || type==RefMetaRefresh)
    return(NULL);

 if(!reference_Urls[type])
   {
    /* Convert links to URLs */

    reference_Urls[type]=(URL**)malloc(reference_num[type]*sizeof(URL*));

    for(i=0;reference_links[type][i];i++)
       reference_Urls[type][i]=LinkURL(baseUrl,reference_links[type][i]);

    reference_Urls[type][i]=NULL;

    /* remove the duplicates */

    for(i=0;reference_Urls[type][i];i++)
      {
       for(j=i+1;reference_Urls[type][j];j++)
          if(!strcmp(reference_Urls[type][i]->file,reference_Urls[type][j]->file))
             break;

       if(reference_Urls[type][j])
         {
          free(reference_links[type][j]);
          FreeURL(reference_Urls[type][j]);
          do
            {
             reference_links[type][j]=reference_links[type][j+1];
             reference_Urls[type][j]=reference_Urls[type][j+1];
            }
          while(reference_Urls[type][j++]);
          i--;
          reference_num[type]--;
         }
      }
   }

 return(reference_Urls[type]);
}


/*++++++++++++++++++++++++++++++++++++++
  Reset all of the reference lists.
  ++++++++++++++++++++++++++++++++++++++*/

void ResetReferences(void)
{
 static int first=1;
 RefType i;

 for(i=0;i<NRefTypes;i++)
   {
    if(!first && reference_links[i])
      {
       int j;

       for(j=0;reference_links[i][j];j++)
          free(reference_links[i][j]);

       if(reference_Urls[i])
          for(j=0;reference_links[i][j];j++)
             FreeURL(reference_Urls[i][j]);

       free(reference_links[i]);

       if(reference_Urls[i])
          free(reference_Urls[i]);
      }

    reference_links[i]=NULL;
    reference_Urls[i]=NULL;
    reference_num[i]=0;
   }

 first=0;
}
