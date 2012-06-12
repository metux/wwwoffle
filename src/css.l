W               [ \t\r\n\f]

nonascii        [\200-\377]
ascii           [ -~]
alphanum        [a-z0-9]
punct           [][!\"#$%&\'()*+,-./:;<=>?@\\^_`{|}~]
safepunct       [][!\#$%&\*+,-./:;=?@\\^_`{|}~]

ident           ({nonascii}|{alphanum}|-)+
url             ({alphanum}|{nonascii}|{safepunct})+

%x COMMENT
%x IMPORT IMPORT_URL
%x MEDIA
%x DECL_LIST DECL_PROP DECL_VALUE DECL_VALUE_URL

%{
/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/css.l,v 1.13 2009-03-13 19:29:50 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9f.
  Parse the CSS and look for the images and stylesheets.
  ******************/ /******************
  Written by Andrew M. Bishop

  Some parts of the lex scanner based on the CSS2 specification from W3C.

  This file Copyright 1997-2009 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "document.h"

/* Parser outputs */

#define LEX_STYLESHEET  1
#define LEX_PROPERTY    2
#define LEX_VALUE       3


/* Local functions */

static void parse_css(URL *Url);

static /*@null@*/ char *css_yylval=NULL;
extern int css_yylex(void);


/*+ The file descriptor that we are reading from. +*/
static int css_yyfd=-1;


/*++++++++++++++++++++++++++++++++++++++
  Parse the CSS and look for references to URLs.

  int fd The file descriptor of the file to parse.

  URL *Url The reference URL to use.
  ++++++++++++++++++++++++++++++++++++++*/

void ParseCSS(int fd,URL *Url)
{
 static int first=1;

 PrintMessage(Debug,"Parsing document using CSS parser.");

 css_yyfd=fd;

 if(!first)
    css_yyrestart(NULL);

 parse_css(Url);

 first=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the CSS and look for references to URLs.

  URL *Url The URL being parsed.
  ++++++++++++++++++++++++++++++++++++++*/

static void parse_css(URL *Url)
{
 int yychar;
 int property_is_image=0;

 /* The actual parser. */

 while((yychar=css_yylex()))
    switch(yychar)
      {
      case LEX_STYLESHEET:
       AddReference(css_yylval,RefStyleSheet);
       break;

      case LEX_PROPERTY:
       if(!strcasecmp("background-image",css_yylval))
          property_is_image=1;
       else if(!strcasecmp("background",css_yylval))
          property_is_image=1;
       else if(!strcasecmp("list-style-image",css_yylval))
          property_is_image=1;
       break;

      case LEX_VALUE:
       if(property_is_image && strcasecmp(css_yylval,"none"))
          AddReference(css_yylval,RefImage);
       property_is_image=0;
       break;

      default:
       break;
      }
}


#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#ifndef css_yywrap
/*+ Needed in lex but does nothing. +*/
#define css_yywrap() 1
#endif

/*+ Don't include the yyinput() or input() function in the lexer. +*/
#define YY_NO_INPUT

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
        if((result=read_data(css_yyfd,buf,max_size))==-1) \
           result=0;

%}

%%
 /* Must use static variables since the parser returns often. */
 static int after_brace=INITIAL;

 /* Handle comments and other tags */

"/*"                        { BEGIN(COMMENT); }
"@import"                   { BEGIN(IMPORT); }
"@media"                    { BEGIN(MEDIA); }
\{                          { BEGIN(DECL_LIST); }
[^{@/]+                     { }
.|\n                        { }

 /* Comments in C style */

<COMMENT>"*"+"/"            { BEGIN(INITIAL); }
<COMMENT>"*"+               { }
<COMMENT>[^*]+              { }

 /* @import */

<IMPORT>{W}+                { }
<IMPORT>url\(\"             { BEGIN(IMPORT_URL); }
<IMPORT>url\(\'             { BEGIN(IMPORT_URL); }
<IMPORT>url\(               { BEGIN(IMPORT_URL); }
<IMPORT>\"                  { BEGIN(IMPORT_URL); }
<IMPORT>\'                  { BEGIN(IMPORT_URL); }
<IMPORT>.|\n                { BEGIN(INITIAL); }

<IMPORT_URL>{W}+            { }
<IMPORT_URL>{url}           { css_yylval=css_yytext; return(LEX_STYLESHEET); }
<IMPORT_URL>.               { BEGIN(INITIAL); }

 /* @media */

<MEDIA>{W}+                 { }
<MEDIA>[^{}]+               { }
<MEDIA>\{                   { after_brace=MEDIA;   BEGIN(INITIAL); }
<MEDIA>\}                   { after_brace=INITIAL; BEGIN(INITIAL); }

 /* Declaration list */

<DECL_LIST>{W}+             { }
<DECL_LIST>{ident}          { BEGIN(DECL_PROP);  css_yylval=css_yytext; return(LEX_PROPERTY); }
<DECL_LIST>\}               { BEGIN(after_brace); }
<DECL_LIST>.|\n             { }

<DECL_PROP>{W}*:            { BEGIN(DECL_VALUE); }
<DECL_PROP>.|\n             { }

<DECL_VALUE>{W}+            { }
<DECL_VALUE>url\(\"         { BEGIN(DECL_VALUE_URL); }
<DECL_VALUE>url\(\'         { BEGIN(DECL_VALUE_URL); }
<DECL_VALUE>url\(           { BEGIN(DECL_VALUE_URL); }
<DECL_VALUE>;               { BEGIN(DECL_LIST); }
<DECL_VALUE>\}              { unput('}'); BEGIN(DECL_LIST); }
<DECL_VALUE>[^;}u]+         { }
<DECL_VALUE>.|\n            { }

<DECL_VALUE_URL>{W}+        { }
<DECL_VALUE_URL>{url}       { BEGIN(DECL_VALUE);  css_yylval=css_yytext; return(LEX_VALUE); }
<DECL_VALUE_URL>.           { }

 /* End of file */

<<EOF>>                     { after_brace=INITIAL; BEGIN(INITIAL); return(0); }

%%
