/***************************************
  $Header$

  HTML modification test program
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 2000,01 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/

#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "document.h"
#include "misc.h"
#include "io.h"
#include "errors.h"


ssize_t wwwoffles_read_data(char *data,int len);
ssize_t wwwoffles_write_data(char *data,int len);

int main(int argc,char **argv)
{
 URL *Url;

 if(argc==1)
   {fprintf(stderr,"usage: test-modify URL < contents-of-url\n");return(1);}

 StderrLevel=ExtraDebug;

 InitErrorHandler("test-modify",0,1);

 InitConfigurationFile("./wwwoffle.conf");

 init_io(STDERR_FILENO);

 if(ReadConfigurationFile(STDERR_FILENO))
    PrintMessage(Fatal,"Error in configuration file 'wwwoffle.conf'.");

 finish_io(STDERR_FILENO);

 Url=SplitURL(argv[1]);

 init_io(0);
 init_io(1);

 OutputHTMLWithModifications(Url,0,"text/html");

 FreeURL(Url);

 finish_io(0);
 finish_io(1);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from the input.

  ssize_t wwwoffles_read_data Returns the number of bytes read.

  char *data The data buffer to fill in.

  int len The length of the buffer to fill in.

  This function is used as a callback from gifmodify.c and htmlmodify.l
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t wwwoffles_read_data(char *data,int len)
{
 int n=read_data(0,data,len);

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  Write data to the output.

  ssize_t wwwoffles_write_data Returns the number of bytes written.

  char *data The data to write.

  int len The number of bytes to write.

  This function is used as a callback from gifmodify.c and htmlmodify.l
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t wwwoffles_write_data(char *data,int len)
{
 write_data(1,data,len);

 return(len);
}
