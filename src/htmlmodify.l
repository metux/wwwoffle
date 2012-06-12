W               [ \t\r\n\f]

nonascii        [\200-\377]
ascii           [ -~]
alphanum        [a-z0-9]
punct           [][!\"#$%&\'()*+,-./:;<=>?@\\^_`{|}~]
safepunct       [][!#$%&()*+,-./:;=?@\\^_`{|}~]

tag             {alphanum}+
key             ({alphanum}|-)+
val             ({alphanum}|{nonascii}|{safepunct})+

%x DOCTYPE
%x COMMENT COMMENT_BAD
%x TAG_START TAG TAG_ATTR_KEY TAG_ATTR_VAL
%x DQUOTED SQUOTED
%x SCRIPT_START SCRIPT SCRIPT_COMMENT_C SCRIPT_COMMENT_CPP SCRIPT_DQUOTED SCRIPT_SQUOTED
%x STYLE_START STYLE

%{
/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/htmlmodify.l 1.92 2006/08/04 18:22:46 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Parse the HTML and modify the source.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
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

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "proto.h"
#include "config.h"
#include "document.h"


/* Parser outputs */

#define LEX_PLAINTEXT      1
#define LEX_COMMENT        2
#define LEX_DOCTYPE        3

#define LEX_TAG_BEGIN     11
#define LEX_TAG_END       12
#define LEX_TAG_END_XHTML 13

#define LEX_ATTR_KEY      21
#define LEX_ATTR_VAL      22
#define LEX_ATTR_VAL_SQ   23
#define LEX_ATTR_VAL_DQ   24

/*+ Tag types +*/

typedef enum _HTMLTags
{
 tag__a        = 0  /* "/a"        */ ,
 tag_applet    = 1  /* "applet"    */ ,
 tag__applet   = 2  /* "/applet"   */ ,
 tag_base      = 3  /* "base"      */ ,
 tag_blink     = 4  /* "blink"     */ ,
 tag__blink    = 5  /* "/blink"    */ ,
 tag__body     = 6  /* "/body"     */ ,
 tag__embed    = 7  /* "/embed"    */ ,
 tag__html     = 8  /* "/html"     */ ,
 tag__iframe   = 9  /* "/iframe"   */ ,
 tag_marquee   =10  /* "marquee"   */ ,
 tag__marquee  =11  /* "/marquee"  */ ,
 tag_noscript  =12  /* "noscript"  */ ,
 tag__noscript =13  /* "/noscript" */ ,
 tag_param     =14  /* "param"     */ ,
 tag__object   =15  /* "/object"   */ ,
 tag_script    =16  /* "script"    */ ,
 tag__script   =17  /* "/script"   */ ,
 tag__style    =18  /* "/style"    */ ,

 tag_complex   =19  /* Complex tags, stored and processed as a whole. */,

 tag_a         =20  /* "a"         */ ,
 tag_body      =21  /* "body"      */ ,
 tag_embed     =22  /* "embed"     */ ,
 tag_iframe    =23  /* "iframe"    */ ,
 tag_img       =24  /* "img"       */ ,
 tag_link      =25  /* "link"      */ ,
 tag_meta      =26  /* "meta"      */ ,
 tag_object    =27  /* "object"    */ ,
 tag_style     =28  /* "style"     */ ,
 tag_td        =29  /* "td"        */ ,

 tag_ntags     =30
}
HTMLTags;

/*+ Tag strings +*/

static const char* const tags[]=
{
 /* tag__a        = 0  */ "/a"         ,
 /* tag_applet    = 1  */ "applet"     ,
 /* tag__applet   = 2  */ "/applet"    ,
 /* tag_base      = 3  */ "base"       ,
 /* tag_blink     = 4  */ "blink"      ,
 /* tag__blink    = 5  */ "/blink"     ,
 /* tag__body     = 6  */ "/body"      ,
 /* tag__embed    = 7  */ "/embed"     ,
 /* tag__html     = 8  */ "/html"      ,
 /* tag__iframe   = 9  */ "/iframe"    ,
 /* tag_marquee   =10  */ "marquee"    ,
 /* tag__marquee  =11  */ "/marquee"   ,
 /* tag_noscript  =12  */ "noscript"   ,
 /* tag__noscript =13  */ "/noscript"  ,
 /* tag_param     =14  */ "param"      ,
 /* tag__object   =15  */ "/object"    ,
 /* tag_script    =16  */ "script"     ,
 /* tag__script   =17  */ "/script"    ,
 /* tag__style    =18  */ "/style"     ,

 /* tag_complex   =19  */  ""          ,

 /* tag_a         =20  */ "a"          ,
 /* tag_body      =21  */ "body"       ,
 /* tag_embed     =22  */ "embed"      ,
 /* tag_iframe    =23  */ "iframe"     ,
 /* tag_img       =24  */ "img"        ,
 /* tag_link      =25  */ "link"       ,
 /* tag_meta      =26  */ "meta"       ,
 /* tag_object    =27  */ "object"     ,
 /* tag_style     =28  */ "style"      ,
 /* tag_td        =29  */ "td"
};

/*+ Attribute types +*/

typedef enum _HTMLAttributes
{
 att_background  = 0  /* "background"  */ ,
 att_classid     = 1  /* "classid"     */ ,
 att_codetype    = 2  /* "codetype"    */ ,
 att_content     = 3  /* "content"     */ ,
 att_data        = 4  /* "data"        */ ,
 att_height      = 5  /* "height"      */ ,
 att_href        = 6  /* "href"        */ ,
 att_http_equiv  = 7  /* "http-equiv"  */ ,
 att_onblur      = 8  /* "onblur"      */ ,
 att_onchange    = 9  /* "onchange"    */ ,
 att_onclick     =10  /* "onclick"     */ ,
 att_ondblclick  =11  /* "ondblclick"  */ ,
 att_onerror     =12  /* "onerror"     */ ,
 att_onfocus     =13  /* "onfocus"     */ ,
 att_onkeydown   =14  /* "onkeydown"   */ ,
 att_onkeypress  =15  /* "onkeypress"  */ ,
 att_onload      =16  /* "onload"      */ ,
 att_onmousedown =17  /* "onmousedown" */ ,
 att_onmousemove =18  /* "onmousemove" */ ,
 att_onmouseout  =19  /* "onmouseout"  */ ,
 att_onmouseover =20  /* "onmouseover" */ ,
 att_onmouseup   =21  /* "onmouseup"   */ ,
 att_onreset     =22  /* "onreset"     */ ,
 att_onselect    =23  /* "onselect"    */ ,
 att_onsubmit    =24  /* "onsubmit"    */ ,
 att_onunload    =25  /* "onunload"    */ ,
 att_rel         =26  /* "rel"         */ ,
 att_src         =27  /* "src"         */ ,
 att_style       =28  /* "style"       */ ,
 att_type        =29  /* "type"        */ ,
 att_width       =30  /* "width"       */ ,

 att_natts       =31
}
HTMLAttributes;

/*+ Attribute strings. +*/

static const char* const attributes[]=
{
 /* att_background  = 0  */ "background"  ,
 /* att_classid     = 1  */ "classid"     ,
 /* att_codetype    = 2  */ "codetype"    ,
 /* att_content     = 3  */ "content"     ,
 /* att_data        = 4  */ "data"        ,
 /* att_height      = 5  */ "height"      ,
 /* att_href        = 6  */ "href"        ,
 /* att_http_equiv  = 7  */ "http-equiv"  ,
 /* att_onblur      = 8  */ "onblur"      ,
 /* att_onchange    = 9  */ "onchange"    ,
 /* att_onclick     =10  */ "onclick"     ,
 /* att_ondblclick  =11  */ "ondblclick"  ,
 /* att_onerror     =12  */ "onerror"     ,
 /* att_onfocus     =13  */ "onfocus"     ,
 /* att_onkeydown   =14  */ "onkeydown"   ,
 /* att_onkeypress  =15  */ "onkeypress"  ,
 /* att_onload      =16  */ "onload"      ,
 /* att_onmousedown =17  */ "onmousedown" ,
 /* att_onmousemove =18  */ "onmousemove" ,
 /* att_onmouseout  =19  */ "onmouseout"  ,
 /* att_onmouseover =20  */ "onmouseover" ,
 /* att_onmouseup   =21  */ "onmouseup"   ,
 /* att_onreset     =22  */ "onreset"     ,
 /* att_onselect    =23  */ "onselect"    ,
 /* att_onsubmit    =24  */ "onsubmit"    ,
 /* att_onunload    =25  */ "onunload"    ,
 /* att_rel         =26  */ "rel"         ,
 /* att_src         =27  */ "src"         ,
 /* att_style       =28  */ "style"       ,
 /* att_type        =29  */ "type"        ,
 /* att_width       =30  */ "width"
};

/*+ A structure to hold a tag and its attributes. +*/

typedef struct _Tag
{
 HTMLTags type;                 /*+ The type of the tag. +*/

 char *tag;                     /*+ The Tag itself. +*/

 int xhtml;                     /*+ A flag that is set for an XHTML closing tag '< ... />' +*/

 int nattr;                     /*+ The number of attributes. +*/
 int nattr_malloc;              /*+ The number of attributes that space is malloced for. +*/

 int *attr_type;                /*+ The list of attribute types. +*/
 char **attr_key;               /*+ The list of attribute keys. +*/
 char **attr_val;               /*+ The list of attribute values. +*/
 char **attr_quote;             /*+ The list of attribute quotes. +*/
}
Tag;


/* Microsoft Character mapping */

/*+ The option to convert the characters when seen. +*/
static int demoronise_ms_chars;
static int fix_mixed_cyrillic;


/* Definitions of why the output is disabled. */

#define DISABLE_NONE         0

#define DISABLE_META         1
#define DISABLE_LINK         2
#define DISABLE_OBJECT       4
#define DISABLE_A            8
#define DISABLE_IFRAME      16
#define DISABLE_IMG         32
#define DISABLE_STYLE       64

#define DISABLE_PARSE      256

/* Local functions */

static void modify_html(URL *Url);

static /*@null@*/ char *htmlmodify_yylval=NULL;
extern int htmlmodify_yylex(void);

static /*@null@*/ char *extract_charset(const char *content_type);

static int handle_a_tag(const Tag *tag,int disable_dontget_anchors,int disable_script);
static int handle_iframe_tag(const Tag *tag,int disable_dontget_iframes,int disable_script);
static void handle_img_tag(Tag *tag,int replace_dontget,const char *dontget_replacement,
                                    int replace_webbug,const char *webbug_replacement,
                                    int disable_script);
static int handle_object_tag(Tag *tag,int replace_dontget,const char *dontget_replacement,
                                      int replace_webbug,const char *webbug_replacement,
                                      int disable_applet,
                                      int disable_flash,
                                      int disable_dontget_iframes,
                                      int disable_script);
static void output_img_or_object_tag(Tag *tag,int src_att,
                                              int replace_dontget,const char *dontget_replacement,
                                              int replace_webbug,const char *webbug_replacement,
                                              int disable_script);
static void handle_link_tag(const Tag *tag,int disable_style,int disable_script);
static int handle_style_script_tag(const Tag *tag,int disable_script);
static void handle_meta_tag(const Tag *tag,int disable_meta_refresh,int disable_meta_refresh_self,int disable_meta_set_cookie);
static void output_tag(const Tag *tag,const char *prefix,const char *suffix);

static void handle_high_bit(unsigned char ch);


/*+ The add-cache-info optional footer. +*/
static /*@null@*/ /*@observer@*/ char *cache_info=NULL;

/*+ The base URL of this page. +*/
static /*@null@*/ URL *baseUrl=NULL;

/*+ Set this to disable the output. +*/
static int disable_output=DISABLE_NONE;


/*++++++++++++++++++++++++++++++++++++++
  Output the file with the modificatons if it is HTML, else just output.

  URL *Url The URL that we are parsing.

  int spool The file descriptor for the spool file to get the date from.

  char *content_type The HTTP header containing the content type (and perhaps the charset).
  ++++++++++++++++++++++++++++++++++++++*/

void OutputHTMLWithModifications(URL *Url,int spool,char *content_type)
{
 static int first=1;

 if(ConfigBooleanURL(AddCacheInfo,Url))
   {
    struct stat buf;
    time_t t_ago;
    char *date,*timeunit,timeago[MAX_INT_STR+1];

    fstat(spool,&buf);

    t_ago=time(NULL)-buf.st_mtime;
    date=RFC822Date(buf.st_mtime,0);
    
    if(t_ago<0)
      {strcpy(timeago,"?");timeunit="";}
    else if(t_ago<3600)
      {sprintf(timeago,"%ld",(long)t_ago/60);timeunit="m";}
    else if(t_ago<(24*3600))
      {sprintf(timeago,"%ld",(long)t_ago/3600);timeunit="h";}
    else if(t_ago<(14*24*3600))
      {sprintf(timeago,"%ld",(long)t_ago/(24*3600));timeunit="d";}
    else if(t_ago<(30*24*3600))
      {sprintf(timeago,"%ld",(long)t_ago/(7*24*3600));timeunit="w";}
    else
      {sprintf(timeago,"%ld",(long)t_ago/(30*24*3600));timeunit="M";}

    cache_info=HTMLMessageString("AddCacheInfo",
                                 "url",Url->name,
                                 "date",date,
                                 "time",timeago,
                                 "unit",timeunit,
                                 NULL);
   }

 demoronise_ms_chars=0;
 if(ConfigBooleanURL(DemoroniseMSChars,Url))
   {
    char* charset=extract_charset(content_type);

    if(charset)
      {
       if(strcasecmp(charset,"utf-8") &&
          strcasecmp(charset,"koi8-r") &&
          strcasecmp(charset,"euc-kr") &&
          strcasecmp(charset,"big5") &&
          strcasecmp(charset,"iso-2022-jp") &&
          strcasecmp(charset,"chinesebig5"))
          demoronise_ms_chars=1;

       free(charset);
      }
    else
       demoronise_ms_chars=1;
   }

 fix_mixed_cyrillic=0;
 if(ConfigBooleanURL(FixMixedCyrillic,Url))
   {
    char* charset=extract_charset(content_type);

    if(charset)
      {
       if(!strcasecmp(charset,"koi8-r"))
          fix_mixed_cyrillic=1;

       free(charset);
      }
    else
       fix_mixed_cyrillic=1;
   }

 baseUrl=Url;

 if(!first)
    htmlmodify_yyrestart(NULL);

 modify_html(Url);

 cache_info=NULL;

 first=0;
}


/*+ A macro to output the data if valid to do so. +*/
#define YY_OUTPUT(text) \
           if(!disable_output && *text) \
              wwwoffles_write_data(text,strlen(text))


/*++++++++++++++++++++++++++++++++++++++
  Extract the charset from a MIME type and charset.

  char *extract_charset Returns the charset that it found or NULL if none.

  const char *content_type The HTTP content type.
  ++++++++++++++++++++++++++++++++++++++*/

static char *extract_charset(const char *content_type)
{
 const char *p;
 char *charset=NULL;

 /* ' *text/html *; *charset *= *["']?...["']?' */

 p=content_type;

 while(*p && *p!=';') p++;
 if(*p!=';') return(NULL); /* unparseable */
 p++;

 while(isspace(*p)) p++;
 if(!*p) return(NULL); /* unparseable */

 if(!strncasecmp(p,"charset",(size_t)7))
   {
    char *q;

    p+=7;

    while(*p && *p!='=') p++;
    if(*p!='=') return(NULL); /* unparseable */
    p++;

    while(isspace(*p)) p++;
    if(!*p) return(NULL); /* unparseable */

    if(*p=='"' || *p=='\'') p++;

    charset=(char*)malloc(strlen(p)+1);
    strcpy(charset,p);

    q=charset+strlen(p)-1;
    if(*q=='"' || *q=='\'') *q=0;
   }

 return(charset);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the anchor tag and parse it.

  int handle_a_tag Returns 1 if the anchor was disabled.

  const Tag *tag The tag information.

  int disable_dontget_anchors The option to disable links to URLs that are not got.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_a_tag(const Tag *tag,int disable_dontget_anchors,int disable_script)
{
 int i;
 int is_dontget=0,is_script=0;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_href && tag->attr_val[i])
      {
       if(disable_dontget_anchors)
         {
          URL *linkUrl=LinkURL(baseUrl,tag->attr_val[i]);

          is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

          FreeURL(linkUrl);
         }
       if(disable_script)
         {
          if(!strncasecmp("javascript:",tag->attr_val[i],(size_t)11))
             is_script=1;
         }
      }

 /* Output the original or modified tag. */

 if(disable_dontget_anchors && is_dontget)
    output_tag(tag,"!-- WWWOFFLE (disable-dontget-links) - "," --");
 else if(disable_script && is_script)
    output_tag(tag,"!-- WWWOFFLE (disable-script) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return((disable_dontget_anchors && is_dontget) || (disable_script && is_script));
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the iframe tag and parse it.

  int handle_iframe_tag Returns 1 if the iframe was disabled.

  const Tag *tag The tag information.

  int disable_dontget_iframes The option to disable iframes to URLs that are not got.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_iframe_tag(const Tag *tag,int disable_dontget_iframes,int disable_script)
{
 int i;
 int is_dontget=0,is_script=0;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_src && tag->attr_val[i])
      {
       if(disable_dontget_iframes)
         {
          URL *linkUrl=LinkURL(baseUrl,tag->attr_val[i]);

          is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

          FreeURL(linkUrl);
         }
       if(disable_script)
         {
          if(!strncasecmp("javascript:",tag->attr_val[i],(size_t)11))
             is_script=1;
         }
      }

 /* Output the original or modified tag. */

 if(disable_dontget_iframes && is_dontget)
    output_tag(tag,"!-- WWWOFFLE (disable-dontget-iframes) - "," --");
 else if(disable_script && is_script)
    output_tag(tag,"!-- WWWOFFLE (disable-script) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return((disable_dontget_iframes && is_dontget) || (disable_script && is_script));
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the img tag and parse it.

  Tag *tag The tag information.

  int replace_dontget The option to replace the images in the DontGet section.

  const char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  const char *webbug_replacement The webbug replacement image.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_img_tag(Tag *tag,int replace_dontget,const char *dontget_replacement,
                                    int replace_webbug,const char *webbug_replacement,
                                    int disable_script)
{
 int i;
 int src_att=-1;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_src && tag->attr_val[i])
       src_att=i;

 output_img_or_object_tag(tag,src_att,replace_dontget,dontget_replacement,replace_webbug,webbug_replacement,disable_script);
}


/*++++++++++++++++++++++++++++++++++++++
  Output an image tag or an object tag that contains an image.

  Tag *tag The tag.

  int src_att The attribute number the contains the URI.

  int replace_dontget The option to replace the images in the DontGet section.

  const char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  const char *webbug_replacement The webbug replacement image.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static void output_img_or_object_tag(Tag *tag,int src_att,
                                              int replace_dontget,const char *dontget_replacement,
                                              int replace_webbug,const char *webbug_replacement,
                                              int disable_script)
{
 int i;
 int is_dontget=0,is_webbug=0,is_script=0;

 if(src_att>=0)
   {
    if(replace_dontget)
      {
       URL *linkUrl=LinkURL(baseUrl,tag->attr_val[src_att]);

       is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

       FreeURL(linkUrl);
      }

    if(replace_webbug)
      {
       int width=1000,height=1000;

       for(i=0;i<tag->nattr;i++)
          if(tag->attr_type[i]==att_width && tag->attr_val[i])
             width=atoi(tag->attr_val[i]);
          else if(tag->attr_type[i]==att_height && tag->attr_val[i])
             height=atoi(tag->attr_val[i]);

       if(width<=1 && height<=1)
          is_webbug=1;
      }

    if(disable_script)
      {
       if(!strncasecmp("javascript:",tag->attr_val[src_att],(size_t)11))
          is_script=1;
      }
   }

 /* Modify the src attribute (if required). */

 if(is_dontget)
   {
    output_tag(tag,"!-- WWWOFFLE (replace-dontget-images) - "," --");

    tag->attr_val[src_att]=(char*)realloc((void*)tag->attr_val[src_att],strlen(dontget_replacement)+1);
    strcpy(tag->attr_val[src_att],dontget_replacement);
   }
 else if(is_webbug)
   {
    output_tag(tag,"!-- WWWOFFLE (replace-webbug-images) - "," --");

    tag->attr_val[src_att]=(char*)realloc((void*)tag->attr_val[src_att],strlen(webbug_replacement)+1);
    strcpy(tag->attr_val[src_att],webbug_replacement);
   }
 else if(is_script)
    output_tag(tag,"!-- WWWOFFLE (replace-scripts) - "," --");

 /* Blank the alt & title attributes or add an empty alt (if required). */

 if(!is_script && (is_dontget || is_webbug))
   {
    int seen_alt=0;

    for(i=0;i<tag->nattr;i++)
       if(!strcasecmp(tag->attr_key[i],"alt") || !strcasecmp(tag->attr_key[i],"title"))
         {
          if(tag->attr_val[i])
             free(tag->attr_val[i]);

          tag->attr_val  [i]=(char*)calloc((size_t)1,1);
          tag->attr_quote[i]="\"";

          seen_alt=1;
         }

    if(!seen_alt)
      {
       if(tag->nattr==tag->nattr_malloc)
         {
          tag->attr_type=(int*)realloc((void*)tag->attr_type,(tag->nattr_malloc+1)*sizeof(int));
          tag->attr_key=(char**)realloc((void*)tag->attr_key,(tag->nattr_malloc+1)*sizeof(char*));
          tag->attr_val=(char**)realloc((void*)tag->attr_val,(tag->nattr_malloc+1)*sizeof(char*));
          tag->attr_quote=(char**)realloc((void*)tag->attr_quote,(tag->nattr_malloc+1)*sizeof(char*));

          tag->attr_key[tag->nattr_malloc]=NULL;
          tag->attr_val[tag->nattr_malloc]=NULL;

          tag->nattr_malloc+=1;
         }

       tag->attr_type [tag->nattr]=att_natts;
       tag->attr_key  [tag->nattr]=(char*)malloc((size_t)4); strcpy(tag->attr_key[tag->nattr],"alt");
       tag->attr_val  [tag->nattr]=(char*)calloc((size_t)1,(size_t)1);
       tag->attr_quote[tag->nattr]="\"";

       tag->nattr++;
      }
   }

 /* Output the original or modified tag. */

 if(!is_script)
    output_tag(tag,NULL,NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the object tag and parse it.

  int handle_object_tag Returns 1 if the object is Java and disabled,
                                2 if Flash and disabled,
                                3 if equivalent to an iframe and disabled.

  Tag *tag The tag information.

  int replace_dontget The option to replace the images in the DontGet section.

  const char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  const char *webbug_replacement The webbug replacement image.

  int disable_applet The option to disable Java applets.

  int disable_flash The option to disable Flash animations.

  int disable_dontget_iframes The option to disable inline frames that are on the dontget list.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_object_tag(Tag *tag,int replace_dontget,const char *dontget_replacement,
                                      int replace_webbug,const char *webbug_replacement,
                                      int disable_applet,
                                      int disable_flash,
                                      int disable_dontget_iframes,
                                      int disable_script)
{
 int i;
 int is_image=0,is_java=0,is_flash=0,is_inline=0,is_dontget=0,is_script=0;
 int data_att=-1;

 /* Check for images. */

 if(replace_dontget || replace_webbug)
   {
    for(i=0;i<tag->nattr;i++)
       if((tag->attr_type[i]==att_codetype && tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"image",(size_t)5)) ||
          (tag->attr_type[i]==att_type && tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"image",(size_t)5)))
          is_image=1;
       else if(tag->attr_type[i]==att_data && tag->attr_val[i])
          data_att=i;
   }

 /* Check for Java */

 if(disable_applet)
   {
    for(i=0;i<tag->nattr;i++)
       if(tag->attr_type[i]==att_codetype && tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"application/java",(size_t)16))
          is_java=1;
       else if(tag->attr_type[i]==att_classid && tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"java:",(size_t)5))
          is_java=1;
   }

 /* Check for Flash */

 if(disable_flash)
   {
    for(i=0;i<tag->nattr;i++)
       if((tag->attr_type[i]==att_codetype || tag->attr_type[i]==att_type) &&
          tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"application/x-shockwave-flash",(size_t)29))
          is_flash=1;
       else if(tag->attr_type[i]==att_classid && tag->attr_val[i] &&
               !strncasecmp(tag->attr_val[i],"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000",(size_t)42))
          is_flash=1;
       else if(tag->attr_type[i]==att_src && tag->attr_val[i] &&
               !strncasecmp(tag->attr_val[i]+strlen(tag->attr_val[i])-4,".swf",(size_t)4))
          is_flash=1;
   }

 /* Check for inline HTML (text) object */

 if(disable_dontget_iframes)
   {
    for(i=0;i<tag->nattr;i++)
       if((tag->attr_type[i]==att_codetype && tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"text",(size_t)4)) ||
          (tag->attr_type[i]==att_type && tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"text",(size_t)4)))
          is_inline=1;
       else if(tag->attr_type[i]==att_data && tag->attr_val[i])
          data_att=i;

    if(is_inline && data_att>=0)
      {
       if(disable_dontget_iframes)
         {
          URL *linkUrl=LinkURL(baseUrl,tag->attr_val[data_att]);

          is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

          FreeURL(linkUrl);
         }
       if(disable_script)
         {
          if(!strncasecmp("javascript:",tag->attr_val[data_att],(size_t)11))
             is_script=1;
         }
      }
   }

 /* Output the original or modified tag. */

 if(is_image && (replace_dontget || replace_webbug || disable_script))
    output_img_or_object_tag(tag,data_att,replace_dontget,dontget_replacement,replace_webbug,webbug_replacement,disable_script);
 else if(disable_applet && is_java)
    output_tag(tag,"!-- WWWOFFLE (disable-applet) - "," --");
 else if(disable_flash && is_flash)
    output_tag(tag,"!-- WWWOFFLE (disable-flash) - "," --");
 else if(is_inline && disable_dontget_iframes && is_dontget)
    output_tag(tag,"!-- WWWOFFLE (disable-dontget-iframes) - "," --");
 else if(is_inline && disable_script && is_script)
    output_tag(tag,"!-- WWWOFFLE (disable-scripts) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return(is_image?0:
        (disable_applet && is_java)?1:
        (disable_flash && is_flash)?2:
        (is_inline && disable_dontget_iframes && is_dontget)?3:
        (is_inline && disable_script && is_script)?3:
        0);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the link tag and parse it.

  const Tag *tag The tag information.

  int disable_style Set to true if stylesheets are disabled.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_link_tag(const Tag *tag,int disable_style,int disable_script)
{
 int i;
 int is_stylesheet=0,is_script=0;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_rel && tag->attr_val[i])
      {
       if(disable_style)
         {
          if(!strncasecmp(tag->attr_val[i],"Stylesheet",(size_t)10))
             is_stylesheet=1;
         }
       if(disable_script)
         {
          if(!strncasecmp(tag->attr_val[i],"javascript:",(size_t)11))
             is_script=1;
         }
      }

 /* Output the original or modified tag. */

 if(disable_style && is_stylesheet)
    output_tag(tag,"!-- WWWOFFLE (disable-style) - "," --");
 else if(disable_script && is_script)
    output_tag(tag,"!-- WWWOFFLE (disable-script) - "," --");
 else
    output_tag(tag,NULL,NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the style tag and parse it.

  int handle_style_script_tag Returns true if a script was detected.

  const Tag *tag The tag information.

  int disable_script Set to true if scripts are disabled.

  See "WhiteHat Security Advisory [Number: WH-08152001-1]" for the details of this.

  The HTML <style type="application/x-javascript"> or <style type="text/javascript">
  can introduce Javascript that WWWOFFLE would not otherwise have blocked.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_style_script_tag(const Tag *tag,int disable_script)
{
 int i;
 int is_script=0;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_type && tag->attr_val[i])
      {
       int j=strlen(tag->attr_val[i])-10;

       while(j>=0) /* strcasestr() */
         {
          if(!strncasecmp(&tag->attr_val[i][j],"javascript",(size_t)10))
             is_script=1;
          j--;
         }
      }

 /* Output the original or modified tag. */

 if(disable_script && is_script)
    output_tag(tag,"!-- WWWOFFLE (disable-script) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return(disable_script && is_script);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the meta tag and parse it.

  const Tag *tag The tag information.

  int disable_meta_refresh Set to the disable-meta-refresh option.

  int disable_meta_refresh_self Set to the disable-meta-refresh-self option.

  int disable_meta_set_cookie Set to the disable-meta-set-cookie option.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_meta_tag(const Tag *tag,int disable_meta_refresh,int disable_meta_refresh_self,int disable_meta_set_cookie)
{
 int i;
 int is_meta_http_equiv_refresh=0;
 int is_meta_http_equiv_set_cookie=0;
 int is_meta_http_equiv_content_type=0;

 /* Check for interesting meta tags */

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_http_equiv && tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"Refresh",(size_t)7))
       is_meta_http_equiv_refresh=1;
    else if(tag->attr_type[i]==att_http_equiv && tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"Set-Cookie",(size_t)10))
       is_meta_http_equiv_set_cookie=1;
    else if(tag->attr_type[i]==att_http_equiv && tag->attr_val[i] && !strncasecmp(tag->attr_val[i],"Content-Type",(size_t)12))
       is_meta_http_equiv_content_type=1;

 /* Handle if they are refresh ones. */

 if(is_meta_http_equiv_refresh && (disable_meta_refresh_self || disable_meta_refresh))
   {
    char *meta_refresh=NULL;

    for(i=0;i<tag->nattr;i++)
       if(tag->attr_type[i]==att_content && tag->attr_val[i] && tag->attr_val[i][0])
         {
          char *p;

          /* ' *[0-9].?[0-9]* *[;,] *(URL *= *|)http://...' */

          p=tag->attr_val[i];
          while(isspace(*p)) p++;
          if(!isdigit(*p))
             ; /* unparseable */
          else
            {
             while(isdigit(*p)) p++;
             if(*p=='.')
               {p++; while(isdigit(*p)) p++;}
             while(isspace(*p)) p++;
             if(!*p)
               {
                meta_refresh=(char*)malloc(strlen(baseUrl->name)+1);
                strcpy(meta_refresh,baseUrl->name);
               }
             else if(*p!=';' && *p!=',')
                ; /* unparseable */
             else
               {
                p++;
                while(isspace(*p)) p++;
                if(!strncasecmp(p,"URL",(size_t)3))
                  {
                   p+=3;
                   while(isspace(*p)) p++;
                   if(*p!='=')
                      ; /* unparseable */
                   else
                     {
                      p++;
                      while(isspace(*p)) p++;
                     }
                   if(!*p)
                      ; /* unparseable */
                   else
                     {
                      meta_refresh=(char*)malloc(strlen(p)+1);
                      strcpy(meta_refresh,p);
                     }
                  }
               }
            }
         }

    /* Check if link is to itself. */

    if(meta_refresh && disable_meta_refresh_self && !disable_meta_refresh)
      {
       URL *linkUrl=LinkURL(baseUrl,meta_refresh);

       if(strcmp(baseUrl->name,linkUrl->name))
          is_meta_http_equiv_refresh=0;

       FreeURL(linkUrl);
      }

    if(meta_refresh)
       free(meta_refresh);
   }

 /* Handle if they are charset ones. */

 if(is_meta_http_equiv_content_type && (demoronise_ms_chars || fix_mixed_cyrillic))
    for(i=0;i<tag->nattr;i++)
       if(tag->attr_type[i]==att_content && tag->attr_val[i] && tag->attr_val[i][0])
         {
          char* charset=extract_charset(tag->attr_val[i]);

          if(charset)
            {
             if(demoronise_ms_chars)
               {
                if(!strcasecmp(charset,"utf-8") ||
                   !strcasecmp(charset,"koi8-r") ||
                   !strcasecmp(charset,"euc-kr") ||
                   !strcasecmp(charset,"big5") ||
                   !strcasecmp(charset,"iso-2022-jp") ||
                   !strcasecmp(charset,"chinesebig5"))
                   demoronise_ms_chars=0;
               }
             else if(fix_mixed_cyrillic)
               {
                if(strcasecmp(charset,"koi8-r"))
                   fix_mixed_cyrillic=0;
               }

             free(charset);
            }
         }

 /* Output the original or modified tag. */

 if(is_meta_http_equiv_refresh && disable_meta_refresh)
    output_tag(tag,"!-- WWWOFFLE (disable-meta-refresh) - "," --");
 else if(is_meta_http_equiv_refresh && disable_meta_refresh_self)
    output_tag(tag,"!-- WWWOFFLE (disable-meta-refresh-self) - "," --");
 else if(is_meta_http_equiv_set_cookie && disable_meta_set_cookie)
    output_tag(tag,"!-- WWWOFFLE (disable-meta-set-cookie) - "," --");
 else
    output_tag(tag,NULL,NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Output a complete tag with optional custom head and/or tail.

  const Tag *tag The tag to output.

  const char *prefix The optional prefix of the tag.

  const char *suffix The optional suffix of the tag.
  ++++++++++++++++++++++++++++++++++++++*/

static void output_tag(const Tag *tag,const char *prefix,const char *suffix)
{
 int i;

 YY_OUTPUT("<");

 if(prefix)
   {YY_OUTPUT(prefix);}

 YY_OUTPUT(tag->tag);

 for(i=0;i<tag->nattr;i++)
   {
    YY_OUTPUT(" ");
    YY_OUTPUT(tag->attr_key[i]);
    if(tag->attr_val[i])
      {
       YY_OUTPUT("=");
       if(tag->attr_quote[i][0]) {YY_OUTPUT(tag->attr_quote[i]);}
       YY_OUTPUT(tag->attr_val[i]);
       if(tag->attr_quote[i][0]) {YY_OUTPUT(tag->attr_quote[i]);}
      }
   }

 if(suffix)
   {YY_OUTPUT(suffix);}

 if(tag->xhtml && !suffix)
   {YY_OUTPUT(" />");}
 else
   {YY_OUTPUT(">");}
}


/*++++++++++++++++++++++++++++++++++++++
  Modify the HTML looking for all of the things to be changed.

  URL *Url The URL that this page comes from.
  ++++++++++++++++++++++++++++++++++++++*/

static void modify_html(URL *Url)
{
 HTMLTags tag=tag_ntags;
 HTMLAttributes key=att_natts;
 int url_cached=0;
 int yychar,i;
 int disable_key_val;
 char *key_string=NULL,*prefix,*suffix,*quote;
 Tag tagdata;

 char *anchor_modify_begin[3];
 char *anchor_modify_end[3];
 int disable_script=ConfigBooleanURL(DisableHTMLScript,Url);
 int disable_applet=ConfigBooleanURL(DisableHTMLApplet,Url);
 int object_nesting=0,disable_object[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
 int disable_style=ConfigBooleanURL(DisableHTMLStyle,Url),disable_style_script=0;
 int disable_blink=ConfigBooleanURL(DisableHTMLBlink,Url);
 int disable_marquee=ConfigBooleanURL(DisableHTMLMarquee,Url);
 int disable_flash=ConfigBooleanURL(DisableHTMLFlash,Url);
 int disable_iframes=ConfigBooleanURL(DisableHTMLIFrame,Url);
 int disable_meta_refresh=ConfigBooleanURL(DisableHTMLMetaRefresh,Url);
 int disable_meta_refresh_self=ConfigBooleanURL(DisableHTMLMetaRefreshSelf,Url);
 int disable_meta_set_cookie=ConfigBooleanURL(DisableHTMLMetaSetCookie,Url);
 int disable_dontget_anchors=ConfigBooleanURL(DisableHTMLDontGetAnchors,Url),disable_anchor=0;
 int disable_dontget_iframes=ConfigBooleanURL(DisableHTMLDontGetIFrames,Url),disable_iframe=0;
 int replace_html_dontget_images=ConfigBooleanURL(ReplaceHTMLDontGetImages,Url);
 char *replacement_html_dontget_image=ConfigStringURL(ReplacementHTMLDontGetImage,Url);
 int replace_html_webbug_images=ConfigBooleanURL(ReplaceHTMLWebbugImages,Url);
 char *replacement_html_webbug_image=ConfigStringURL(ReplacementHTMLWebbugImage,Url);

 anchor_modify_begin[0]=ConfigStringURL(AnchorModifyBegin[0],Url);
 anchor_modify_begin[1]=ConfigStringURL(AnchorModifyBegin[1],Url);
 anchor_modify_begin[2]=ConfigStringURL(AnchorModifyBegin[2],Url);
 anchor_modify_end[0]=ConfigStringURL(AnchorModifyEnd[0],Url);
 anchor_modify_end[1]=ConfigStringURL(AnchorModifyEnd[1],Url);
 anchor_modify_end[2]=ConfigStringURL(AnchorModifyEnd[2],Url);

 if(*replacement_html_dontget_image=='/')
   {
    char *copy=replacement_html_dontget_image;
    char *localurl=GetLocalURL();

    replacement_html_dontget_image=(char*)malloc(strlen(copy)+strlen(localurl)+1);

    strcpy(replacement_html_dontget_image,localurl);
    strcat(replacement_html_dontget_image,copy);

    free(localurl);
   }

 if(*replacement_html_webbug_image=='/')
   {
    char *copy=replacement_html_webbug_image;
    char *localurl=GetLocalURL();

    replacement_html_webbug_image=(char*)malloc(strlen(copy)+strlen(localurl)+1);

    strcpy(replacement_html_dontget_image,localurl);
    strcat(replacement_html_dontget_image,copy);

    free(localurl);
   }

 /* Initialise the tagdata */

 tagdata.type=tag_ntags;
 tagdata.tag=NULL;
 tagdata.xhtml=0;
 tagdata.nattr=0;
 tagdata.nattr_malloc=16;
 tagdata.attr_type=(int*)calloc((size_t)16,sizeof(int));
 tagdata.attr_key=(char**)calloc((size_t)16,sizeof(char*));
 tagdata.attr_val=(char**)calloc((size_t)16,sizeof(char*));
 tagdata.attr_quote=(char**)calloc((size_t)16,sizeof(char*));

 /* The actual parser. */

 while((yychar=htmlmodify_yylex()))
    switch(yychar)
      {
      case LEX_PLAINTEXT:
       break;

      case LEX_COMMENT:
       break;

      case LEX_DOCTYPE:
       break;

      case LEX_TAG_BEGIN:
       for(tag=0;tag<tag_ntags;tag++)
          if(!strcasecmp(htmlmodify_yylval,tags[tag]))
             break;

       if(tag>tag_complex && tag<tag_ntags)
         {
          tagdata.type=tag;
          tagdata.tag=(char*)realloc((void*)tagdata.tag,strlen(htmlmodify_yylval)+1);
          strcpy(tagdata.tag,htmlmodify_yylval);
          tagdata.nattr=0;
          tagdata.xhtml=0;
         }

       prefix=NULL;

       if(tag==tag__a)
         {
          if(disable_anchor)
             prefix="!-- WWWOFFLE (disable-dontget-links) - ";
          else
            {
             if(url_cached==1)
               {if(anchor_modify_end[0]) {YY_OUTPUT(anchor_modify_end[0]);}}
             else if(url_cached==2)
               {if(anchor_modify_end[1]) {YY_OUTPUT(anchor_modify_end[1]);}}
             else if(url_cached==-1)
               {if(anchor_modify_end[2]) {YY_OUTPUT(anchor_modify_end[2]);}}
             url_cached=0;
            }
         }
       else if(tag==tag__iframe && disable_iframe)
          prefix="!-- WWWOFFLE (disable-dontget-iframes) - ";
       else if(tag==tag__body && cache_info)
         {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}
       else if(tag==tag__html && cache_info)
         {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}
       else if((tag==tag_blink || tag==tag__blink) && disable_blink)
          prefix="!-- WWWOFFLE (disable-blink) - ";
       else if((tag==tag_marquee || tag==tag__marquee) && disable_marquee)
          prefix="!-- WWWOFFLE (disable-marquee) - ";
       else if((tag==tag_script || tag==tag__script) && disable_script)
          prefix="!-- WWWOFFLE (disable-script) - ";
       else if((tag==tag_noscript || tag==tag__noscript) && disable_script)
          prefix="!-- WWWOFFLE (disable-script) - ";
       else if((tag==tag_applet || tag==tag__applet) && disable_applet)
          prefix="!-- WWWOFFLE (disable-applet) - ";
       else if(tag==tag_param && disable_object[object_nesting]==1)
          prefix="!-- WWWOFFLE (disable-applet) - ";
       else if(tag==tag_param && disable_object[object_nesting]==2)
          prefix="!-- WWWOFFLE (disable-flash) - ";
       else if(tag==tag_param && disable_object[object_nesting]==3)
          prefix="!-- WWWOFFLE (disable-dontget-iframes) - ";
       else if(tag==tag__object && disable_object[object_nesting]==1)
          prefix="!-- WWWOFFLE (disable-applet) - ";
       else if(tag==tag__object && disable_object[object_nesting]==2)
          prefix="!-- WWWOFFLE (disable-flash) - ";
       else if(tag==tag__object && disable_object[object_nesting]==3)
          prefix="!-- WWWOFFLE (disable-dontget-iframes) - ";
       else if(tag==tag__embed && disable_object[object_nesting])
          prefix="!-- WWWOFFLE (disable-flash) - ";
       else if(tag==tag_style && disable_style)
          prefix="!-- WWWOFFLE (disable-style) - ";
       else if(tag==tag_style && disable_script) /* could be script pretending to be style */
          disable_output|=DISABLE_STYLE;
       else if(tag==tag__style && disable_style)
          prefix="!-- WWWOFFLE (disable-style) - ";
       else if(tag==tag__style && disable_style_script)
          prefix="!-- WWWOFFLE (disable-script) - ";
       else if(tag==tag_meta && (disable_meta_refresh_self || disable_meta_refresh || disable_meta_set_cookie))
          disable_output|=DISABLE_META;
       else if(tag==tag_link && (disable_style || disable_script))
          disable_output|=DISABLE_LINK;
       else if((tag==tag_object || tag==tag_embed) &&
               (replace_html_dontget_images || replace_html_webbug_images ||
                disable_applet || disable_flash ||
                disable_dontget_iframes || disable_script))
          disable_output|=DISABLE_OBJECT;
       else if(tag==tag_a && (disable_dontget_anchors || disable_script))
          disable_output|=DISABLE_A;
       else if(tag==tag_iframe && (disable_iframes || disable_dontget_iframes || disable_script))
          disable_output|=DISABLE_IFRAME;
       else if(tag==tag_img && (replace_html_dontget_images || replace_html_webbug_images || disable_script))
          disable_output|=DISABLE_IMG;

       disable_output&=~DISABLE_PARSE;

       YY_OUTPUT("<");
       if(prefix)
         {YY_OUTPUT(prefix);}
       YY_OUTPUT(htmlmodify_yylval);
       break;

      case LEX_TAG_END_XHTML:
       tagdata.xhtml=1;

       /*@fallthrough@*/

      case LEX_TAG_END:
       suffix=NULL;

       if(tag==tag_applet)
         {if(object_nesting<sizeof(disable_object)/sizeof(disable_object[0])) object_nesting++;}

       if(tag==tag__a && disable_anchor)
         {suffix=" --";disable_anchor=0;}
       else if(tag==tag__iframe && disable_iframe)
         {suffix=" --";disable_iframe=0;}
       else if((tag==tag_blink || tag==tag__blink) && disable_blink)
         suffix=" --";
       else if((tag==tag_marquee || tag==tag__marquee) && disable_marquee)
         suffix=" --";
       else if((tag==tag_script || tag==tag__script) && disable_script)
         suffix=" --";
       else if((tag==tag_noscript || tag==tag__noscript) && disable_script)
         suffix=" --";
       else if(tag==tag_applet && disable_applet)
         {suffix=" --"; disable_object[object_nesting]=1;}
       else if(tag==tag__applet && disable_applet)
         {suffix=" --"; disable_object[object_nesting]=0;}
       else if(tag==tag_param && disable_object[object_nesting])
         suffix=" --";
       else if(tag==tag__object && disable_object[object_nesting])
         {suffix=" --"; disable_object[object_nesting]=0;}
       else if(tag==tag__embed && disable_object[object_nesting])
         {suffix=" --"; disable_object[object_nesting]=0;}
       else if((tag==tag_style || tag==tag__style) && disable_style)
         suffix=" --";
       else if((tag==tag_style || tag==tag__style) && disable_style)
         suffix=" --";
       else if(tag==tag__style && disable_style_script)
         {suffix="--"; disable_style_script=0;}

       if(tag==tag__object || tag==tag__applet || tag==tag__embed)
         {if(object_nesting>0) object_nesting--;}

       if(suffix)
         {YY_OUTPUT(suffix);}
       if(yychar==LEX_TAG_END_XHTML && !suffix)
         {YY_OUTPUT(" />");}
       else
         {YY_OUTPUT(">");}

       if(tag==tag_meta && (disable_meta_refresh_self || disable_meta_refresh || disable_meta_set_cookie ||
                            demoronise_ms_chars || fix_mixed_cyrillic))
         {
          disable_output&=~DISABLE_META;
          handle_meta_tag(&tagdata,disable_meta_refresh,disable_meta_refresh_self,disable_meta_set_cookie);
         }
       else if(tag==tag_link && (disable_style || disable_script))
         {
          disable_output&=~DISABLE_LINK;
          handle_link_tag(&tagdata,disable_style,disable_script);
         }
       else if((tag==tag_object || tag==tag_embed) &&
               (replace_html_dontget_images || replace_html_webbug_images ||
                disable_applet || disable_flash ||
                disable_dontget_iframes || disable_script))
         {
          disable_output&=~DISABLE_OBJECT;
          if(object_nesting<sizeof(disable_object)/sizeof(disable_object[0])) object_nesting++;
          disable_object[object_nesting]=handle_object_tag(&tagdata,replace_html_dontget_images,replacement_html_dontget_image,
                                                                replace_html_webbug_images,replacement_html_webbug_image,
                                                                disable_applet,
                                                                disable_flash,
                                                                disable_dontget_iframes,
                                                                disable_script);
         }
       else if(tag==tag_a)
         {
          if(disable_dontget_anchors || disable_script)
            {
             disable_output&=~DISABLE_A;
             disable_anchor=handle_a_tag(&tagdata,disable_dontget_anchors,disable_script);
            }

          if(!disable_anchor)
            {
             if(url_cached==1)
               {if(anchor_modify_begin[0]) {YY_OUTPUT(anchor_modify_begin[0]);}}
             else if(url_cached==2)
               {if(anchor_modify_begin[1]) {YY_OUTPUT(anchor_modify_begin[1]);}}
             else if(url_cached==-1)
               {if(anchor_modify_begin[2]) {YY_OUTPUT(anchor_modify_begin[2]);}}
            }
         }
       else if(tag==tag_iframe)
         {
          if(disable_iframes)
            {
             disable_output&=~DISABLE_IFRAME;
             output_tag(&tagdata,"!-- WWWOFFLE (disable-iframes) - "," --");
             disable_iframe=1;
            }
          else if(disable_dontget_iframes || disable_script)
            {
             disable_output&=~DISABLE_IFRAME;
             disable_iframe=handle_iframe_tag(&tagdata,disable_dontget_iframes,disable_script);
            }
         }
       else if(tag==tag_img && (replace_html_dontget_images || replace_html_webbug_images || disable_script))
         {
          disable_output&=~DISABLE_IMG;
          handle_img_tag(&tagdata,replace_html_dontget_images,replacement_html_dontget_image,
                                  replace_html_webbug_images,replacement_html_webbug_image,
                                  disable_script);
         }
       else if(tag==tag_script && disable_script)
          disable_output|=DISABLE_PARSE;
       else if(tag==tag_style && disable_style)
          disable_output|=DISABLE_PARSE;
       else if(tag==tag_style && disable_script)
         {
          disable_output&=~DISABLE_STYLE;

          disable_style_script=handle_style_script_tag(&tagdata,disable_script);

          if(disable_style_script)
             disable_output|=DISABLE_PARSE;
         }

       tag=tag_ntags;
       key=att_natts;
       break;

      case LEX_ATTR_KEY:
       key_string=(char*)realloc((void*)key_string,strlen(htmlmodify_yylval)+1);
       strcpy(key_string,htmlmodify_yylval);

       for(key=0;key<att_natts;key++)
          if(!strcasecmp(htmlmodify_yylval,attributes[key]))
             break;
      break;

      case LEX_ATTR_VAL_DQ:
       /*@fallthrough@*/
      case LEX_ATTR_VAL_SQ:
       /*@fallthrough@*/
      case LEX_ATTR_VAL:
       disable_key_val=0;

       if(yychar==LEX_ATTR_VAL_DQ)
          quote="\"";
       else if(yychar==LEX_ATTR_VAL_SQ)
          quote="\'";
       else
          quote="";

       /* Links */

       if(key==att_href && tag==tag_a && htmlmodify_yylval)
         {
          if(*htmlmodify_yylval)
            {
             URL *linkUrl=LinkURL(baseUrl,htmlmodify_yylval);

             if(!IsProtocolHandled(linkUrl))
                url_cached=0;
             else if(ExistsWebpageSpoolFile(linkUrl) || IsLocalNetHost(linkUrl->host))
                url_cached=1;
             else if(ExistsOutgoingSpoolFile(linkUrl))
                url_cached=2;
             else
               {
                URL *aliasUrl=GetAliasURL(linkUrl);

                if(!aliasUrl)
                   url_cached=-1;
                else
                  {
                   if(!IsProtocolHandled(aliasUrl))
                      url_cached=0;
                   else if(ExistsWebpageSpoolFile(aliasUrl) || IsLocalNetHost(aliasUrl->host))
                      url_cached=1;
                   else if(ExistsOutgoingSpoolFile(aliasUrl))
                      url_cached=2;
                   else
                      url_cached=-1;

                   FreeURL(aliasUrl);
                  }
               }

             FreeURL(linkUrl);
            }
          else
             url_cached=1;
         }

       /* Base tag */

       if(key==att_href && htmlmodify_yylval && tag==tag_base)
          baseUrl=SplitURL(htmlmodify_yylval);

       /* Script events */

       else if(disable_script &&
               (key==att_onblur || key==att_onchange || key==att_onclick || key==att_ondblclick || key==att_onfocus ||
                key==att_onerror || key==att_onkeydown || key==att_onkeypress || key==att_onload || key==att_onmousedown ||
                key==att_onmousemove || key==att_onmouseout || key==att_onmouseover || key==att_onmouseup ||
                key==att_onreset || key==att_onselect || key==att_onsubmit || key==att_onunload))
          disable_key_val=1;

       /* Style references */

       else if(disable_style && key==att_style)
          disable_key_val=1;

       /* More complicated tags that depend on other attributes are stored and done later. */

       else if(tag>tag_complex && tag<tag_ntags)
         {
          if(tagdata.nattr==tagdata.nattr_malloc)
            {
             tagdata.attr_type=(int*)realloc((void*)tagdata.attr_type,(tagdata.nattr_malloc+1)*sizeof(int));
             tagdata.attr_key=(char**)realloc((void*)tagdata.attr_key,(tagdata.nattr_malloc+1)*sizeof(char*));
             tagdata.attr_val=(char**)realloc((void*)tagdata.attr_val,(tagdata.nattr_malloc+1)*sizeof(char*));
             tagdata.attr_quote=(char**)realloc((void*)tagdata.attr_quote,(tagdata.nattr_malloc+1)*sizeof(char*));

             tagdata.attr_key[tagdata.nattr_malloc]=NULL;
             tagdata.attr_val[tagdata.nattr_malloc]=NULL;

             tagdata.nattr_malloc+=1;
            }

          tagdata.attr_type[tagdata.nattr]=key;
          tagdata.attr_key[tagdata.nattr]=(char*)realloc((void*)tagdata.attr_key[tagdata.nattr],strlen(key_string)+1);
          strcpy(tagdata.attr_key[tagdata.nattr],key_string);
          if(htmlmodify_yylval)
            {
             tagdata.attr_val[tagdata.nattr]=(char*)realloc((void*)tagdata.attr_val[tagdata.nattr],strlen(htmlmodify_yylval)+1);
             strcpy(tagdata.attr_val[tagdata.nattr],htmlmodify_yylval);
            }
          else
            {
             if(tagdata.attr_val[tagdata.nattr]) free(tagdata.attr_val[tagdata.nattr]);
             tagdata.attr_val[tagdata.nattr]=NULL;
            }
          tagdata.attr_quote[tagdata.nattr]=quote;

          tagdata.nattr++;
         }

       /* Output the attribute and key or not. */

       if(!disable_key_val)
         {
          YY_OUTPUT(key_string);
          if(htmlmodify_yylval)
            {
             YY_OUTPUT("=");
             if(*quote)
               {YY_OUTPUT(quote);}
             YY_OUTPUT(htmlmodify_yylval);
             if(*quote)
               {YY_OUTPUT(quote);}
            }
         }

       key=att_natts;
       break;

      default:
       break;
      }

 if(cache_info)
   {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}

 if(key_string)
    free(key_string);

 /* Delete the tagdata */

 for(i=0;i<tagdata.nattr_malloc;i++)
   {
    if(tagdata.attr_key[i]) free(tagdata.attr_key[i]);
    if(tagdata.attr_val[i]) free(tagdata.attr_val[i]);
   }

 if(tagdata.tag) free(tagdata.tag);

 free(tagdata.attr_type);
 free(tagdata.attr_key);
 free(tagdata.attr_val);
 free(tagdata.attr_quote);
}


/* The demoronise_ms_chars idea is taken from the public domain Demoroniser perl script */

/*************************************************/
/* De-moron-ise Text from Microsoft Applications */
/*         by John Walker -- January 1998        */
/*            http://www.fourmilab.ch/           */
/*************************************************/

 /*+ The list of characters to replace. +*/
 static const char* const demoronise_ms_chars_list[]={/* 0x80 */ "\200",
                                                      /* 0x81 */ "\201",
                                                      /* 0x82 */ ",",
                                                      /* 0x83 */ "<em>f</em>",
                                                      /* 0x84 */ ",,",
                                                      /* 0x85 */ "...",
                                                      /* 0x86 */ "\206",
                                                      /* 0x87 */ "\207",
                                                      /* 0x88 */ "^",
                                                      /* 0x89 */ " °/°°",
                                                      /* 0x8A */ "\212",
                                                      /* 0x8B */ "<",
                                                      /* 0x8C */ "Oe",
                                                      /* 0x8D */ "\215",
                                                      /* 0x8E */ "\216",
                                                      /* 0x8F */ "\217",
                                                      /* 0x90 */ "\220",
                                                      /* 0x91 */ "`",
                                                      /* 0x92 */ "'",
                                                      /* 0x93 */ "\"",
                                                      /* 0x94 */ "\"",
                                                      /* 0x95 */ "*",
                                                      /* 0x96 */ "-",
                                                      /* 0x97 */ "--",
                                                      /* 0x98 */ "<sup>~</sup>",
                                                      /* 0x99 */ "<sup>TM</sup>",
                                                      /* 0x9A */ "\232",
                                                      /* 0x9B */ ">",
                                                      /* 0x9C */ "oe",
                                                      /* 0x9D */ "\235",
                                                      /* 0x9E */ "\236",
                                                      /* 0x9F */ "\237"};

/* This "fix-mixed-cyrillic" code is written by Ilya Dogolazky
 * e-mail: ilyad at math dot uni-bonn dot de
 *
 * There are weird Russian websites [ www.novayagazeta.ru for example ] using
 * koi8-r encoding for the letters of Russian alphabeth, and cp-1251 encoding
 * for the punctuations ("<<", ">>", "No", "\bullet" etc).
 *
 * We take all characters between 0x80 and 0xBF (there are three exceptions
 * below) and recode they from windows-1251 to Unicode (in "&#xCODE;" HTML
 * notation)
 *
 * The idea is similar to that of "demoronise-ms-chars", but these options are
 * clearly mutually exclusive
 */

 /* This table is generated by the following Perl statement:
  * print ord(decode("windows-1251",chr($_)) for (0x80..0xBF) ;
  * See 3 exceptions below...
  */                                         

static const unsigned int fix_mixed_cyrillic_list[]={0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,
                                                     0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
                                                     0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
                                                     0x0   ,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
                                                     0x00A0,0x040E,0x045E,0x0   ,0x00A4,0x0490,0x00A6,0x00A7,
                                                     0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
                                                     0x00B0,0x00B1,0x0406,0x0   ,0x0491,0x00B5,0x00B6,0x00B7,
                                                     0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457};


/*++++++++++++++++++++++++++++++++++++++
  Handle the characters on the input with the high-bit set.

  unsigned char ch The character.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_high_bit(unsigned char ch)
{
 if(demoronise_ms_chars && ch<=0x9F)
   {
    YY_OUTPUT("<!-- WWWOFFLE (demoronise-ms-chars) - '");
    YY_OUTPUT(htmlmodify_yytext);
    YY_OUTPUT("' -->");
    YY_OUTPUT(demoronise_ms_chars_list[ch-0x80]);
   }
 else if(fix_mixed_cyrillic) /* && ch<=0xBF */
   {
    unsigned int unicode_value = fix_mixed_cyrillic_list[ch-0x80] ;

    if(unicode_value==0x0)
      {
       /* We have 3 such values for ch: the first and the second are koi8-r
          codes of \"e and \"E (ch==163 || ch==179), and the third is
          ch==152. This value is very mysterious: there is no Unicode-equivalent
          for character with code 152 of windows-1251 page (perl's decode fails
          for this code, ask Bill Gates for details :-) */
       YY_OUTPUT(htmlmodify_yytext);
      }
    else
      {
       char html_buffer[9];
       sprintf(html_buffer,"&#x%04X;",unicode_value);

       YY_OUTPUT("<!-- WWWOFFLE (fix-mixed-cyrillic) - '");
       YY_OUTPUT(htmlmodify_yytext);
       YY_OUTPUT("' -->");
       YY_OUTPUT(html_buffer);
      }
   }
 else
    YY_OUTPUT(htmlmodify_yytext);
}


#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#ifndef htmlmodify_yywrap
/*+ Needed in lex but does nothing. +*/
#define htmlmodify_yywrap() 1
#endif

/*+ Reset the current string. +*/
#define reset_string \
 if(string) *string=0; \
 stringused=0;

/*+ append information to the current string. +*/
#define append_string(xx) \
 newlen=strlen(xx); \
 if((stringused+newlen)>=stringlen) \
    string=(char*)realloc((void*)string,stringlen=(stringused+newlen+1)); \
 strcpy(string+stringused,xx); \
 stringused+=newlen;

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
        if((result=wwwoffles_read_data(buf,max_size))==-1) \
           result=0;

%}

%%
 /* Must use static variables since the parser returns often. */
 static char *string=NULL;
 static size_t stringlen=0,stringused=0;
 static int after_tag=INITIAL;
 int newlen;

 /* Handle comments and other tags */

[\x80-\xBF]                 { handle_high_bit(*(unsigned char*)htmlmodify_yytext); }
[^<\x80-\xBF]+              { YY_OUTPUT(htmlmodify_yytext); /* htmlmodify_yylval=htmlmodify_yytext; return(LEX_PLAINTEXT); */ }

"<!DOCTYPE"                 { YY_OUTPUT(htmlmodify_yytext); BEGIN(DOCTYPE); reset_string; }
"<!--"                      { YY_OUTPUT(htmlmodify_yytext); BEGIN(COMMENT); reset_string; }
"<!"{W}*"-"*                { YY_OUTPUT(htmlmodify_yytext); BEGIN(COMMENT_BAD); reset_string; }
"<"{W}*                     { BEGIN(TAG_START); reset_string; append_string(htmlmodify_yytext); }

 /* Doctype (DTD) */

<DOCTYPE>">"                { YY_OUTPUT(htmlmodify_yytext); BEGIN(INITIAL); /* htmlmodify_yylval=string; return(LEX_DOCTYPE); */ }
<DOCTYPE>[^>]+              { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }

 /* Comments - COMMENT_BAD is not a legal comment format (except <!>) but people use it as one.
               COMMENT is not strictly correct, but works better than the real thing. */

<COMMENT>"--"{W}*">"        { YY_OUTPUT(htmlmodify_yytext); BEGIN(INITIAL); /* htmlmodify_yylval=string; return(LEX_COMMENT); */ }
<COMMENT>">"                { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }
<COMMENT>"-"                { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }
<COMMENT>[^->]+             { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }

<COMMENT_BAD>">"            { YY_OUTPUT(htmlmodify_yytext); BEGIN(INITIAL); /* htmlmodify_yylval=string; return(LEX_COMMENT); */ }
<COMMENT_BAD>[^>]+          { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }

 /* Tags */

<TAG_START>"script"/{W}     { BEGIN(TAG); after_tag=SCRIPT_START; htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"script"/">"     { BEGIN(TAG); after_tag=SCRIPT_START; htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"style"/{W}      { BEGIN(TAG); after_tag=STYLE_START;  htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"style"/">"      { BEGIN(TAG); after_tag=STYLE_START;  htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/" "    { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/\t     { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/\n     { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/\r     { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/">"    { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>(.|\n)           { BEGIN(INITIAL); YY_OUTPUT(string); YY_OUTPUT(htmlmodify_yytext); }

<TAG>{W}+                   { YY_OUTPUT(htmlmodify_yytext); }
<TAG>"/>"                   { BEGIN(after_tag);                              htmlmodify_yylval=""; return(LEX_TAG_END_XHTML); }
<TAG>">"                    { BEGIN(after_tag);                              htmlmodify_yylval=""; return(LEX_TAG_END); }
<TAG>"<"                    { BEGIN(after_tag); unput(htmlmodify_yytext[0]); htmlmodify_yylval=""; return(LEX_TAG_END); }
<TAG>{key}                  { BEGIN(TAG_ATTR_KEY); htmlmodify_yylval=htmlmodify_yytext; return(LEX_ATTR_KEY); }
<TAG>(.|\n)                 { YY_OUTPUT(htmlmodify_yytext); }

<TAG_ATTR_KEY>{W}*=         { BEGIN(TAG_ATTR_VAL); }
<TAG_ATTR_KEY>(.|\n)        { BEGIN(TAG); unput(htmlmodify_yytext[0]); htmlmodify_yylval=NULL; return(LEX_ATTR_VAL); }

<TAG_ATTR_VAL>\"            { BEGIN(DQUOTED); reset_string; }
<TAG_ATTR_VAL>\'            { BEGIN(SQUOTED); reset_string; }
<TAG_ATTR_VAL>{W}+          { }
<TAG_ATTR_VAL>{val}         { BEGIN(TAG);                              htmlmodify_yylval=htmlmodify_yytext; return(LEX_ATTR_VAL); }
<TAG_ATTR_VAL>(.|\n)        { BEGIN(TAG); unput(htmlmodify_yytext[0]); htmlmodify_yylval="";                return(LEX_ATTR_VAL); }

 /* Quoted strings */

<DQUOTED>\\\"               { append_string(htmlmodify_yytext); }
<DQUOTED>\\                 { append_string(htmlmodify_yytext); }
<DQUOTED>\"                 { BEGIN(TAG); htmlmodify_yylval=string; return(LEX_ATTR_VAL_DQ); }
<DQUOTED>[\r\n]+            { }
<DQUOTED>[^\\\"\r\n]+       { append_string(htmlmodify_yytext); }

<SQUOTED>\\\'               { append_string(htmlmodify_yytext); }
<SQUOTED>\\                 { append_string(htmlmodify_yytext); }
<SQUOTED>\'                 { BEGIN(TAG); htmlmodify_yylval=string; return(LEX_ATTR_VAL_SQ); }
<SQUOTED>[\r\n]+            { }
<SQUOTED>[^\\\'\r\n]+       { append_string(htmlmodify_yytext); }

 /* Scripts */

<SCRIPT_START>(.|\n)        { disable_output^=DISABLE_PARSE; YY_OUTPUT("\n<!-- WWWOFFLE (disable-script) - ... -->\n"); disable_output^=DISABLE_PARSE;
                              unput(htmlmodify_yytext[0]); BEGIN(SCRIPT); }

<SCRIPT>\"                  { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_DQUOTED); }
<SCRIPT>\'                  { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_SQUOTED); }
<SCRIPT>"/*"                { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_COMMENT_C); }
<SCRIPT>"//"                { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_COMMENT_CPP); }
<SCRIPT>"*"                 { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT>"/"                 { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT>"<"/"/script"       { BEGIN(TAG_START); }
<SCRIPT>"<"                 { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT>[^\'\"</]+          { YY_OUTPUT(htmlmodify_yytext); }

 /* Comments in scripts, C or C++ style */

<SCRIPT_COMMENT_C>"*"+"/"   { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_COMMENT_C>"*"+      { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_COMMENT_C>[^*]+     { YY_OUTPUT(htmlmodify_yytext); }

<SCRIPT_COMMENT_CPP>[\r\n]  { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_COMMENT_CPP>"<"/"/script" { BEGIN(TAG_START); }
<SCRIPT_COMMENT_CPP>"<"     { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_COMMENT_CPP>[^\r\n<]+ { YY_OUTPUT(htmlmodify_yytext); }

 /* Quoted strings in scripts - assume they follow usual rules */

<SCRIPT_DQUOTED>\\\"        { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_DQUOTED>\\          { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_DQUOTED>\"          { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_DQUOTED>[^\\\"]+    { YY_OUTPUT(htmlmodify_yytext); }

<SCRIPT_SQUOTED>\\\'        { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_SQUOTED>\\          { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_SQUOTED>\'          { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_SQUOTED>[^\\\']+    { YY_OUTPUT(htmlmodify_yytext); }

 /* Styles */

<STYLE_START>(.|\n)         { disable_output^=DISABLE_PARSE; YY_OUTPUT("\n<!-- WWWOFFLE (disable-style) - ... -->\n"); disable_output^=DISABLE_PARSE;
                              unput(htmlmodify_yytext[0]); BEGIN(STYLE); }

<STYLE>"<"/"/style"         { BEGIN(TAG_START); }
<STYLE>"<"                  { YY_OUTPUT(htmlmodify_yytext); }
<STYLE>[^<]+                { YY_OUTPUT(htmlmodify_yytext); }

 /* End of file */

<<EOF>>                     { free(string); string=NULL; stringlen=stringused=0;
                              after_tag=INITIAL; BEGIN(INITIAL); return(0); }

%%
