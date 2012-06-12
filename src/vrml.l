W               [ \t\r\n]
Q               [\"\']
NQ              [^\"\']
F               [-a-z0-9$_.!*(),%;/?:@&=+~|]
FA              [-a-z0-9$_.!*()%;/?:@&=+~|]
PA              [[]
PE              []]
NP              [^]]

%x VRML1
%x VRML1NAME VRML1REF

%x VRML2
%x VRML2URL VRML2REF VRML2REFEND

%{
/***************************************
  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9h.
  Parse the VRML and look for the WWWInline, WWWanchor and other things.
  ******************/ /******************
  Written by Walter Pfannenmueller

  This file Copyright 1997-2011 Walter Pfannenmueller & Andrew M. Bishop
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


extern int vrml_yylex(void);

/*+ The file descriptor that we are reading from. +*/
static int vrml_yyfd=-1;


/*++++++++++++++++++++++++++++++++++++++
  Parse the VRML and look for references.

  int fd The file descriptor of the file to parse.

  URL *Url The reference URL to use.
  ++++++++++++++++++++++++++++++++++++++*/

void ParseVRML(int fd,URL *Url)
{
 static int first=1;

 PrintMessage(Debug,"Parsing document using VRML parser.");

 vrml_yyfd=fd;

 if(!first)
   vrml_yyrestart(NULL);

 vrml_yylex();

 first=0;
}


#define YY_NO_INPUT 1 /* Remove annoying gcc warning message */

#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#ifndef vrml_yywrap
/*+ Needed in lex but does nothing. +*/
#define vrml_yywrap() 1
#endif

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
        if((result=read_data(vrml_yyfd,buf,max_size))==-1) \
           result=0;

%}

%%
 /* Can use local variables since the parser only returns at EOF. */
 RefType type=0;
 int more=0;


 /* Handle comments and other angle brackets */

"#VRML"" "+"V1.0"       { BEGIN(VRML1); }
 
"#VRML"" "+"V2.0"       { BEGIN(VRML2); }
.|\r|\n                 { }

 /*
  *   VRML V1.0
  */

 /* Comments */
<VRML1>"#".*            { BEGIN(VRML1); }
 /* Strings */
<VRML1>{Q}{NQ}*{Q}      { BEGIN(VRML1); }
<VRML1>"WWWAnchor"      { type = RefLink; BEGIN(VRML1NAME); }
<VRML1>"WWWInline"      { type = RefInlineObject; BEGIN(VRML1NAME); }
<VRML1>"Texture2"{W}    { type = RefInlineObject; BEGIN(VRML1NAME); }
<VRML1>.|\r|\n          { BEGIN(VRML1); }

 /* VRML V1.0 Reference */
<VRML1NAME>"#".*        { BEGIN(VRML1NAME); }
<VRML1NAME>{Q}{NQ}*{Q}  { BEGIN(VRML1NAME); }
<VRML1NAME>"file"*"name"{W}*{Q} { BEGIN(VRML1REF); }
<VRML1NAME>.|\r|\n      { BEGIN(VRML1NAME); }

<VRML1REF>{F}+          { AddReference(vrml_yytext,type); BEGIN(VRML1REF); }
<VRML1REF>{Q}           { BEGIN(VRML1); }
<VRML1REF>.|\r|\n       { }

 /*
  *  VRML V2.0
  */

 /* Comments */
<VRML2>"#".*            { BEGIN(VRML2); }
 /* Strings */
<VRML2>{Q}{NQ}*{Q}      { BEGIN(VRML2); }
<VRML2>"Anchor"         { type = RefLink; BEGIN(VRML2); }
<VRML2>"AudioClip"      { type = RefInlineObject; BEGIN(VRML2); }
<VRML2>"BackGround"     { type = RefInlineObject; BEGIN(VRML2); }
<VRML2>"ImageTexture"   { type = RefInlineObject; BEGIN(VRML2); }
<VRML2>"Inline"         { type = RefInlineObject; BEGIN(VRML2); }
<VRML2>"MovieTexture"   { type = RefInlineObject; BEGIN(VRML2); }
<VRML2>"Script"         { type = RefInlineObject; BEGIN(VRML2); }
<VRML2>"url"            { BEGIN(VRML2URL); }
<VRML2>"backUrl"        { BEGIN(VRML2URL); }
<VRML2>"bottomUrl"      { BEGIN(VRML2URL); }
<VRML2>"frontUrl"       { BEGIN(VRML2URL); }
<VRML2>"leftUrl"        { BEGIN(VRML2URL); }
<VRML2>"rightUrl"       { BEGIN(VRML2URL); }
<VRML2>"topUrl"         { BEGIN(VRML2URL); }
<VRML2>"EXTERNPROTO"{NP}*"]" { type = RefInlineObject; BEGIN(VRML2URL); }
<VRML2>.|\r|\n          { BEGIN(VRML2); }

 /* URLs */

<VRML2URL>{PE}          { more = 0; BEGIN(VRML2); }
<VRML2URL>"#".*         { BEGIN(VRML2URL); }
<VRML2URL>{W}+          { BEGIN(VRML2URL); }
<VRML2URL>{PA}          { more = 1; BEGIN(VRML2URL); }
<VRML2URL>{Q}           { BEGIN(VRML2REF); }
<VRML2URL>","           { BEGIN(VRML2URL); }
<VRML2URL>.|\r|\n       { unput(*vrml_yytext); BEGIN(VRML2); }

<VRML2REF>"javascript:"{F}* { /* not implemented yet */ 
                              /* LoadURL, GrabVrmlFromURL functions
                                 could add URLs 
                               */    
                              BEGIN(VRML2REFEND); }
<VRML2REF>"javabc:"{F}*     { /* not implemented yet */ 
                              /* parsing java bytecode */
                              BEGIN(VRML2REFEND); }
<VRML2REF>{F}+          { AddReference(vrml_yytext,type);
                          /* printf("AddReference %s %d\n",vrml_yytext,type); */
                          BEGIN(VRML2REFEND); }
<VRML2REF>.|\r|\n       { }


<VRML2REFEND>{NQ}*{Q}   { BEGIN(more ? VRML2URL : VRML2); }
<VRML2REFEND>.|\r|\n    { }

 /* End of file */

<<EOF>>                 { BEGIN(INITIAL); return(0); }

%%
