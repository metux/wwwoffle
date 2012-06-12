/***************************************
 $Header$

 Document parser test program
 ******************/ /******************
 Written by Andrew M. Bishop

 This file Copyright 1998,99,2000,01,03 Andrew M. Bishop
 It may be distributed under the GNU Public License, version 2, or
 any higher version.  See section COPYING of the GNU Public license
 for conditions under which this file may be redistributed.
 ***************************************/

#include <stdio.h>
#include <unistd.h>

#include "document.h"
#include "misc.h"
#include "io.h"
#include "config.h"
#include "errors.h"


int main(int argc,char **argv)
{
 URL *Url;
 URL **list,*refresh;
 int j;

 if(argc==1)
   {fprintf(stderr,"usage: test-doc URL < contents-of-url\n");return(1);}

 StderrLevel=ExtraDebug;

 InitErrorHandler("test-doc",0,1);

 InitConfigurationFile("./wwwoffle.conf");

 init_io(STDERR_FILENO);

 if(ReadConfigurationFile(STDERR_FILENO))
    PrintMessage(Fatal,"Error in configuration file 'wwwoffle.conf'.");

 finish_io(STDERR_FILENO);

 Url=SplitURL(argv[1]);

 init_io(0);

 ParseDocument(0,Url,1);

 if((refresh=GetReference(RefMetaRefresh)))
    printf("Refresh = %s\n",refresh->file);

 if((list=GetReferences(RefStyleSheet)))
    for(j=0;list[j];j++)
       printf("StyleSheet = %s\n",list[j]->file);

 if((list=GetReferences(RefImage)))
    for(j=0;list[j];j++)
       printf("Image = %s\n",list[j]->file);

 if((list=GetReferences(RefFrame)))
    for(j=0;list[j];j++)
       printf("Frame = %s\n",list[j]->file);

 if((list=GetReferences(RefScript)))
    for(j=0;list[j];j++)
       printf("Script = %s\n",list[j]->file);

 if((list=GetReferences(RefObject)))
    for(j=0;list[j];j++)
       printf("Object = %s\n",list[j]->file);

 if((list=GetReferences(RefInlineObject)))
    for(j=0;list[j];j++)
       printf("InlineObject = %s\n",list[j]->file);

 if((list=GetReferences(RefLink)))
    for(j=0;list[j];j++)
       printf("Link = %s\n",list[j]->file);

 FreeURL(Url);

 finish_io(0);

 return(0);
}
