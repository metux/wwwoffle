/***************************************
 $Header$

 File IO test program
 ******************/ /******************
 Written by Andrew M. Bishop

 This file Copyright 2003 Andrew M. Bishop
 It may be distributed under the GNU Public License, version 2, or
 any higher version.  See section COPYING of the GNU Public license
 for conditions under which this file may be redistributed.
 ***************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "autoconfig.h"
#include "io.h"
#include "errors.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif


int main(int argc,char **argv)
{
 char infile[32],outfile[32];
 char buffer[IO_BUFFER_SIZE];
 int n;
 int count;

 /* Writing test */

 strcpy(infile,"file.txt");

 for(count=0;count<4;count++)
   {
    int zlib,chunk;
    int read_fd,write_fd;

    zlib =count&1;
    chunk=count&2;

    strcpy(outfile,"write");
    if(zlib) strcat(outfile,"-zlib");
    if(chunk) strcat(outfile,"-chunk");
    strcat(outfile,".txt");

    printf("Writing");
    if(zlib) printf(" with compression");
    if(chunk) printf(" with chunked encoding");
    printf(" %s -> %s\n",infile,outfile);

    read_fd=open(infile,O_RDONLY|O_BINARY);
    if(read_fd==-1)
       PrintMessage(Fatal,"Cannot open '%s' for reading [%!s]",infile);

    write_fd=open(outfile,O_CREAT|O_TRUNC|O_WRONLY|O_BINARY,0644);
    if(write_fd==-1)
       PrintMessage(Fatal,"Cannot open '%s' for writing [%!s]",outfile);

    init_io(write_fd);

#if USE_ZLIB
    if(zlib)
       configure_io_zlib(write_fd,-1,zlib?2:0);
#endif
    if(chunk)
       configure_io_chunked(write_fd,-1,chunk?1:0);

    do
      {
       int size=1+rand()%128;

       n=read(read_fd,buffer,size);
       if(n>0)
          write_data(write_fd,buffer,n);
      }
    while(n>0);

    finish_io(write_fd);
    close(write_fd);

    close(read_fd);
   }

 /* Reading test */

 for(count=0;count<4;count++)
   {
    int zlib,chunk;
    int read_fd,write_fd;

    zlib =count&1;
    chunk=count&2;

    strcpy(infile,"write");
    if(zlib) strcat(infile,"-zlib");
    if(chunk) strcat(infile,"-chunk");
    strcat(infile,".txt");

    strcpy(outfile,"read");
    if(zlib) strcat(outfile,"-zlib");
    if(chunk) strcat(outfile,"-chunk");
    strcat(outfile,".txt");

    printf("Reading");
    if(zlib) printf(" with compression");
    if(chunk) printf(" with chunked encoding");
    printf(" %s -> %s\n",infile,outfile);

    read_fd=open(infile,O_RDONLY|O_BINARY);
    if(read_fd==-1)
       PrintMessage(Fatal,"Cannot open '%s' for reading [%!s]",infile);

    write_fd=open(outfile,O_CREAT|O_TRUNC|O_WRONLY|O_BINARY,0644);
    if(write_fd==-1)
       PrintMessage(Fatal,"Cannot open '%s' for writing [%!s]",outfile);

    init_io(read_fd);

#if USE_ZLIB
    if(zlib)
       configure_io_zlib(read_fd,zlib?2:0,-1);
#endif
    if(chunk)
       configure_io_chunked(read_fd,chunk?1:0,-1);

    do
      {
       int size=1+rand()%128;

       n=read_data(read_fd,buffer,size);
       if(n>0)
          write(write_fd,buffer,n);
      }
    while(n>0);

    finish_io(read_fd);
    close(read_fd);

    close(write_fd);
   }

 /* Reading lines test */

 for(count=0;count<4;count++)
   {
    int zlib,chunk,lineno;
    int read_fd,write_fd;
    char *line=NULL;

    zlib =count&1;
    chunk=count&2;

    /* Create the files with headers */

    strcpy(infile,"write");
    if(zlib) strcat(infile,"-zlib");
    if(chunk) strcat(infile,"-chunk");
    strcat(infile,".txt");

    strcpy(outfile,"line");
    if(zlib) strcat(outfile,"-zlib");
    if(chunk) strcat(outfile,"-chunk");
    strcat(outfile,".txt");

    read_fd=open(infile,O_RDONLY|O_BINARY);
    if(read_fd==-1)
       PrintMessage(Fatal,"Cannot open '%s' for reading [%!s]",infile);

    write_fd=open(outfile,O_CREAT|O_TRUNC|O_WRONLY|O_BINARY,0644);
    if(write_fd==-1)
       PrintMessage(Fatal,"Cannot open '%s' for writing [%!s]",outfile);

    for(lineno=0;lineno<26;lineno++)
      {
       int size=1+rand()%256;

       buffer[size+3]=0;
       buffer[size+2]='\n';
       buffer[size+1]='\r';
       while(size>=0)
          buffer[size--]='A'+lineno;

       write(write_fd,buffer,strlen(buffer));
      }

    write(write_fd,"\r\n",2);

    while((n=read(read_fd,buffer,1024))>0)
       write(write_fd,buffer,n);

    close(read_fd);
    close(write_fd);

    /* Read in the headers */

    strcpy(infile,"line");
    if(zlib) strcat(infile,"-zlib");
    if(chunk) strcat(infile,"-chunk");
    strcat(infile,".txt");

    strcpy(outfile,"head");
    if(zlib) strcat(outfile,"-zlib");
    if(chunk) strcat(outfile,"-chunk");
    strcat(outfile,".txt");

    printf("Reading Lines (header)");
    printf(" %s -> %s\n",infile,outfile);

    read_fd=open(infile,O_RDONLY|O_BINARY);
    if(read_fd==-1)
       PrintMessage(Fatal,"Cannot open '%s' for reading [%!s]",infile);

    write_fd=open(outfile,O_CREAT|O_TRUNC|O_WRONLY|O_BINARY,0644);
    if(write_fd==-1)
       PrintMessage(Fatal,"Cannot open '%s' for writing [%!s]",outfile);

    init_io(read_fd);

    while((line=read_line(read_fd,line)))
      {
       write(write_fd,line,strlen(line));
       if(*line=='\r' || *line=='\n')
          break;
      }

    close(write_fd);

    /* Read in the body */

    strcpy(outfile,"body");
    if(zlib) strcat(outfile,"-zlib");
    if(chunk) strcat(outfile,"-chunk");
    strcat(outfile,".txt");

    printf("Reading Lines (body)");
    if(zlib) printf(" with compression");
    if(chunk) printf(" with chunked encoding");
    printf(" %s -> %s\n",infile,outfile);

    write_fd=open(outfile,O_CREAT|O_TRUNC|O_WRONLY|O_BINARY,0644);
    if(write_fd==-1)
       PrintMessage(Fatal,"Cannot open '%s' for writing [%!s]",outfile);

#if USE_ZLIB
    if(zlib)
       configure_io_zlib(read_fd,zlib?2:0,-1);
#endif
    if(chunk)
       configure_io_chunked(read_fd,chunk?1:0,-1);

    do
      {
       int size=1+rand()%1024;

       n=read_data(read_fd,buffer,size);
       if(n>0)
          write(write_fd,buffer,n);
      }
    while(n>0);

    finish_io(read_fd);
    close(read_fd);

    close(write_fd);
   }

 return(0);
}
