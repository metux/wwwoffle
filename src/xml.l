W               [ \t\r\n]
Q               [\"\']
NQ              [^\"\']
F               [-a-z0-9$_.!*(),%;/?:@&=+~|]

%x ENTITY ENTITY_URL
%x DOCTYPE DOCTYPE_URL
%x COMMENT

%{
/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/xml.l,v 1.12 2003-09-06 12:55:06 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8.
  Parse the XML and look for external references.
  ******************/ /******************
  Written by Walter Pfannenmueller

  This file Copyright 1997,98,99,2001,03 Walter Pfannenmueller & Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "document.h"


extern int xml_yylex(void);

/*+ The file descriptor that we are reading from. +*/
static int xml_yyfd=-1;


/*++++++++++++++++++++++++++++++++++++++
  Parse the XML and look for references.

  int fd The file descriptor of the file to parse.

  URL *Url The reference URL to use.
  ++++++++++++++++++++++++++++++++++++++*/

void ParseXML(int fd,URL *Url)
{
 static int first=1;

 PrintMessage(Debug,"Parsing document using XML parser.");

 xml_yyfd=fd;

 if(!first)
    xml_yyrestart(NULL);

 xml_yylex();

 first=0;
}


#define YY_NO_UNPUT 1 /* Remove annoying gcc warning message */

#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#ifndef xml_yywrap
/*+ Needed in lex but does nothing. +*/
#define xml_yywrap() 1
#endif

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
        if((result=read_data(xml_yyfd,buf,max_size))==-1) \
           result=0;

%}

%%

 /*
        Just doctypes and external entities are handled so far.
 */

[^<]+                    { }
"<!--"                   { BEGIN(COMMENT); }
"<!ENTITY"               { BEGIN(ENTITY); }
"<!DOCTYPE"              { BEGIN(DOCTYPE); }
.|\r|\n                  { }

 /* ENTITIES */
<ENTITY>{W}*             { }
<ENTITY>"SYSTEM"{W}+{Q}  { BEGIN(ENTITY_URL); }
<ENTITY>"PUBLIC"{W}+{Q}{NQ}*{Q}{W}*{Q} { BEGIN(ENTITY_URL); }
<ENTITY>">"              { BEGIN(INITIAL); }
<ENTITY>.|\r|\n          { }

 /* Urls from entities. */
<ENTITY_URL>{F}+         { AddReference(xml_yytext,RefInlineObject); BEGIN(INITIAL); }
<ENTITY_URL>.|\r|\n      { BEGIN(INITIAL); }

 /* DOCTYPE */
<DOCTYPE>{W}*            { }
<DOCTYPE>"SYSTEM"{W}+{Q} { BEGIN(ENTITY_URL); }
<DOCTYPE>">"             { BEGIN(INITIAL); }
<DOCTYPE>.|\r|\n         { }

 /* Urls from entities. */
<DOCTYPE_URL>{F}+        { AddReference(xml_yytext,RefInlineObject); BEGIN(INITIAL); }
<DOCTYPE_URL>.|\r|\n     { BEGIN(INITIAL); }
 /* Comments */

<COMMENT>[^->]+          { }
<COMMENT>"-->"           { BEGIN(INITIAL); }
<COMMENT>.|\r|\n         { }

 /* End of file */

<<EOF>>                  { BEGIN(INITIAL); return(0); }

%%
