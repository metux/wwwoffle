/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/gifmodify.c 1.12 2005/03/13 13:55:34 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  A function to modify GIFs by deleting all except the first image.
  ******************/ /******************
  Written by Andrew M. Bishop

  Copyright 2004,05 Andrew M. Bishop

  This file may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include "wwwoffle.h"
#include "io.h"
#include "document.h"


/*+ Part types +*/

typedef enum _GIFParts
{
 Unknown          = 0,          /*+ Don't know what comes next. +*/

 CopyToEnd        = 1,          /*+ Copy the remaining data until the end. +*/
 SkipToEnd        = 2,          /*+ Skip the remaining data until the end. +*/

 GIFHeader        = 3,          /*+ The initial header signature + version. +*/
 ScreenDescriptor = 4,          /*+ The image size and depth etc. +*/
 ImageDescriptor  = 5,          /*+ The individual image ddescriptor +*/

 LZWCodeSize      = 6,          /*+ The LZW code size. +*/
 ImageData        = 7,          /*+ The actual image data. +*/

 Trailer          = 8,          /*+ The final data trailer. +*/

 ExtensionIntro   = 9,          /*+ An extension block introducer byte. +*/
 ExtensionType    =10,          /*+ An extension block type byte. +*/
 ExtensionData    =11           /*+ The extension block data. +*/
}
GIFBlock;


/*++++++++++++++++++++++++++++++++++++++
  Disable the animation of GIF87a & GIF89a files.
  The output GIF is only the first image in a multi-image GIF.
  ++++++++++++++++++++++++++++++++++++++*/

void OutputGIFWithModifications(void)
{
 GIFBlock state=GIFHeader;
 unsigned int count=0,remain=0;
 char data[IO_BUFFER_SIZE];
 int n;

 /* The contents of the packed byte in the Screen/ImageDescriptor */

 char packed=0;

 /* Loop until finished reading */

 while((n=wwwoffles_read_data(data,IO_BUFFER_SIZE))>0)
   {
    char *p=data;

    while(n)
      {
       /* Ignore the remaining data */

       if(state==SkipToEnd)
          break;

       /* Copy the remaining data */

       else if(state==CopyToEnd)
         {
          wwwoffles_write_data(p,n);
          break;
         }

       /* Output the following 'remain' bytes. */

       else if(remain>=(unsigned)n)
         {
          wwwoffles_write_data(p,n);
          remain-=n;
          break;
         }
       else if(remain)
         {
          wwwoffles_write_data(p,remain);
          p+=remain;
          n-=remain;
          remain=0;
         }

       /* Decide what data comes next */

       if(state==Unknown && n)
         {
          if(*p==0x2c)
            {
             state=ImageDescriptor;
             count=0;
            }
          else if(*p==0x21)
             state=ExtensionIntro;
          else if(*p==0x3b)
             state=Trailer;
          else
             state=LZWCodeSize;
         }

       /* Header, first in file, appears only once */

       if(state==GIFHeader && n)
         {
          int m=0;
          switch(count)
            {
            case 0:                 if(p[m]!='G')              goto copytoend; m++; /*@fallthrough@*/
            case 1: if(m>=n) break; if(p[m]!='I')              goto copytoend; m++; /*@fallthrough@*/
            case 2: if(m>=n) break; if(p[m]!='F')              goto copytoend; m++; /*@fallthrough@*/
            case 3: if(m>=n) break; if(p[m]!='8')              goto copytoend; m++; /*@fallthrough@*/
            case 4: if(m>=n) break; if(p[m]!='7' && p[m]!='9') goto copytoend; m++; /*@fallthrough@*/
            case 5: if(m>=n) break; if(p[m]!='a')              goto copytoend; m++;
             state=ScreenDescriptor;
             count=0;
            }

          if(state==GIFHeader)
             count+=m;
          wwwoffles_write_data(p,m);
          p+=m;
          n-=m;
         }

       /* Screen Descriptor, appears only once after Header */

       if(state==ScreenDescriptor && n)
         {
          int m=0;
          switch(count)
            {
            case 0:                 m++;           /*@fallthrough@*/
            case 1: if(m>=n) break; m++;           /*@fallthrough@*/
            case 2: if(m>=n) break; m++;           /*@fallthrough@*/
            case 3: if(m>=n) break; m++;           /*@fallthrough@*/
            case 4: if(m>=n) break; packed=p[m++]; /*@fallthrough@*/
            case 5: if(m>=n) break; m++;           /*@fallthrough@*/
            case 6: if(m>=n) break; m++;
             if(packed&0x80)
                remain=3*(2<<(packed&0x07));
             else
                remain=0;

             state=Unknown;
            }

          if(state==ScreenDescriptor)
             count+=m;
          wwwoffles_write_data(p,m);
          p+=m;
          n-=m;
         }

       /* Global Colour table, appears only once, optional, skipped, after Screen Descriptor */

       /* Image descriptor, appears once per image  */

       if(state==ImageDescriptor && n)
         {
          int m=0;
          switch(count)
            {
            case 0:                 m++;           /*@fallthrough@*/
            case 1: if(m>=n) break; m++;           /*@fallthrough@*/
            case 2: if(m>=n) break; m++;           /*@fallthrough@*/
            case 3: if(m>=n) break; m++;           /*@fallthrough@*/
            case 4: if(m>=n) break; m++;           /*@fallthrough@*/
            case 5: if(m>=n) break; m++;           /*@fallthrough@*/
            case 6: if(m>=n) break; m++;           /*@fallthrough@*/
            case 7: if(m>=n) break; m++;           /*@fallthrough@*/
            case 8: if(m>=n) break; m++;           /*@fallthrough@*/
            case 9: if(m>=n) break; packed=p[m++];
             if(packed&0x80)
                remain=3*(2<<(packed&0x07));
             else
                remain=0;

             state=Unknown;
            }

          if(state==ImageDescriptor)
             count+=m;
          wwwoffles_write_data(p,m);
          p+=m;
          n-=m;
         }

       /* Local Colour table, appears once per image, optional, skipped, after Image Descriptor */

       /* Image block start, one per image */

       if(state==LZWCodeSize && n)
         {
          wwwoffles_write_data(p,1);
          p++;
          n--;
          state=ImageData;
         }

       /* Image data, one per image */

       if(state==ImageData && n)
         {
          remain=(unsigned char)*p;

#if 1 /* single image wanted */
          if(remain==0)
             state=Trailer;
#else /* pass through */
          if(remain==0)
             state=Unknown;
#endif

          wwwoffles_write_data(p,1);
          p++;
          n--;
         }

       /* Trailer, appears once at end */

       if(state==Trailer && n)
         {
          char end=0x3b;
          wwwoffles_write_data(&end,1);
          state=SkipToEnd;
         }

       /* Extension Introducer, optional, appears any number of times */

       if(state==ExtensionIntro && n)
         {
          state=ExtensionType;

          wwwoffles_write_data(p,1);
          p++;
          n--;
         }

       /* Extension type, optional, appears any number of times */

       if(state==ExtensionType && n)
         {
          state=ExtensionData;

          wwwoffles_write_data(p,1);
          p++;
          n--;
         }

       /* Extension data, optional, appears any number of times */

       if(state==ExtensionData && n)
         {
          remain=(unsigned char)*p;
          if(remain==0)
             state=Unknown;

          wwwoffles_write_data(p,1);
          p++;
          n--;
         }

       continue;

      copytoend:
       state=CopyToEnd;
      }
   }
}
