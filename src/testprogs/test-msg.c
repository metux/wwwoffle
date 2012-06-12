/***************************************
  $Header$

  Messages parser test program
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,2000,01 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/

#include <stdio.h>

#include "wwwoffle.h"


int main(int argc,char **argv)
{
 char *var[5],*val[5];
 int i;

 if(argc==1)
   {fprintf(stderr,"usage: test-msg <Page> [<variable>=<value>] ...\n");return(1);}

 for(i=2;i<argc;i++)
   {
    char *p=argv[i];

    var[i-2]=argv[i];

    while(*p && *p!='=')
       p++;

    if(*p)
      {
       *p=0;
       val[i-2]=++p;
      }
    else
       val[i-2]=NULL;
   }

 switch(argc)
   {
   case 2:
    HTMLMessageBody(1,argv[1],
                    NULL);
    break;

   case 3:
    HTMLMessageBody(1,argv[1],
                    var[0],val[0],
                    NULL);
    break;

   case 4:
    HTMLMessageBody(1,argv[1],
                    var[0],val[0],
                    var[1],val[1],
                    NULL);
    break;

   case 5:
    HTMLMessageBody(1,argv[1],
                    var[0],val[0],
                    var[1],val[1],
                    var[2],val[2],
                    NULL);
    break;

   case 6:
    HTMLMessageBody(1,argv[1],
                    var[0],val[0],
                    var[1],val[1],
                    var[2],val[2],
                    var[3],val[3],
                    NULL);
    break;

   case 7:
    HTMLMessageBody(1,argv[1],
                    var[0],val[0],
                    var[1],val[1],
                    var[2],val[2],
                    var[3],val[3],
                    var[4],val[4],
                    NULL);
    break;

   default:
    fprintf(stderr,"Too many arguments\n");    
    return(1);
   }


 return(0);
}
