/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/miscencdec.c 1.15 2006/01/15 10:13:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Miscellaneous HTTP / HTML Encoding & Decoding functions.
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

#include "misc.h"
#include "md5.h"


/* To understand why the URLDecode*() and URLEncode*() functions are coded this way see README.URL */

/*+ For conversion from integer to hex string. +*/
static const char hexstring[17]="0123456789ABCDEF";

/*+ For conversion from hex string to integer. +*/
static const unsigned char unhexstring[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                              0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                              0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                              0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */


/*++++++++++++++++++++++++++++++++++++++
  Decode a string that has been UrlEncoded.

  char *URLDecodeGeneric Returns a malloced copy of the decoded string.

  const char *str The generic string to be decoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLDecodeGeneric(const char *str)
{
 int i,j;
 char *copy=(char*)malloc(strlen(str)+1);

 for(i=0,j=0;str[i];i++)
    if(str[i]=='%' && str[i+1]!='\0' && str[i+2]!='\0')
      {
       unsigned char val=0;
       val=unhexstring[(unsigned char)str[++i]]<<4;
       val+=unhexstring[(unsigned char)str[++i]];
       copy[j++]=val;
      }
    else
       copy[j++]=str[i];

 copy[j]=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Decode a POSTed form or URL arguments that has been UrlEncoded.

  char *URLDecodeFormArgs Returns a malloced copy of the decoded form data string.

  const char *str The form data string to be decoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLDecodeFormArgs(const char *str)
{
 int i,j;
 char *copy=(char*)malloc(strlen(str)+1);

 /* This is the same as the generic function except that '+' is used for ' ' instead of '%20'. */

 for(i=0,j=0;str[i];i++)
    if(str[i]=='%' && str[i+1]!='\0' && str[i+2]!='\0')
      {
       unsigned char val=0;
       val=unhexstring[(unsigned char)str[++i]]<<4;
       val+=unhexstring[(unsigned char)str[++i]];
       copy[j++]=val;
      }
    else if(str[i]=='+')
       copy[j++]=' ';
    else
       copy[j++]=str[i];

 copy[j]=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Take a POSTed form data or a URL argument string and URLDecode and URLEncode it again.

  char *URLRecodeFormArgs Returns a malloced copy of the decoded and re-encoded form/argument string.

  const char *str The form/argument string to be decoded and re-encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLRecodeFormArgs(const char *str)
{
 int i,j;
 char *copy=(char*)malloc(3*strlen(str)+1);

 /*
   The characters in the range 0x00-0x1f and 0x7f-0xff are always disallowed.
   The '%' character is always disallowed because it is the quote character.
   RFC 1738 section 2.2 calls " <>"#%{}|\^~[]`" unsafe characters, I make an exception for "|~".
   RFC 1738 section 2.2 calls ";/?:@=&" reserved characters, I make an exception for "/:".
   RFC 1866 section 8.2.1 says that ' ' is replaced by '+'.
   I disallow "'" because it may lead to confusion.
   The unencoded characters "&=;" on the input are left unencoded on the output
   The encoded character "+" on the input is left encoded on the output
   The unencoded character "?" on the input is left unencoded on the output to handle broken servers.
 */

 static const char allowed[257]=
 "                                "  /* 0x00-0x1f "                                " */
 " !  $   ()* ,-./0123456789:     "  /* 0x20-0x3f " !"#$%&'()*+,-./0123456789:;<=>?" */
 " ABCDEFGHIJKLMNOPQRSTUVWXYZ    _"  /* 0x40-0x5f "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_" */
 " abcdefghijklmnopqrstuvwxyz | ~ "  /* 0x60-0x7f "`abcdefghijklmnopqrstuvwxyz{|}~ " */
 "                                "  /* 0x80-0x9f "                                " */
 "                                "  /* 0xa0-0xbf "                                " */
 "                                "  /* 0xc0-0xdf "                                " */
 "                                "; /* 0xe0-0xff "                                " */

 for(i=0,j=0;str[i];i++)
    if(allowed[(unsigned char)str[i]]!=' ' || str[i]=='&' || str[i]=='=' || str[i]==';' || str[i]=='+' || str[i]=='?')
       copy[j++]=str[i];
    else if(str[i]=='%' && str[i+1]!='\0' && str[i+2]!='\0')
      {
       unsigned char val=0;
       val=unhexstring[(unsigned char)str[++i]]<<4;
       val+=unhexstring[(unsigned char)str[++i]];

       if(allowed[val]!=' ')
          copy[j++]=val;
       else if(val==' ')
          copy[j++]='+';
       else
         {
          copy[j++]='%';
          copy[j++]=toupper(str[i-1]);
          copy[j++]=toupper(str[i]);
         }
      }
    else if(str[i]==' ')
       copy[j++]='+';
    else
      {
       unsigned char val=str[i];
       copy[j]='%';
       copy[j+2]=hexstring[val%16];
       val>>=4;
       copy[j+1]=hexstring[val%16];
       j+=3;
      }

 copy[j]=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Encode a string suitable for use as a pathname.

  char *URLEncodePath Returns a malloced copy of the encoded pathname string.

  const char *str The pathname string to be encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLEncodePath(const char *str)
{
 int i,j;
 char *copy=(char*)malloc(3*strlen(str)+1);

 /*
   The characters in the range 0x00-0x1f and 0x7f-0xff are always disallowed.
   The '%' character is always disallowed because it is the quote character.
   RFC 1738 section 2.2 calls " <>"#%{}|\^~[]`" unsafe characters, I make an exception for '~'.
   RFC 1738 section 2.2 calls ";/?:@=&" reserved characters, I make an exception for ";/:=".
   I disallow "'" because it may lead to confusion.
 */

 static const char allowed[257]=
 "                                "  /* 0x00-0x1f "                                " */
 " !  $   ()*+,-./0123456789:; =  "  /* 0x20-0x3f " !"#$%&'()*+,-./0123456789:;<=>?" */
 " ABCDEFGHIJKLMNOPQRSTUVWXYZ    _"  /* 0x40-0x5f "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_" */
 " abcdefghijklmnopqrstuvwxyz   ~ "  /* 0x60-0x7f "`abcdefghijklmnopqrstuvwxyz{|}~ " */
 "                                "  /* 0x80-0x9f "                                " */
 "                                "  /* 0xa0-0xbf "                                " */
 "                                "  /* 0xc0-0xdf "                                " */
 "                                "; /* 0xe0-0xff "                                " */

 for(i=0,j=0;str[i];i++)
    if(allowed[(unsigned char)str[i]]!=' ')
       copy[j++]=str[i];
    else
      {
       unsigned char val=str[i];
       copy[j]='%';
       copy[j+2]=hexstring[val%16];
       val>>=4;
       copy[j+1]=hexstring[val%16];
       j+=3;
      }

 copy[j]=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Encode a string suitable for use with the POST method or as URL arguments.

  char *URLEncodeFormArgs Returns a malloced copy of the encoded form data string.

  const char *str The form data string to be encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLEncodeFormArgs(const char *str)
{
 int i,j;
 char *copy=(char*)malloc(3*strlen(str)+1);

 /*
   The characters in the range 0x00-0x1f and 0x7f-0xff are always disallowed.
   The '%' character is always disallowed because it is the quote character.
   RFC 1738 section 2.2 calls " <>"#%{}|\^~[]`" unsafe characters, I make an exception for "|~".
   RFC 1738 section 2.2 calls ";/?:@=&" reserved characters, I make an exception for "/:".
   RFC 1866 section 8.2.1 says that ' ' is replaced by '+'.
   I disallow ""'\`" because they may lead to confusion.
 */

 static const char allowed[257]=
 "                                "  /* 0x00-0x1f "                                " */
 " !  $   ()* ,-./0123456789:     "  /* 0x20-0x3f " !"#$%&'()*+,-./0123456789:;<=>?" */
 " ABCDEFGHIJKLMNOPQRSTUVWXYZ    _"  /* 0x40-0x5f "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_" */
 " abcdefghijklmnopqrstuvwxyz | ~ "  /* 0x60-0x7f "`abcdefghijklmnopqrstuvwxyz{|}~ " */
 "                                "  /* 0x80-0x9f "                                " */
 "                                "  /* 0xa0-0xbf "                                " */
 "                                "  /* 0xc0-0xdf "                                " */
 "                                "; /* 0xe0-0xff "                                " */

 for(i=0,j=0;str[i];i++)
    if(allowed[(unsigned char)str[i]]!=' ')
       copy[j++]=str[i];
    else if(str[i]==' ')
       copy[j++]='+';
    else
      {
       unsigned char val=str[i];
       copy[j]='%';
       copy[j+2]=hexstring[val%16];
       val>>=4;
       copy[j+1]=hexstring[val%16];
       j+=3;
      }

 copy[j]=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Encode a string suitable for use as a username / password in a URL.

  char *URLEncodePassword Returns a malloced copy of the encoded username / password string.

  const char *str The password / username string to be encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLEncodePassword(const char *str)
{
 int i,j;
 char *copy=(char*)malloc(3*strlen(str)+1);

 /*
   The characters in the range 0x00-0x1f and 0x7f-0xff are always disallowed.
   The '%' character is always disallowed because it is the quote character.
   RFC 1738 section 2.2 calls " <>"#%{}|\^~[]`" unsafe characters, I make an exception for '~'.
   RFC 1738 section 2.2 calls ";/?:@=&" reserved characters, I make an exception for "=".
   RFC 1738 section 3.1 says that "@:/" are disallowed.
   I disallow "'()" because they may lead to confusion.
 */

 static const char allowed[257]=
 "                                "  /* 0x00-0x1f "                                " */
 " !  $     *+,-. 0123456789   =  "  /* 0x20-0x3f " !"#$%&'()*+,-./0123456789:;<=>?" */
 " ABCDEFGHIJKLMNOPQRSTUVWXYZ    _"  /* 0x40-0x5f "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_" */
 " abcdefghijklmnopqrstuvwxyz   ~ "  /* 0x60-0x7f "`abcdefghijklmnopqrstuvwxyz{|}~ " */
 "                                "  /* 0x80-0x9f "                                " */
 "                                "  /* 0xa0-0xbf "                                " */
 "                                "  /* 0xc0-0xdf "                                " */
 "                                "; /* 0xe0-0xff "                                " */

 for(i=0,j=0;str[i];i++)
    if(allowed[(unsigned char)str[i]]!=' ')
       copy[j++]=str[i];
    else
      {
       unsigned char val=str[i];
       copy[j]='%';
       copy[j+2]=hexstring[val%16];
       val>>=4;
       copy[j+1]=hexstring[val%16];
       j+=3;
      }

 copy[j]=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Split a form body or URL arguments up into the component parts.

  char **SplitFormArgs Returns an array of pointers into a copy of the string.

  const char *str The form data or arguments to split.
  ++++++++++++++++++++++++++++++++++++++*/

char **SplitFormArgs(const char *str)
{
 char **args=(char**)malloc(10*sizeof(char*));
 char *p;
 int i=0;

 args[0]=(char*)malloc(strlen(str)+1);
 strcpy(args[0],str);

 p=args[0];
 while(*p)
   {
    if(*p=='&' || *p==';')
      {
       *p=0;
       if(i && !(i%8))
          args=(char**)realloc((void*)args,(i+10)*sizeof(char*));
       args[++i]=p+1;
      }
    p++;
   }

 args[++i]=NULL;

 return(args);
}


/*++++++++++++++++++++++++++++++++++++++
  Trim any leading or trailing whitespace from an argument string

  char *TrimArgs The string to modify

  char *str The modified string (modifications made in-place).
  ++++++++++++++++++++++++++++++++++++++*/

char *TrimArgs(char *str)
{
 char *l=str,*r=str;

 if(isspace(*l))
   {
    l++;

    while(isspace(*l))
       l++;

    while(*l)
       *r++=*l++;

    *r--=0;
   }
 else
    r=str+strlen(str)-1;

 if(r>=str && isspace(*r))
   {
    *r--=0;
    while(r>=str && isspace(*r))
       *r--=0;
   }

 return(str);
}


/*++++++++++++++++++++++++++++++++++++++
  Generate a hash value for a string.

  char *MakeHash Returns a string that can be used as the hashed string.

  const char *args The arguments.
  ++++++++++++++++++++++++++++++++++++++*/

char *MakeHash(const char *args)
{
 char md5[17];
 char *hash,*p;
 struct MD5Context ctx;

 /* Initialize the computation context.  */
 MD5Init (&ctx);

 /* Process whole buffer but last len % 64 bytes.  */
 MD5Update (&ctx, (const unsigned char*)args, (unsigned)strlen(args));

 /* Put result in desired memory area.  */
 MD5Final ((unsigned char *)md5, &ctx);

 md5[16]=0;

 hash=Base64Encode(md5,(size_t)16);

 for(p=hash;*p;p++)
    if(*p=='/')
       *p='-';
    else if(*p=='=')
       *p=0;

 return(hash);
}


/*+ Conversion from time_t to date string and back (day of week). +*/
static const char* const weekdays[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

/*+ Conversion from time_t to date string and back (month of year). +*/
static const char* const months[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};


/*++++++++++++++++++++++++++++++++++++++
  Convert the time into an RFC 822 compliant date.

  char *RFC822Date Returns a pointer to a fixed string containing the date.

  time_t t The time.

  int utc Set to true to get Universal Time, else localtime.
  ++++++++++++++++++++++++++++++++++++++*/

char *RFC822Date(time_t t,int utc)
{
 static char value[4][32];
 static int which=0;
 char weekday[4];
 char month[4];
 struct tm *tim;

 if(utc) /* Get UTC using English language. */
   {
    tim=gmtime(&t);

    strcpy(weekday,weekdays[tim->tm_wday]);
    strcpy(month,months[tim->tm_mon]);
   }
 else /* Get the local time using current language. */
   {
    tim=localtime(&t);

    if(tim->tm_isdst<0)
      {tim=gmtime(&t);utc=1;}

    strftime(weekday,(size_t)4,"%a",tim);
    strftime(month,(size_t)4,"%b",tim);
   }

 which=(which+1)%4;

 /* Sun, 06 Nov 1994 08:49:37 GMT    ; RFC 822, updated by RFC 1123 */

 sprintf(value[which],"%3s, %02d %3s %4d %02d:%02d:%02d %s",
         weekday,
         tim->tm_mday,
         month,
         tim->tm_year+1900,
         tim->tm_hour,
         tim->tm_min,
         tim->tm_sec,
#if defined(HAVE_TM_ZONE)
         utc?"GMT":tim->tm_zone
#elif defined(HAVE_TZNAME)
         utc?"GMT":tzname[tim->tm_isdst>0]
#elif defined(__CYGWIN__)
         utc?"GMT":_tzname[tim->tm_isdst>0]
#else
         utc?"GMT":"???"
#endif
         );

 return(value[which]);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a string representing a date into a time.

  long DateToTimeT Returns the time.

  const char *date The date string.
  ++++++++++++++++++++++++++++++++++++++*/

long DateToTimeT(const char *date)
{
 int  year,day,hour,min,sec;
 char monthstr[4];
 long retval=0;

 if(sscanf(date,"%*s %d %3s %d %d:%d:%d",&day,monthstr,&year,&hour,&min,&sec)==6 ||
    sscanf(date,"%*s %d-%3s-%d %d:%d:%d",&day,monthstr,&year,&hour,&min,&sec)==6 ||
    sscanf(date,"%*s %3s %d %d:%d:%d %d",monthstr,&day,&hour,&min,&sec,&year)==6)
   {
    char *old_tz=NULL,*env;
    struct tm tim;
    int mon;

    for(mon=0;mon<12;mon++)
       if(!strcmp(monthstr,months[mon]))
          break;

    tim.tm_sec=sec;
    tim.tm_min=min;
    tim.tm_hour=hour;
    tim.tm_mday=day;
    tim.tm_mon=mon;
    if(year<38)
       tim.tm_year=year+100;
    else if(year<100)
       tim.tm_year=year;
    else
       tim.tm_year=year-1900;
    tim.tm_isdst=0;
    tim.tm_wday=0;              /* unused */
    tim.tm_yday=0;              /* unused */

    if((env=getenv("TZ")))
      {
       old_tz=(char*)malloc(strlen(env)+4);
       strcpy(old_tz,"TZ=");
       strcat(old_tz,env);
      }

    putenv("TZ=GMT");
    tzset();

    retval=mktime(&tim);

    if(old_tz)
       putenv(old_tz);
    else
       putenv("TZ");
    tzset();

    /*
      Who decided on this broken behaviour?!?
      (as described in the Linux manual page for putenv)

           The libc4 and libc5 and glibc 2.1.2  versions  conform  to
           SUSv2:  the  pointer string given to putenv() is used.  In
           particular, this string becomes part of  the  environment;
           changing  it later will change the environment.  (Thus, it
           is an error is to call putenv() with an automatic variable
           as  the  argument,  then  return from the calling function
           while string is still part of the environment.)   However,
           glibc 2.0-2.1.1 differs: a copy of the string is used.  On
           the one hand this causes a memory leak, and on  the  other
           hand it violates SUSv2. This has been fixed in glibc2.1.2.

           The BSD4.4 version, like glibc 2.0, uses a copy.

      It means that I can't free the memory like I wanted to.

    if(old_tz)
       free(old_tz);
    */

    if(retval==-1)
       retval=0;
   }
 else
    sscanf(date,"%ld %1s",&retval,monthstr);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Make up a string that contains a duration (in seconds) in human readable format.

  char *DurationToString Returns a (static, one of two) string.

  const time_t duration The duration in seconds.
  ++++++++++++++++++++++++++++++++++++++*/

char *DurationToString(const time_t duration)
{
 static int which=0;
 static char string[2][64];
 int n=0;
 time_t weeks,days,hours,minutes,seconds=duration;

 weeks=seconds/(3600*24*7);
 seconds-=(3600*24*7)*weeks;

 days=seconds/(3600*24);
 seconds-=(3600*24)*days;

 hours=seconds/(3600);
 seconds-=(3600)*hours;

 minutes=seconds/(60);
 seconds-=(60)*minutes;

 which^=1;

 if(weeks>4)
    n+=sprintf(string[which]+n,"%dw",(unsigned)weeks);
 else
   {days+=7*weeks;weeks=0;}

 if(days)
    n+=sprintf(string[which]+n,"%s%dd",weeks?" ":"",(unsigned)days);

 if(hours || minutes || seconds)
   {
    if(days || weeks)
       n+=sprintf(string[which]+n," ");
    if(hours && !minutes && !seconds)
       n+=sprintf(string[which]+n,"%dh",(unsigned)hours);
    else if(!seconds)
       n+=sprintf(string[which]+n,"%02d:%02d",(unsigned)hours,(unsigned)minutes);
    else
       n+=sprintf(string[which]+n,"%02d:%02d:%02d",(unsigned)hours,(unsigned)minutes,(unsigned)seconds);
   }

 sprintf(string[which]+n," (%ds)",(unsigned)duration);

 return(string[which]);
}


/*+ The conversion from a 6 bit value to an ASCII character. +*/
static const char base64[64]={'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
                              'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
                              'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
                              'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'};

/*++++++++++++++++++++++++++++++++++++++
  Decode a base 64 string.

  char *Base64Decode Return a malloced string containing the decoded version.

  const char *str The string to be decoded.

  size_t *l Returns the length of the decoded string.
  ++++++++++++++++++++++++++++++++++++++*/

char *Base64Decode(const char *str,size_t *l)
{
 size_t le=strlen(str);
 char *decoded=(char*)malloc(le+1);
 int i,j,k;

 while(str[le-1]=='=')
    le--;

 *l=3*(le/4)+(le%4)-1+!(le%4);

 for(j=0;j<le;j++)
    for(k=0;k<64;k++)
       if(base64[k]==str[j])
         {decoded[j]=k;break;}

 for(i=j=0;j<(le+4);i+=3,j+=4)
   {
    unsigned long s=0;

    for(k=0;k<4;k++)
       if((j+k)<le)
          s|=((unsigned long)decoded[j+k]&0xff)<<(18-6*k);

    for(k=0;k<3;k++)
       if((i+k)<*l)
          decoded[i+k]=(char)((s>>(16-8*k))&0xff);
   }
 decoded[*l]=0;

 return(decoded);
}


/*++++++++++++++++++++++++++++++++++++++
  Encode a string into base 64.

  char *Base64Encode Return a malloced string containing the encoded version.

  const char *str The string to be encoded.

  size_t l The length of the string to be encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *Base64Encode(const char *str,size_t l)
{
 size_t le=4*(l/3)+(l%3)+!!(l%3);
 char *encoded=(char*)malloc(4*(le/4)+4*!!(le%4)+1);
 int i,j,k;

 for(i=j=0;i<(l+3);i+=3,j+=4)
   {
    unsigned long s=0;

    for(k=0;k<3;k++)
       if((i+k)<l)
          s|=((unsigned long)str[i+k]&0xff)<<(16-8*k);

    for(k=0;k<4;k++)
       if((j+k)<le)
          encoded[j+k]=(char)((s>>(18-6*k))&0x3f);
   }

 for(j=0;j<le;j++)
    encoded[j]=base64[(int)encoded[j]];
 for(;j%4;j++)
    encoded[j]='=';
 encoded[j]=0;

 return(encoded);
}


/*++++++++++++++++++++++++++++++++++++++
  Replace all occurences of '&amp;' with '&' by modifying the string in place.

  char *string The string to be modified.
  ++++++++++++++++++++++++++++++++++++++*/

void URLReplaceAmp(char *string)
{
 char *q=string,*p=string;

 while(*p)
   {
    if(*p=='&' &&
       (*(p+1)=='a' || *(p+1)=='A') &&
       (*(p+2)=='m' || *(p+2)=='M') &&
       (*(p+3)=='p' || *(p+3)=='P') &&
       *(p+4)==';')
      {
       *q++=*p++;
       p+=4;
      }
    else
      {
       *q++=*p++;
      }
   }

 *q=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Make the input string safe to output as HTML ( not < > & " ).

  char* HTMLString Returns a safe HTML string.

  const char* c A non-safe HTML string.

  int nbsp Use a non-breaking space in place of normal ones.
  ++++++++++++++++++++++++++++++++++++++*/

char* HTMLString(const char* c,int nbsp)
{
 int i=0,j=0,len=256-5;              /* 5 is the longest possible inserted amount */
 char* ret=(char*)malloc((size_t)257);

 do
   {
    for(;j<len && c[i];i++)
       switch(c[i])
         {
         case '<':
          ret[j++]='&';
          ret[j++]='l';
          ret[j++]='t';
          ret[j++]=';';
          break;
         case '>':
          ret[j++]='&';
          ret[j++]='g';
          ret[j++]='t';
          ret[j++]=';';
          break;
         case '&':
          ret[j++]='&';
          ret[j++]='a';
          ret[j++]='m';
          ret[j++]='p';
          ret[j++]=';';
          break;
         case ' ':
          if(nbsp)
            {
             ret[j++]='&';
             ret[j++]='n';
             ret[j++]='b';
             ret[j++]='s';
             ret[j++]='p';
             ret[j++]=';';
             break;
            }
         /*@ fallthrough @*/
         default:
          ret[j++]=c[i];
         }

    if(c[i])                 /* Not finished */
      {
       ret=(char*)realloc((void*)ret,len+256+5);
       len+=256;
      }
   }
 while(c[i]);

 ret[j]=0;

 return(ret);
}
