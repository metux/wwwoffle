VAR    \$[a-z_0-9]+
FUNC   \%[a-z_0-9]+\[[a-z_0-9]+\]

%x ASSIGNMENT_START
%x ASSIGNMENT_WORD ASSIGNMENT_DOUBLE_QUOTES ASSIGNMENT_SINGLE_QUOTES
%x ALTERNATE_TEST
%x ALTERNATE_SKIP_FIRST ALTERNATE_USE_FIRST
%x ALTERNATE_SKIP_SECOND ALTERNATE_USE_SECOND

%{
/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/messages.l,v 1.62 2006-01-29 19:59:19 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Parse the HTML to create the messages.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02,03,04,05 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

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
#include <fcntl.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "version.h"


#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif


extern int msg_yylex(void);


/* Local functions */

static char *html_message_body(int fd,char *template, va_list ap);

static void write_or_append_string(char *str);

static char /*@observer@*/ *add_variable(char *var,char *val);
static void delete_variables(void);
static char /*@observer@*/ *get_variable(char *var);
static char *call_function(char *func);

static void strip_trailing_whitespace(char *string);


/* Local variables */

/*+ A known fixed empty string. +*/
static char *empty="";

/*+ The file descriptor that we are reading from. +*/
static int msg_yyfd=-1;

/*+ The file descriptor that we are writing to. +*/
static int out_fd=-1;

/*+ The string we are appending to. +*/
static char *out_str=NULL;

/*+ The list of variables. +*/
static char **variables=NULL;

/*+ The list of values. +*/
static char **values=NULL;

/*+ The number of variables. +*/
static int nvariables=0;

/*+ The option to use compression or chunked encoding for the reply. +*/
static int client_compression=0,client_chunked=0;


/*++++++++++++++++++++++++++++++++++++++
  Set the options to use for the socket sending the message.

  int compressed Use compression (and type).

  int chunked Use chunked encoding.
 ++++++++++++++++++++++++++++++++++++++*/

void SetMessageOptions(int compressed,int chunked)
{
 client_compression=compressed;
 client_chunked=chunked;
}


/*++++++++++++++++++++++++++++++++++++++
  Create a simple message using the template in the html directory.

  int fd The file descriptor to write it to.

  int status_val The numeric status value.

  char *status_str The status string.

  char *location A Location: HTTP header or NULL for none.

  char *template The name of the template for the message.

  ... A list of variable-value pairs to use in the parsing (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

void HTMLMessage(int fd,int status_val,char *status_str,char *location,char *template, ...)
{
 va_list ap;

 if(location)
    HTMLMessageHead(fd,status_val,status_str,
                    "Location",location,
                    NULL);
 else
    HTMLMessageHead(fd,status_val,status_str,
                    NULL);

#ifdef __STDC__
 va_start(ap,template);
#else
 va_start(ap);
#endif

 html_message_body(fd,template,ap);

 va_end(ap);
}


/*++++++++++++++++++++++++++++++++++++++
  Create an html header using the specified fields.

  int fd The file descriptor to write it to.

  int status_val The numeric status value.

  char *status_str The status string.

  ... A list of variable-value pairs to use in the header (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

void HTMLMessageHead(int fd,int status_val,char *status_str, ...)
{
 char *headline,*headerstr;
 char *var,*val;
 va_list ap;
 Header *header;
 int content_type=0;

#ifdef __STDC__
 va_start(ap,status_str);
#else
 va_start(ap);
#endif

 /* The start of the header */

 headline=(char*)malloc(strlen(status_str)+16+MAX_INT_STR);
 sprintf(headline,"HTTP/1.0 %d %s\r\n",status_val,status_str);
 header=CreateHeader(headline,0);
 free(headline);

 AddToHeader(header,"Server","WWWOFFLE/" WWWOFFLE_VERSION);
 AddToHeader(header,"Date",RFC822Date(time(NULL),1));

 /* Start filling in the header. */

 while((var=va_arg(ap,char*)))
   {
    val=va_arg(ap,char*);

    if(!strcmp(var,"Content-Type"))
       content_type=1;

    if(val)
       AddToHeader(header,var,val);
   }

 va_end(ap);

 /* Set up compression header for the client if available and required. */

 if(client_compression)
   {
    /* If it is not to be compressed then don't */

    if(NotCompressed(GetHeader(header,"Content-Type"),NULL))
       client_compression=0;

    /* Add the compression header for the client. */

    else
      {
       if(client_compression==1)
          AddToHeader(header,"Content-Encoding","deflate");
       else /* if(client_compression==2) */
          AddToHeader(header,"Content-Encoding","gzip");

       PrintMessage(Debug,"Using 'Content-Encoding: %s' for the client.",
                    client_compression==1?"deflate":"gzip");
      }
   }

 /* Set up chunked encoding header for the client if required. */

 if(client_chunked)
   {
    ChangeVersionInHeader(header,"HTTP/1.1");
    AddToHeader(header,"Transfer-Encoding","chunked");

    PrintMessage(Debug,"Using 'Transfer-Encoding: chunked' for the client.");
   }

 /* The end of the header. */

 if(!content_type)
    AddToHeader(header,"Content-type","text/html");

 AddToHeader(header,"Connection","close");
 AddToHeader(header,"Proxy-Connection","close");

 /* Write the header */

 headerstr=HeaderString(header);

 if(StderrLevel==ExtraDebug)
    PrintMessage(ExtraDebug,"Reply Head (internal page)\n%s",headerstr);

 write_string(fd,headerstr);

 free(headerstr);

 FreeHeader(header);

 /* Initialise the client compression. */

 if(client_compression)
    configure_io_zlib(fd,-1,client_compression);

 /* Initialise the client chunked encoding. */

 if(client_chunked)
    configure_io_chunked(fd,-1,1);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a HTML message body by passing the specified template through the micro-language processor.

  int fd The file descriptor to write to.

  char *template The name of the template for the message.

  ... A list of variable-value pairs to use in the parsing (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

void HTMLMessageBody(int fd,char *template, ...)
{
 va_list ap;

#ifdef __STDC__
 va_start(ap,template);
#else
 va_start(ap);
#endif

 html_message_body(fd,template,ap);

 va_end(ap);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a HTML message string by passing the specified template through the micro-language processor.

  char *HTMLMessageString Returns a string containing the parsed template.

  char *template The name of the template for the message.

  ... A list of variable-value pairs to use in the parsing (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

char *HTMLMessageString(char *template, ...)
{
 char *string;
 va_list ap;

#ifdef __STDC__
 va_start(ap,template);
#else
 va_start(ap);
#endif

 string=html_message_body(-1,template,ap);

 va_end(ap);

 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  Finish up and free memory.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishMessages(void)
{
 delete_variables();
}


/*++++++++++++++++++++++++++++++++++++++
  Create a HTML message by passing the specified template through the micro-language processor.

  char *html_message_body Returns a string for the body or NULL when writing to file.

  int fd The file descriptor to write to (or -1 to create a string).

  char *template The name of the template for the message.

  va_list ap A list of variable-value pairs to use in the parsing (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

static char *html_message_body(int fd,char *template,va_list ap)
{
 char *var,*val;
 char *filename;
 static int first=1;

 /* Set up the variables. */

 out_fd=fd;
 out_str=NULL;

 if(fd==-1)
    delete_variables();

 if(nvariables==0)
   {
    char *localurl=GetLocalURL();
    add_variable("localurl",localurl);
    free(localurl);
    add_variable("version",WWWOFFLE_VERSION);
   }

 while((var=va_arg(ap,char*)))
   {
    val=va_arg(ap,char*);

    add_variable(var,val);
   }

 /* Open the file */

 filename=(char*)malloc(16+strlen(template));
 sprintf(filename,"messages/%s.html",template);

 msg_yyfd=OpenLanguageFile(filename);

 free(filename);

 if(msg_yyfd==-1)
    PrintMessage(Fatal,"Cannot open the message template '%s.html'.",template);

 init_io(msg_yyfd);

 /* Parse the template and fill in the gaps. */

 if(!first)
    msg_yyrestart(NULL);

 msg_yylex();

 finish_io(msg_yyfd);
 close(msg_yyfd);

 first=0;

 return(out_str);
}


/*++++++++++++++++++++++++++++++++++++++
  Write the string to the file descriptor out_fd or append to the string out_str.

  char *str The string to write or append.
  ++++++++++++++++++++++++++++++++++++++*/

static void write_or_append_string(char *str)
{
 if(out_fd==-1)
   {
    if(out_str)
      {
       out_str=(char*)realloc((void*)out_str,strlen(out_str)+strlen(str)+1);
       strcat(out_str,str);
      }
    else
      {
       out_str=(char*)malloc(256+strlen(str));
       strcpy(out_str,str);
      }
   }
 else
    write_buffer_data(out_fd,str,strlen(str)); /* expect to make lots of small writes */
}


/*++++++++++++++++++++++++++++++++++++++
  Add a new variable.

  char *add_variable Returns the newly allocated variable.

  char *var The variable to add.

  char *val The value of the variable.
  ++++++++++++++++++++++++++++++++++++++*/

static char *add_variable(char *var,char *val)
{
 int i;

 for(i=0;i<nvariables;i++)
    if(!strcmp(var,variables[i]))
      {
       if(values[i]!=empty)
          free(values[i]);
       if(val)
         {values[i]=(char*)malloc(strlen(val)+1); strcpy(values[i],val);}
       else
          values[i]=empty;

       return(values[i]);
      }

 if(nvariables==0)
   {
    variables=(char**)malloc(8*sizeof(char*));
    values   =(char**)malloc(8*sizeof(char*));
   }
 else if(nvariables%8==0)
   {
    variables=(char**)realloc((void*)variables,(nvariables+8)*sizeof(char*));
    values   =(char**)realloc((void*)values   ,(nvariables+8)*sizeof(char*));
   }

 variables[nvariables]=(char*)malloc(strlen(var)+1); strcpy(variables[nvariables],var);
 if(val)
   {values[nvariables]=(char*)malloc(strlen(val)+1); strcpy(values[nvariables],val);}
 else
    values[nvariables]=empty;

 nvariables++;

 return(values[nvariables-1]);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete all of the variables.
  ++++++++++++++++++++++++++++++++++++++*/

static void delete_variables(void)
{
 int i;

 for(i=0;i<nvariables;i++)
   {
    free(variables[i]);
    if(values[i]!=empty)
       free(values[i]);
   }

 if(nvariables)
   {
    free(variables);
    free(values);
    nvariables=0;
    variables=NULL;
    values=NULL;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Get the value of the named variable.

  char *get_variable Return the value of the variable or an empty string.

  char *var The variable to get the value of.
  ++++++++++++++++++++++++++++++++++++++*/

static char *get_variable(char *var)
{
 int i;

 for(i=0;i<nvariables;i++)
    if(!strcmp(var,variables[i]))
       return(values[i]);

 return(empty);
}


/*++++++++++++++++++++++++++++++++++++++
  Call the named function with the specificied variable.

  char *call_function Return the value of the function call.

  char *func The function name to call, including the argument.
  ++++++++++++++++++++++++++++++++++++++*/

static char *call_function(char *func)
{
 char *var,*val,*result=NULL,*bracket1,*bracket2;

 bracket1=strchr(func,'[');
 *bracket1=0;
 var=bracket1+1;
 bracket2=strchr(var,']');
 *bracket2=0;

 val=get_variable(var);

 if(val && !strcmp(func,"encode"))
    result=URLEncodeFormArgs(val);
 else if(val && !strcmp(func,"decode"))
   {
    char *ques=strchr(val,'?');
    char *dec;

    if(ques)
      {
       char *dec_path,*dec_args;

       *ques=0;

       dec_path=URLDecodeGeneric(val);
       dec_args=URLDecodeFormArgs(ques+1);
       
       dec=(char*)malloc(strlen(dec_path)+strlen(dec_args)+2);
       strcpy(dec,dec_path);
       strcat(dec,"?");
       strcat(dec,dec_args);

       *ques='?';

       free(dec_path);
       free(dec_args);
      }
    else
       dec=URLDecodeGeneric(val);

    result=HTMLString(dec,1);

    free(dec);
   }
 else if(val && !strcmp(func,"html"))
   {
    result=HTMLString(val,1);
   }
 else
    result=empty;

 *bracket1='[';
 *bracket2=']';

 val=add_variable(func,result);

 if(result && result!=empty)
    free(result);

 return(val);
}


/*++++++++++++++++++++++++++++++++++++++
  Strip trailing white-space (this is done so that the lex is simpler, no variable trailing context).

  char *string The string to strip the spaces from.
  ++++++++++++++++++++++++++++++++++++++*/

static void strip_trailing_whitespace(char *string)
{
 char *p=string;

 while(*p && !isspace(*p))
    p++;

 *p=0;
}


#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#ifndef msg_yywrap
/*+ Needed in lex but does nothing. +*/
#define msg_yywrap() 1
#endif

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
        if((result=read_data(msg_yyfd,buf,max_size))==-1) \
           result=0;

/*+ A macro to append a string to the end of the existing one. +*/
#define APPEND_VAL(xxx) \
{ \
 char *XXX=xxx; \
 if(val) \
   { \
    val=(char*)realloc((void*)val,strlen(val)+strlen(XXX)+1); \
    strcat(val,XXX); \
   } \
 else \
   { \
    val=(char*)malloc(strlen(XXX)+1); \
    strcpy(val,XXX); \
   } \
}

%}

%%
 /* Can use local variables since the parser only returns at EOF. */
 char *var=NULL;
 char *val=NULL;
 int previous=INITIAL;
 int any=0,in;

[^$%?\\\n \t]+                          { write_or_append_string(msg_yytext); any++; }
\n                                      { write_or_append_string(msg_yytext); any=0; }
\\[\"\'$%?\\{}]                         { write_or_append_string(msg_yytext+1); any++; }
[$%\\?]                                 { write_or_append_string(msg_yytext); any++; }

[ \t]+                                  { write_or_append_string(msg_yytext); any++; }
[ \t]*\\\r*\n                           { }

{VAR}                                   { write_or_append_string(get_variable(msg_yytext+1)); }
{VAR}[ \t]*/=                           { strip_trailing_whitespace(msg_yytext);
                                          var=(char*)malloc(strlen(msg_yytext)); strcpy(var,msg_yytext+1);
                                          previous=INITIAL; val=NULL; BEGIN(ASSIGNMENT_START); }
{VAR}[ \t]*/\?                          { strip_trailing_whitespace(msg_yytext);
                                          val=get_variable(msg_yytext+1); do{in=input();} while(in!='{' && in!=EOF);
                                          if(*val) BEGIN(ALTERNATE_USE_FIRST); else BEGIN(ALTERNATE_SKIP_FIRST); }
{VAR}[ \t]*/\?=                         { strip_trailing_whitespace(msg_yytext);
                                          val=get_variable(msg_yytext+1); do{in=input();} while(in!='=' && in!=EOF);
                                          BEGIN(ALTERNATE_TEST); }

{FUNC}                                  { write_or_append_string(call_function(msg_yytext+1)); }

<ASSIGNMENT_START>[^\'\" \t\r\n=}]      { yyless(0); BEGIN(ASSIGNMENT_WORD); }
<ASSIGNMENT_START>\"                    { BEGIN(ASSIGNMENT_DOUBLE_QUOTES); }
<ASSIGNMENT_START>\'                    { BEGIN(ASSIGNMENT_SINGLE_QUOTES); }
<ASSIGNMENT_START>[ \t\r]+              { }
<ASSIGNMENT_START>=                     { }
<ASSIGNMENT_START>\}                    { yyless(0); add_variable(var,NULL); free(var); BEGIN(previous); }
<ASSIGNMENT_START>\r*\n                 { add_variable(var,NULL); free(var); BEGIN(previous); }


<ASSIGNMENT_WORD>[^}$% \t\r\n]          { APPEND_VAL(msg_yytext); }
<ASSIGNMENT_WORD>[$%]                   { APPEND_VAL(msg_yytext); }
<ASSIGNMENT_WORD>\\\}                   { APPEND_VAL(msg_yytext+1); }
<ASSIGNMENT_WORD>{VAR}                  { APPEND_VAL(get_variable(msg_yytext+1)); }
<ASSIGNMENT_WORD>{FUNC}                 { APPEND_VAL(call_function(msg_yytext+1)); }
<ASSIGNMENT_WORD>\}                     { yyless(0); add_variable(var,val); if(val)free(val); free(var); BEGIN(previous); }
<ASSIGNMENT_WORD>[ \t\r\n]+             { add_variable(var,val); if(val)free(val); free(var); BEGIN(previous); }

<ASSIGNMENT_DOUBLE_QUOTES>[^\\\"$%]+    { APPEND_VAL(msg_yytext); }
<ASSIGNMENT_DOUBLE_QUOTES>\\\"          { APPEND_VAL(msg_yytext+1); }
<ASSIGNMENT_DOUBLE_QUOTES>[$%\\]        { APPEND_VAL(msg_yytext); }
<ASSIGNMENT_DOUBLE_QUOTES>{VAR}         { APPEND_VAL(get_variable(msg_yytext+1)); }
<ASSIGNMENT_DOUBLE_QUOTES>{FUNC}        { APPEND_VAL(call_function(msg_yytext+1)); }
<ASSIGNMENT_DOUBLE_QUOTES>\"            { add_variable(var,val); if(val)free(val); free(var); BEGIN(previous); }

<ASSIGNMENT_SINGLE_QUOTES>[^\\\'$%]+    { APPEND_VAL(msg_yytext); }
<ASSIGNMENT_SINGLE_QUOTES>\\\'          { APPEND_VAL(msg_yytext+1); }
<ASSIGNMENT_SINGLE_QUOTES>[$%\\]        { APPEND_VAL(msg_yytext); }
<ASSIGNMENT_SINGLE_QUOTES>{VAR}         { APPEND_VAL(get_variable(msg_yytext+1)); }
<ASSIGNMENT_SINGLE_QUOTES>{FUNC}        { APPEND_VAL(call_function(msg_yytext+1)); }
<ASSIGNMENT_SINGLE_QUOTES>\'            { add_variable(var,val); if(val)free(val); free(var); BEGIN(previous); }


<ALTERNATE_TEST>[ \t]+                  { }
<ALTERNATE_TEST>{VAR}                   { char *compare=get_variable(msg_yytext+1);
                                          do{in=input();} while(in!='{' && in!=EOF);
                                          if(!strcmp(val,compare)) BEGIN(ALTERNATE_USE_FIRST); else BEGIN(ALTERNATE_SKIP_FIRST); }
<ALTERNATE_TEST>[^\'\" \t\r\n{]+        { do{in=input();} while(in!='{' && in!=EOF);
                                          if(!strcmp(val,msg_yytext)) BEGIN(ALTERNATE_USE_FIRST); else BEGIN(ALTERNATE_SKIP_FIRST); }
<ALTERNATE_TEST>[\"\'\r\n{]             { BEGIN(INITIAL); }


<ALTERNATE_USE_FIRST>[^}$%?\\\n]+       { write_or_append_string(msg_yytext); any++; }
<ALTERNATE_USE_FIRST>\n                 { if(any) write_or_append_string(msg_yytext); any=0; }
<ALTERNATE_USE_FIRST>\\[\"\'$%?\\{}]    { write_or_append_string(msg_yytext+1); any++; }
<ALTERNATE_USE_FIRST>[$%\\?]            { write_or_append_string(msg_yytext); any++; }
<ALTERNATE_USE_FIRST>{VAR}              { write_or_append_string(get_variable(msg_yytext+1));any++; }
<ALTERNATE_USE_FIRST>{FUNC}             { write_or_append_string(call_function(msg_yytext+1)); any++; }
<ALTERNATE_USE_FIRST>{VAR}[ \t]*/=      { strip_trailing_whitespace(msg_yytext);
                                          var=(char*)malloc(strlen(msg_yytext)); strcpy(var,msg_yytext+1);
                                          previous=ALTERNATE_USE_FIRST; val=NULL; BEGIN(ASSIGNMENT_START); }
<ALTERNATE_USE_FIRST>\}[^{]*\{          { BEGIN(ALTERNATE_SKIP_SECOND); }
<ALTERNATE_USE_FIRST>\}                 { BEGIN(INITIAL); }


<ALTERNATE_SKIP_FIRST>[^\\}]+           { }
<ALTERNATE_SKIP_FIRST>\\\}              { }
<ALTERNATE_SKIP_FIRST>\\                { }
<ALTERNATE_SKIP_FIRST>\}[^{]*\{         { BEGIN(ALTERNATE_USE_SECOND); }
<ALTERNATE_SKIP_FIRST>\}                { BEGIN(INITIAL); }


<ALTERNATE_USE_SECOND>[^}$%?\\\n]+      { write_or_append_string(msg_yytext); any++; }
<ALTERNATE_USE_SECOND>\n                { if(any) write_or_append_string(msg_yytext); any=0; }
<ALTERNATE_USE_SECOND>\\[\"\'$%?\\{}]   { write_or_append_string(msg_yytext+1); any++; }
<ALTERNATE_USE_SECOND>[$%\\?]           { write_or_append_string(msg_yytext); any++; }
<ALTERNATE_USE_SECOND>{VAR}             { write_or_append_string(get_variable(msg_yytext+1)); any++; }
<ALTERNATE_USE_SECOND>{FUNC}            { write_or_append_string(call_function(msg_yytext+1)); any++; }
<ALTERNATE_USE_SECOND>{VAR}[ \t]*/=     { strip_trailing_whitespace(msg_yytext);
                                          var=(char*)malloc(strlen(msg_yytext)); strcpy(var,msg_yytext+1);
                                          previous=ALTERNATE_USE_SECOND; val=NULL; BEGIN(ASSIGNMENT_START); }
<ALTERNATE_USE_SECOND>\}[ \t]*\r*\n     { BEGIN(INITIAL); if(any) write_or_append_string("\n"); any=0; }
<ALTERNATE_USE_SECOND>\}[ \t]+          { BEGIN(INITIAL); unput(' '); }
<ALTERNATE_USE_SECOND>\}                { BEGIN(INITIAL); }


<ALTERNATE_SKIP_SECOND>[^\\\}]+         { }
<ALTERNATE_SKIP_SECOND>\\\}             { }
<ALTERNATE_SKIP_SECOND>\\               { }
<ALTERNATE_SKIP_SECOND>\}[ \t]*\r*\n    { BEGIN(INITIAL); if(any) write_or_append_string("\n"); any=0; }
<ALTERNATE_SKIP_SECOND>\}[ \t]+         { BEGIN(INITIAL); unput(' '); }
<ALTERNATE_SKIP_SECOND>\}               { BEGIN(INITIAL); }

 /* End of file */

<<EOF>>                                 { BEGIN(INITIAL); return(0); }

%%
