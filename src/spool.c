/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/spool.c 2.98 2006/11/14 17:10:19 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Handle all of the spooling of files in the spool directory.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#include <utime.h>
#include <sys/stat.h>

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <fcntl.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"


#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif

/*+ The length of the hash created for a cached name +*/
#define CACHE_HASHED_NAME_LEN 22


/* Local functions */

static int ChangeToCacheDir(URL *Url,int create,int errors);
static int ChangeToSpecialDir(const char *dirname,int create,int errors);


/* Local variables */

#if defined(__CYGWIN__)

/*+ The name of the spool directory. +*/
static char* sSpoolDir=NULL;

#else

/*+ The file descriptor of the spool directory. +*/
static int fSpoolDir=-1;

#endif


/*++++++++++++++++++++++++++++++++++++++
  Open a new file in the outgoing directory to write into.

  int OpenNewOutgoingSpoolFile Returns a file descriptor, or -1 on failure.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenNewOutgoingSpoolFile(void)
{
 int fd=-1;
 char name[8+MAX_INT_STR+1];

 sprintf(name,"tmp.%ld",(long)getpid());

 /* Create the outgoing directory if needed and change to it */

 if(ChangeToSpecialDir("outgoing",1,1))
    return(-1);

 /* Open the outgoing file */

 fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));
 /* init_io(fd) not called since fd is returned */

 if(fd==-1)
    PrintMessage(Warning,"Cannot open file 'outgoing/%s' to write [%!s]",name);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Close an outgoing spool file and rename it to the hashed name.

  int fd The file descriptor to close.

  URL *Url The URL to close.
  ++++++++++++++++++++++++++++++++++++++*/

void CloseNewOutgoingSpoolFile(int fd,URL *Url)
{
 char oldname[8+MAX_INT_STR+1],*newname;
 int ufd;

 /* finish_io(fd) not called since fd was returned */
 close(fd);

 /* Change to the outgoing directory. */

 if(ChangeToSpecialDir("outgoing",0,1))
    return;

 /* Create and rename the file */

 sprintf(oldname,"tmp.%ld",(long)getpid());

 newname=URLToFileName(Url,'U',0);

 ufd=open(newname,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

 if(ufd!=-1)
   {
    init_io(ufd);

    if(write_string(ufd,Url->file)==-1)
      {
       PrintMessage(Warning,"Cannot write to file 'outgoing/%s' [%!s]; disk full?",newname);
       unlink(newname);
      }

    finish_io(ufd);
    close(ufd);
   }

 newname=URLToFileName(Url,'O',0);

 if(rename(oldname,newname))
   {PrintMessage(Warning,"Cannot rename 'outgoing/%s' to 'outgoing/%s' [%!s].",oldname,newname);unlink(oldname);}

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Open the first existing file in the outgoing directory to read from.

  int OpenExistingOutgoingSpoolFile Returns a file descriptor, or -1 on failure.

  URL **Url Returns the URL of the file.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenExistingOutgoingSpoolFile(URL **Url)
{
 int fd=-1;
 char name[8+MAX_INT_STR+1];
 struct dirent* ent;
 DIR *dir;

 *Url=NULL;

 sprintf(name,"tmp.%ld",(long)getpid());

 /* Create the outgoing directory if needed and change to it */

 if(ChangeToSpecialDir("outgoing",1,1))
    return(-1);

 /* Open the outgoing file */

 dir=opendir(".");

 if(!dir)
   {PrintMessage(Warning,"Cannot open current directory 'outgoing' [%!s].");ChangeBackToSpoolDir();return(-1);}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read current directory 'outgoing' [%!s].");closedir(dir);ChangeBackToSpoolDir();return(-1);}

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(*ent->d_name=='O')
      {
       if(rename(ent->d_name,name))
          PrintMessage(Inform,"Cannot rename file 'outgoing/%s' to 'outgoing/%s' [%!s]; race condition?",ent->d_name,name);
       else
         {
          fd=open(name,O_RDONLY|O_BINARY);
          /* init_io(fd) not called since fd is returned */

          if(fd==-1)
             PrintMessage(Inform,"Cannot open file 'outgoing/%s' to read [%!s]; race condition?",name);
          else
            {
             *ent->d_name='U';
             *Url=FileNameToURL(ent->d_name);

             unlink(ent->d_name);
             unlink(name);

             break;
            }
         }
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if a specified URL exists in the outgoing directory.

  int ExistsOutgoingSpoolFile Returns a boolean.

  URL *Url The URL to check for.
  ++++++++++++++++++++++++++++++++++++++*/

int ExistsOutgoingSpoolFile(URL *Url)
{
 struct stat buf;
 int existsO,existsU;

 /* Change to the outgoing directory. */

 if(ChangeToSpecialDir("outgoing",0,1))
    return(0);

 /* Stat the outgoing file */

 existsO=!stat(URLToFileName(Url,'O',0),&buf);

 existsU=!stat(URLToFileName(Url,'U',0),&buf);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(existsO && existsU);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a hash value from the request for a specified URL in the outgoing directory.

  char *HashOutgoingSpoolFile Returns a hash string or NULL in error.

  URL *Url The URL to create the hash for.
  ++++++++++++++++++++++++++++++++++++++*/

char *HashOutgoingSpoolFile(URL *Url)
{
 char *name,*req,*hash;
 int fd,r;

 /* Change to the outgoing directory. */

 if(ChangeToSpecialDir("outgoing",0,1))
    return(NULL);

 /* Read the outgoing file */

 name=URLToFileName(Url,'O',0);

 fd=open(name,O_RDONLY|O_BINARY);

 if(fd==-1)
   {PrintMessage(Warning,"Cannot open outgoing request 'outgoing/%s' to create hash [%!s].",name);ChangeBackToSpoolDir();return(NULL);}

 init_io(fd);

 req=(char*)malloc((size_t)1025);

 r=read_data(fd,req,1024);
 if(r==1024)
   {
    int rr=0;
    do
      {
       r+=rr;
       req=(char*)realloc(req,r+1024+1);
      }
    while((rr=read_data(fd,&req[r],1024))>0);
   }

 finish_io(fd);
 close(fd);

 if(r==-1)
   {PrintMessage(Warning,"Cannot read from outgoing request 'outgoing/%s' to create hash [%!s].",name);free(req);ChangeBackToSpoolDir();return(NULL);}

 req[r]=0;

 hash=MakeHash(req);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 free(req);

 return(hash);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a specified URL request from the outgoing requests.

  char *DeleteOutgoingSpoolFile Returns NULL if OK else error message.

  URL *Url The URL to delete or NULL for all of them.
  ++++++++++++++++++++++++++++++++++++++*/

char *DeleteOutgoingSpoolFile(URL *Url)
{
 char *err=NULL;

 /* Change to the outgoing directory. */

 if(ChangeToSpecialDir("outgoing",0,1))
   {
    err=strcpy(malloc((size_t)40),"Cannot change to 'outgoing' directory");
    return(err);
   }

 /* Delete the file for the request or all of them. */

 if(Url)
   {
    char *name;

    name=URLToFileName(Url,'O',0);

    if(unlink(name))
       err=GetPrintMessage(Warning,"Cannot unlink outgoing request 'outgoing/%s' [%!s].",name);

    name=URLToFileName(Url,'U',0);

    unlink(name);
   }
 else
   {
    int any=0;
    struct dirent* ent;
    DIR *dir=opendir(".");

    if(!dir)
      {err=GetPrintMessage(Warning,"Cannot open current directory 'outgoing' [%!s].");ChangeBackToSpoolDir();return(err);}

    ent=readdir(dir);
    if(!ent)
      {err=GetPrintMessage(Warning,"Cannot read current directory 'outgoing' [%!s].");closedir(dir);ChangeBackToSpoolDir();return(err);}

    do
      {
       if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
          continue; /* skip . & .. */

       if(*ent->d_name=='U' || *ent->d_name=='O')
          if(unlink(ent->d_name))
            {
             any++;
             PrintMessage(Warning,"Cannot unlink outgoing request 'outgoing/%s' [%!s].",ent->d_name);
            }
      }
    while((ent=readdir(dir)));

    if(any)
       err=strcpy((char*)malloc((size_t)40),"Cannot delete some outgoing requests.");

    closedir(dir);
   }

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Open a file in a spool subdirectory to write into / read from.

  int OpenWebpageSpoolFile Returns a file descriptor.

  int rw Set to 1 to read, 0 to write.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenWebpageSpoolFile(int rw,URL *Url)
{
 char *file;
 int fd=-1;

 /* Create the spool directory if needed (write only) and change to it. */

 if(ChangeToCacheDir(Url,!rw,1))
    return(-1);

 /* Open the file for the web page. */

 file=URLToFileName(Url,'D',0);

 if(rw)
    fd=open(file,O_RDONLY|O_BINARY);
 else
    fd=open(file,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

 /* init_io(fd) not called since fd is returned */

 if(!rw && fd!=-1)
   {
    int ufd;

    file=URLToFileName(Url,'U',0);
    ufd=open(file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

    if(ufd!=-1)
      {
       init_io(ufd);

       if(write_string(ufd,Url->file)==-1)
         {
          PrintMessage(Warning,"Cannot write to file '%s/%s/%s' [%!s]; disk full?",Url->proto,URLToDirName(Url),file);
          unlink(file);
          close(fd);
          fd=-1;
         }

       finish_io(ufd);
       close(ufd);
      }
    else
      {
       close(fd);
       fd=-1;
      }
   }

 /* Change the modification time on the directory. */

 if(!rw && fd!=-1)
   {
    utime(".",NULL);

    ChangeBackToSpoolDir();
    chdir(Url->proto);

    utime(".",NULL);
   }

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a file in a spool subdirectory.

  char *DeleteWebpageSpoolFile Return NULL if OK else error message.

  URL *Url The URL to delete.

  int all If set then delete all pages from this host.
  ++++++++++++++++++++++++++++++++++++++*/

char *DeleteWebpageSpoolFile(URL *Url,int all)
{
 char *err=NULL;

 /* Change to the spool directory. */

 if(ChangeToCacheDir(Url,0,1))
   {
    err=strcpy(malloc((size_t)40),"Cannot change to URL's directory");
    return(err);
   }

 /* Delete the file for the web page. */

 if(all)
   {
    struct dirent* ent;
    DIR *dir=opendir(".");

    if(!dir)
      {err=GetPrintMessage(Warning,"Cannot open current directory '%s/%s' [%!s].",Url->proto,URLToDirName(Url));ChangeBackToSpoolDir();return(err);}

    ent=readdir(dir);
    if(!ent)
      {err=GetPrintMessage(Warning,"Cannot read current directory '%s/%s' [%!s].",Url->proto,URLToDirName(Url));closedir(dir);ChangeBackToSpoolDir();return(err);}

    do
      {
       if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
          continue; /* skip . & .. */

       if(*ent->d_name=='D')
         {
          URL *delUrl;

          if((delUrl=FileNameToURL(ent->d_name)))
            {
             char *err;

             ChangeBackToSpoolDir();

             err=DeleteLastTimeSpoolFile(delUrl);
             if(err) {free(err); err=NULL;}

             ChangeToCacheDir(Url,0,0);

             FreeURL(delUrl);
            }
         }

       if(unlink(ent->d_name))
          err=GetPrintMessage(Warning,"Cannot unlink cached file '%s/%s/%s' [%!s].",Url->proto,URLToDirName(Url),ent->d_name);
      }
    while((ent=readdir(dir)));

    closedir(dir);

    ChangeBackToSpoolDir();
    chdir(Url->proto);

    if(rmdir(URLToDirName(Url)))
      err=GetPrintMessage(Warning,"Cannot delete what should be an empty directory '%s/%s' [%!s].",Url->proto,URLToDirName(Url));

    ChangeBackToSpoolDir();
   }
 else
   {
    char *file,*err;
    struct stat buf;
    int didstat=1;

    if(stat(".",&buf))
       PrintMessage(Warning,"Cannot stat directory '%s/%s' [%!s].",Url->proto,URLToDirName(Url));
    else
       didstat=1;

    file=URLToFileName(Url,'D',0);
    if(unlink(file))
       err=GetPrintMessage(Warning,"Cannot unlink cached file '%s/%s/%s' [%!s].",Url->proto,URLToDirName(Url),file);

    file=URLToFileName(Url,'U',0);
    unlink(file);

    if(didstat)
      {
       struct utimbuf utbuf;

       utbuf.actime=time(NULL);
       utbuf.modtime=buf.st_mtime;
       utime(".",&utbuf);
      }

    ChangeBackToSpoolDir();

    err=DeleteLastTimeSpoolFile(Url);
    if(err) free(err);
   }

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Touch a file in a spool subdirectory.

  URL *Url The URL to touch.

  time_t when The time to set the access time to.
  ++++++++++++++++++++++++++++++++++++++*/

void TouchWebpageSpoolFile(URL *Url,time_t when)
{
 char *file;

 /* Change to the spool directory. */

 if(ChangeToCacheDir(Url,0,1))
    return;

 /* Touch the file for the web page. */

 file=URLToFileName(Url,'D',0);

 if(when)
   {
    struct stat buf;

    if(!stat(file,&buf))
      {
       struct utimbuf ubuf;

       ubuf.actime=when;
       ubuf.modtime=buf.st_mtime;

       utime(file,&ubuf);
      }
   }
 else
    utime(file,NULL);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Check if a file in a spool subdirectory exists.

  time_t ExistsWebpageSpoolFile Return a the time the page was last accessed if the page exists.

  URL *Url The URL to check for.
  ++++++++++++++++++++++++++++++++++++++*/

time_t ExistsWebpageSpoolFile(URL *Url)
{
 struct stat buf;
 int existsD,existsU;

 /* Change to the spool directory. */

 if(ChangeToCacheDir(Url,0,0))
    return(0);

 /* Stat the file for the web page. */

 existsU=!stat(URLToFileName(Url,'U',0),&buf);

 existsD=!stat(URLToFileName(Url,'D',0),&buf);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 if(existsU && existsD)
    return(buf.st_atime);
 else
    return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a backup copy of a file in a spool subdirectory.

  URL *Url The URL to make a copy of.
  ++++++++++++++++++++++++++++++++++++++*/

void CreateBackupWebpageSpoolFile(URL *Url)
{
 struct stat buf;
 char *bakfile,*orgfile;

 /* Change to the spool directory. */

 if(ChangeToCacheDir(Url,0,1))
    return;

 /* Create the filenames and rename the files. */

 orgfile=URLToFileName(Url,'D',0);

 bakfile=(char*)malloc(strlen(orgfile)+2);
 strcpy(bakfile,orgfile);
 strcat(bakfile,"~");

 if(!stat(bakfile,&buf))
    PrintMessage(Inform,"Backup already exists for '%s'.",Url->name);
 else
   {
    if(rename(orgfile,bakfile))
       PrintMessage(Warning,"Cannot rename backup cached file '%s/%s/%s' to %s [%!s].",Url->proto,URLToDirName(Url),orgfile,bakfile);

    *bakfile=*orgfile='U';
    rename(orgfile,bakfile);
   }

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 free(bakfile);
}


/*++++++++++++++++++++++++++++++++++++++
  Restore the backup copy of a file in a spool subdirectory.

  URL *Url The URL to restore.
  ++++++++++++++++++++++++++++++++++++++*/

void RestoreBackupWebpageSpoolFile(URL *Url)
{
 struct stat buf;
 char *bakfile,*orgfile;

 /* Change to the spool directory. */

 if(ChangeToCacheDir(Url,0,1))
    return;

 /* Create the filenames and rename the files. */

 orgfile=URLToFileName(Url,'D',0);

 bakfile=(char*)malloc(strlen(orgfile)+2);
 strcpy(bakfile,orgfile);
 strcat(bakfile,"~");

 if(!stat(bakfile,&buf))
   {
    if(rename(bakfile,orgfile))
       PrintMessage(Warning,"Cannot rename backup cached file '%s/%s/%s' to %s [%!s].",Url->proto,URLToDirName(Url),bakfile,orgfile);

    *bakfile=*orgfile='U';
    rename(bakfile,orgfile);
   }

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 free(bakfile);
}


/*++++++++++++++++++++++++++++++++++++++
  Open the backup copy of a file in a spool subdirectory to read from.

  int OpenBackupWebpageSpoolFile Returns a file descriptor.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenBackupWebpageSpoolFile(URL *Url)
{
 int fd=-1;

 /* Change to the spool directory. */

 if(ChangeToCacheDir(Url,0,0))
    return(-1);

 /* Open the file for the web page. */

 fd=open(URLToFileName(Url,'D','~'),O_RDONLY|O_BINARY);

 /* init_io(fd) not called since fd is returned */

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a file in a spool subdirectory.

  URL *Url The URL to delete.
  ++++++++++++++++++++++++++++++++++++++*/

void DeleteBackupWebpageSpoolFile(URL *Url)
{
 char *bakfile;

 /* Change to the spool directory. */

 if(ChangeToCacheDir(Url,0,1))
    return;

 /* Delete the file for the backup web page. */

 /* It might seem strange to touch a file just before deleting it, but there is
    a reason.  The backup file is linked to the files in the prevtime(x)
    directories.  Touching it here will update all of the linked files so that
    sorting the prevtime(x) index by date changed will distinguish files that
    have been fetched again since that prevtime(x) index. */

 bakfile=URLToFileName(Url,'D','~');

 utime(bakfile,NULL);

 if(unlink(bakfile))
    PrintMessage(Warning,"Cannot unlink backup cached file '%s/%s/%s' [%!s].",Url->proto,URLToDirName(Url),bakfile);

 bakfile=URLToFileName(Url,'U','~');
 unlink(bakfile);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Create a lock file in a spool subdirectory.

  int CreateLockWebpageSpoolFile Returns 1 if created OK or 0 in case of error.

  URL *Url The URL to lock.
  ++++++++++++++++++++++++++++++++++++++*/

int CreateLockWebpageSpoolFile(URL *Url)
{
 struct stat buf;
 char *lockfile;
 int retval=1;

 /* Check for configuration file option. */

 if(!ConfigBoolean(LockFiles))
    return(1);

 /* Create the spool directory if needed and change to it. */

 if(ChangeToCacheDir(Url,1,1))
    return(-1);

 /* Create the lock file for the web page. */

 lockfile=URLToFileName(Url,'L',0);

 if(!stat(lockfile,&buf))
   {
    PrintMessage(Inform,"Lock file already exists for '%s'.",Url->name);
    retval=0;
   }
 else
   {
    int fd;

    /* Using open() instead of link() allows a race condition over NFS.
       Using NFS for the WWWOFFLE spool is not recommended anyway. */

    fd=open(lockfile,O_WRONLY|O_CREAT|O_TRUNC|O_EXCL,(mode_t)ConfigInteger(FilePerm));

    if(fd==-1)
      {
       PrintMessage(Warning,"Cannot make a lock file for '%s/%s/%s' [%!s].",Url->proto,URLToDirName(Url),lockfile);
       retval=0;
      }
    else
       close(fd);
   }

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Check for the existance of a lock file in a spool subdirectory.

  int ExistsLockWebpageSpoolFile Return a true value if the lock file exists.

  URL *Url The URL to check for the lock file for.
  ++++++++++++++++++++++++++++++++++++++*/

int ExistsLockWebpageSpoolFile(URL *Url)
{
 struct stat buf;
 int existsL;

 /* Check for configuration file option. */

 if(!ConfigBoolean(LockFiles))
    return(0);

 /* Change to the spool directory. */

 if(ChangeToCacheDir(Url,0,0))
    return(0);

 /* Stat the file for the web page. */

 existsL=!stat(URLToFileName(Url,'L',0),&buf);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(existsL);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a lock file in a spool subdirectory.

  URL *Url The URL with the lock to delete.
  ++++++++++++++++++++++++++++++++++++++*/

void DeleteLockWebpageSpoolFile(URL *Url)
{
 char *lockfile;

 /* Check for configuration file option. */

 if(!ConfigBoolean(LockFiles))
    return;

 /* Change to the spool directory */

 if(ChangeToCacheDir(Url,0,1))
    return;

 /* Delete the file for the backup web page. */

 lockfile=URLToFileName(Url,'L',0);

 if(unlink(lockfile))
    PrintMessage(Inform,"Cannot unlink lock file '%s/%s/%s' [%!s].",Url->proto,URLToDirName(Url),lockfile);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Create a file in the lasttime directory.

  int CreateLastTimeSpoolFile Returns 1 if the file already exists.

  URL *Url The URL to create.
  ++++++++++++++++++++++++++++++++++++++*/

int CreateLastTimeSpoolFile(URL *Url)
{
 struct stat buf;
 char *file,*name;
 int exists=0;

 if(!ConfigBoolean(CreateHistoryIndexes))
    return(1);

 /* Change to the last time directory */

 if(ChangeToSpecialDir("lasttime",0,1))
    return(0);

 /* Create the file. */

 file=URLToFileName(Url,'D',0);

 if(!stat(file,&buf))
    exists=1;

 name=(char*)malloc(strlen(Url->proto)+strlen(URLToDirName(Url))+strlen(file)+8);
 sprintf(name,"../%s/%s/%s",Url->proto,URLToDirName(Url),file);

 unlink(file);
 if(link(name,file))
   {PrintMessage(Warning,"Cannot create file 'lasttime/%s' [%!s].",file);}
 else
   {
    file=URLToFileName(Url,'U',0);
    sprintf(name,"../%s/%s/%s",Url->proto,URLToDirName(Url),file);

    unlink(file);
    if(link(name,file))
      {PrintMessage(Warning,"Cannot create file 'lasttime/%s' [%!s].",file);}
   }

 free(name);

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(exists);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a specified URL from the lasttime directory.

  char *DeleteLastTimeSpoolFile Returns NULL if OK else error message.

  URL *Url The URL to delete or NULL for all of them.
  ++++++++++++++++++++++++++++++++++++++*/

char *DeleteLastTimeSpoolFile(URL *Url)
{
 struct stat buf;
 char *err=NULL;
 int i;
 char *name;

 for(i=0;i<=NUM_PREVTIME_DIR;i++)
   {
    char lasttime[8+MAX_INT_STR+1];

    if(i)
       sprintf(lasttime,"prevtime%d",i);
    else
       strcpy(lasttime,"lasttime");

    /* Change to the last time directory */

    if(ChangeToSpecialDir(lasttime,0,1))
       continue;

    name=URLToFileName(Url,'D',0);

    if(!stat(name,&buf))
      {
       if(unlink(name))
          err=GetPrintMessage(Warning,"Cannot unlink lasttime request '%s/%s' [%!s].",lasttime,name);

       name=URLToFileName(Url,'U',0);
       unlink(name);
      }

    ChangeBackToSpoolDir();
   }

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Cycle the URLs from the lasttime directory down to the prevtime directories.
  ++++++++++++++++++++++++++++++++++++++*/

void CycleLastTimeSpoolFile(void)
{
 char lasttime[8+MAX_INT_STR+1],prevlasttime[8+MAX_INT_STR+1];
 struct stat buf;
 int i,fd;

 /* Don't cycle if last done today. */

 if(ConfigBoolean(CycleIndexesDaily) && !stat("lasttime/.timestamp",&buf))
   {
    time_t timenow=time(NULL);
    struct tm *then,*now;
    long thenday,nowday;

    then=localtime(&buf.st_mtime);
    thenday=then->tm_year*400+then->tm_yday;

    now=localtime(&timenow);
    nowday=now->tm_year*400+now->tm_yday;

    if(thenday==nowday)
       return;
   }

 /* Cycle the lasttime/prevtime? directories */

 for(i=NUM_PREVTIME_DIR;i>=0;i--)
   {
    if(i)
       sprintf(lasttime,"prevtime%d",i);
    else
       strcpy(lasttime,"lasttime");

    /* Create it if it does not exist. */

    if(stat(lasttime,&buf))
      {
       PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.",lasttime);
       if(mkdir(lasttime,(mode_t)ConfigInteger(DirPerm)))
          PrintMessage(Warning,"Cannot create directory '%s' [%!s].",lasttime);
      }
    else
       if(!S_ISDIR(buf.st_mode))
          PrintMessage(Warning,"The file '%s' is not a directory.",lasttime);

    /* Delete the contents of the oldest one, rename the newer ones. */

    if(i==NUM_PREVTIME_DIR)
      {
       DIR *dir;
       struct dirent* ent;

       if(ChangeToSpecialDir(lasttime,0,1))
          continue;

       dir=opendir(".");

       if(!dir)
         {PrintMessage(Warning,"Cannot open current directory '%s' [%!s].",lasttime);ChangeBackToSpoolDir();continue;}

       ent=readdir(dir);
       if(!ent)
         {PrintMessage(Warning,"Cannot read current directory '%s' [%!s].",lasttime);closedir(dir);ChangeBackToSpoolDir();continue;}

       do
         {
          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

          if(unlink(ent->d_name))
             PrintMessage(Warning,"Cannot unlink previous time page '%s/%s' [%!s].",lasttime,ent->d_name);
         }
       while((ent=readdir(dir)));

       closedir(dir);

       ChangeBackToSpoolDir();

       if(rmdir(lasttime))
          PrintMessage(Warning,"Cannot unlink previous time directory '%s' [%!s].",lasttime);
      }
    else
       if(rename(lasttime,prevlasttime))
          PrintMessage(Warning,"Cannot rename previous time directory '%s' to '%s' [%!s].",lasttime,prevlasttime);

    strcpy(prevlasttime,lasttime);
   }

 /* Create the lasttime directory and the timestamp. */

 if(mkdir("lasttime",(mode_t)ConfigInteger(DirPerm)))
    PrintMessage(Warning,"Cannot create directory 'lasttime' [%!s].");

 fd=open("lasttime/.timestamp",O_WRONLY|O_CREAT|O_TRUNC,(mode_t)ConfigInteger(FilePerm));
 if(fd!=-1)
    close(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Cycle the URLs from the outgoing directory to the lastout to prevout directories.
  ++++++++++++++++++++++++++++++++++++++*/

void CycleLastOutSpoolFile(void)
{
 char lastout[8+MAX_INT_STR+1],prevlastout[8+MAX_INT_STR+1];
 struct stat buf;
 int i,fd;

 /* Don't cycle if last done today. */

 if(ConfigBoolean(CycleIndexesDaily) && !stat("lastout/.timestamp",&buf))
   {
    time_t timenow=time(NULL);
    struct tm *then,*now;

    long thenday,nowday;

    then=localtime(&buf.st_mtime);
    thenday=then->tm_year*400+then->tm_yday;

    now=localtime(&timenow);
    nowday=now->tm_year*400+now->tm_yday;

    if(thenday==nowday)
       goto link_outgoing;
   }

 /* Cycle the lastout/prevout? directories */

 for(i=NUM_PREVTIME_DIR;i>=0;i--)
   {
    if(i)
       sprintf(lastout,"prevout%d",i);
    else
       strcpy(lastout,"lastout");

    /* Create it if it does not exist. */

    if(stat(lastout,&buf))
      {
       PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.",lastout);
       if(mkdir(lastout,(mode_t)ConfigInteger(DirPerm)))
          PrintMessage(Warning,"Cannot create directory '%s' [%!s].",lastout);
      }
    else
       if(!S_ISDIR(buf.st_mode))
          PrintMessage(Warning,"The file '%s' is not a directory.",lastout);

    /* Delete the contents of the oldest one, rename the newer ones. */

    if(i==NUM_PREVTIME_DIR)
      {
       DIR *dir;
       struct dirent* ent;

       if(ChangeToSpecialDir(lastout,0,1))
          continue;

       dir=opendir(".");

       if(!dir)
         {PrintMessage(Warning,"Cannot open current directory '%s' [%!s].",lastout);ChangeBackToSpoolDir();continue;}

       ent=readdir(dir);
       if(!ent)
         {PrintMessage(Warning,"Cannot read current directory '%s' [%!s].",lastout);closedir(dir);ChangeBackToSpoolDir();continue;}

       do
         {
          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

          if(unlink(ent->d_name))
             PrintMessage(Warning,"Cannot unlink previous time page '%s/%s' [%!s].",lastout,ent->d_name);
         }
       while((ent=readdir(dir)));

       closedir(dir);

       ChangeBackToSpoolDir();

       if(rmdir(lastout))
          PrintMessage(Warning,"Cannot unlink previous time directory '%s' [%!s].",lastout);
      }
    else
       if(rename(lastout,prevlastout))
          PrintMessage(Warning,"Cannot rename previous time directory '%s' to '%s' [%!s].",lastout,prevlastout);

    strcpy(prevlastout,lastout);
   }

 /* Create the lastout directory and the timestamp. */

 if(mkdir("lastout",(mode_t)ConfigInteger(DirPerm)))
    PrintMessage(Warning,"Cannot create directory 'lastout' [%!s].");

 fd=open("lastout/.timestamp",O_WRONLY|O_CREAT|O_TRUNC,(mode_t)ConfigInteger(FilePerm));
 if(fd!=-1)
    close(fd);

 /* Link the files from the outgoing directory to the lastout directory. */

link_outgoing:

 if(!ConfigBoolean(CreateHistoryIndexes))
    return;

 if(!ChangeToSpecialDir("outgoing",0,1))
   {
    DIR *dir;
    struct dirent* ent;

    dir=opendir(".");

    if(!dir)
       PrintMessage(Warning,"Cannot open current directory '%s' [%!s].",lastout);
    else
      {
       ent=readdir(dir);
       if(!ent)
          PrintMessage(Warning,"Cannot read current directory '%s' [%!s].",lastout);
       else
         {
          do
            {
             if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
                continue; /* skip . & .. */

             if(*ent->d_name=='U' || *ent->d_name=='O')
               {
                char newname[11+1+CACHE_HASHED_NAME_LEN+1+1]; /* 22 for hash, 1 for prefix, 1 for postfix, 1 terminator */

                strcpy(newname,"../lastout/");
                strcat(newname,ent->d_name);

                unlink(newname);
                if(link(ent->d_name,newname))
                   PrintMessage(Warning,"Cannot create lastout page '%s' [%!s].",&newname[3]);
               }
            }
          while((ent=readdir(dir)));
         }

       closedir(dir);
      }

    ChangeBackToSpoolDir();
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Open a file in the monitor directory to write into.

  int CreateMonitorSpoolFile Returns a file descriptor, or -1 on failure.

  URL *Url The URL of the file to monitor.

  char MofY[13] A mask indicating the months of the year allowed.

  char DofM[32] A mask indicating the days of the month allowed.

  char DofW[8] A mask indicating the days of the week allowed.

  char HofD[25] A mask indicating the hours of the day allowed.
  ++++++++++++++++++++++++++++++++++++++*/

int CreateMonitorSpoolFile(URL *Url,char MofY[13],char DofM[32],char DofW[8],char HofD[25])
{
 int fd=-1;
 char *file;

 /* Create the monitor directory if needed and change to it */

 if(ChangeToSpecialDir("monitor",1,1))
    return(-1);

 /* Open the monitor file */

 file=URLToFileName(Url,'O',0);

 fd=open(file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

 if(fd==-1)
   {PrintMessage(Warning,"Cannot create file 'monitor/%s' [%!s].",file);}
 else
   {
    int ufd,mfd;

    /* init_io(fd) not called since fd is returned */

    file=URLToFileName(Url,'U',0);
    ufd=open(file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

    if(ufd!=-1)
      {
       init_io(ufd);

       if(write_string(ufd,Url->file)==-1)
         {
          PrintMessage(Warning,"Cannot write to file 'monitor/%s' [%!s]; disk full?",file);
          unlink(file);
          close(fd);
          fd=-1;
         }

       finish_io(ufd);
       close(ufd);
      }
    else
      {
       close(fd);
       fd=-1;
      }

    file=URLToFileName(Url,'M',0);
    mfd=open(file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

    if(mfd!=-1)
      {
       init_io(mfd);

       if(write_formatted(mfd,"%s\n",MofY)==-1 ||
          write_formatted(mfd,"%s\n",DofM)==-1 ||
          write_formatted(mfd,"%s\n",DofW)==-1 ||
          write_formatted(mfd,"%s\n",HofD)==-1)
         {
          PrintMessage(Warning,"Cannot write to file 'monitor/%s' [%!s]; disk full?",file);
          unlink(file);
          close(fd);
          fd=-1;
         }

       finish_io(mfd);
       close(mfd);
      }
    else
      {
       close(fd);
       fd=-1;
      }
   }

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a file containing the time to be monitored.

  long ReadMonitorTimesSpoolFile Returns the timestamp of the file.

  URL *Url The URL to read from.

  char MofY[13] Returns a mask indicating the months of the year allowed.

  char DofM[32] Returns a mask indicating the days of the month allowed.

  char DofW[8] Returns a mask indicating the days of the week allowed.

  char HofD[25] Returns a mask indicating the hours of the day allowed.
  ++++++++++++++++++++++++++++++++++++++*/

long ReadMonitorTimesSpoolFile(URL *Url,char MofY[13],char DofM[32],char DofW[8],char HofD[25])
{
 time_t mtime;
 struct stat buf;
 int mfd;

 strcpy(MofY,"111111111111");
 strcpy(DofM,"1111111111111111111111111111111");
 strcpy(DofW,"1111111");
 strcpy(HofD,"100000000000000000000000");

 /* Change to the monitor directory. */

 if(ChangeToSpecialDir("monitor",0,1))
    return(0);

 /* Check for 'M*' file, missing or in the old format. */

 if(stat(URLToFileName(Url,'M',0),&buf))
   {
    /* file missing. */

    if(!stat(URLToFileName(Url,'U',0),&buf))
       PrintMessage(Warning,"Monitor time file is missing for %s.",Url->name);

    /* force fetching again. */

    mtime=0;
   }
 else if(buf.st_size<8)
   {
    /* file in the old format. */

    PrintMessage(Warning,"Monitor time file is obsolete for %s.",Url->name);

    /* force fetching again. */

    mtime=0;
   }
 else
   {
    mtime=buf.st_mtime;

    /* Assume that the file is in the new format. */

    mfd=open(URLToFileName(Url,'M',0),O_RDONLY|O_BINARY);

    if(mfd!=-1)
      {
       char line[80];
       line[78]=0;

       init_io(mfd);

       if(read_data(mfd,line,79)!=78 ||
          sscanf(line,"%12s %31s %7s %24s",MofY,DofM,DofW,HofD)!=4 ||
          strlen(MofY)!=12 || strlen(DofM)!=31 || strlen(DofW)!=7 || strlen(HofD)!=24)
         {
          PrintMessage(Warning,"Monitor time file is invalid for %s.",Url->name);
         }

       finish_io(mfd);
       close(mfd);
      }
   }

 ChangeBackToSpoolDir();

 return(mtime);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a specified URL from the monitor directory.

  char *DeleteMonitorSpoolFile Returns NULL if OK else error message.

  URL *Url The URL to delete or NULL for all of them.
  ++++++++++++++++++++++++++++++++++++++*/

char *DeleteMonitorSpoolFile(URL *Url)
{
 char *err=NULL;

 /* Change to the monitor directory. */

 if(ChangeToSpecialDir("monitor",0,1))
   {
    err=strcpy(malloc((size_t)40),"Cannot change to 'monitor' directory");
    return(err);
   }

 /* Delete the file for the request. */

 if(Url)
   {
    if(unlink(URLToFileName(Url,'O',0)))
       err=GetPrintMessage(Warning,"Cannot unlink monitor request 'monitor/%s' [%!s].",URLToFileName(Url,'O',0));

    unlink(URLToFileName(Url,'U',0));

    unlink(URLToFileName(Url,'M',0));
   }
 else
   {
    struct dirent* ent;
    DIR *dir=opendir(".");

    if(!dir)
      {err=GetPrintMessage(Warning,"Cannot open current directory 'monitor' [%!s].");ChangeBackToSpoolDir();return(err);}

    ent=readdir(dir);
    if(!ent)
      {err=GetPrintMessage(Warning,"Cannot read current directory 'monitor' [%!s].");closedir(dir);ChangeBackToSpoolDir();return(err);}

    do
      {
       if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
          continue; /* skip . & .. */

       if(*ent->d_name=='U' || *ent->d_name=='O' || *ent->d_name=='M')
          if(unlink(ent->d_name))
             err=GetPrintMessage(Warning,"Cannot unlink monitor request 'monitor/%s' [%!s].",ent->d_name);
      }
    while((ent=readdir(dir)));

    closedir(dir);
   }

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a filename to a URL.

  URL *FileNameToURL Returns the URL.

  const char *file The file name.
  ++++++++++++++++++++++++++++++++++++++*/

URL *FileNameToURL(const char *file)
{
 URL *Url;
 struct stat buf;
 char *url,*copy;
 int ufd;
 ssize_t nr;

 if(!file || !*file)
    return(NULL);

 copy=(char*)malloc(strlen(file)+1);
 strcpy(copy,file);

 *copy='U';

 ufd=open(copy,O_RDONLY|O_BINARY);

 free(copy);

 if(ufd==-1)
    return(NULL);

 if(fstat(ufd,&buf))
    return(NULL);

 url=(char*)malloc((size_t)buf.st_size+1);

 init_io(ufd);

 nr=read_data(ufd,url,buf.st_size);

 finish_io(ufd);
 close(ufd);

 if(nr!=buf.st_size)
   {
    free(url);
    return(NULL);
   }

 url[nr]=0;

 Url=SplitURL(url);

 free(url);

 return(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a URL to a filename.

  char *URLToFileName Returns a string with the filename using the specified prefix and postfix characters.

  URL *Url The URL to convert to a filename.

  char prefix The prefix character to use.

  char postfix The postfix character to use.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLToFileName(URL *Url,char prefix,char postfix)
{
 if(!Url->private_file)
   {
    char *hash;

    hash=MakeHash(Url->file);

    Url->private_file=(char*)malloc(1+CACHE_HASHED_NAME_LEN+1+1); /* 22 for hash, 1 for prefix, 1 for postfix, 1 terminator */

    strcpy(Url->private_file+1,hash);

    free(hash);
   }

 Url->private_file[0]=prefix;
 Url->private_file[CACHE_HASHED_NAME_LEN+1]=postfix;
 Url->private_file[CACHE_HASHED_NAME_LEN+2]=0;

 return(Url->private_file);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a URL to a directory name.

  char *URLToDirName Returns a string with the directory name (modified for cygwin if needed).

  URL *Url The URL to convert to a directory name.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLToDirName(URL *Url)
{
 if(!Url->private_dir)
   {
#if defined(__CYGWIN__)

    if(strchr(Url->hostport,':'))
      {
       int i;
       Url->private_dir=(char*)malloc(strlen(Url->hostport)+1);

       for(i=0;Url->hostport[i];i++)
          if(Url->hostport[i]==':')
             Url->private_dir[i]='!';
          else
             Url->private_dir[i]=Url->hostport[i];

       Url->private_dir[i]=0;
      }
    else
       Url->private_dir=Url->hostport;

#else

    Url->private_dir=Url->hostport;

#endif
   }

 return(Url->private_dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Change directory to a protocol / hostname directory.

  int ChangeToCacheDir Returns 0 if OK or an error.

  URL *Url The URL of the directory to change to.

  int create A flag that should be set if the directory is to be created.

  int errors A flag to indicate that an error message should be printed.
  ++++++++++++++++++++++++++++++++++++++*/

static int ChangeToCacheDir(URL *Url,int create,int errors)
{
 /* Create the spool proto directory if requested and change to it. */

 if(create)
   {
    struct stat buf;

    if(stat(Url->proto,&buf))
      {
       PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.",Url->proto);
       if(mkdir(Url->proto,(mode_t)ConfigInteger(DirPerm)))
         {
          PrintMessage(Warning,"Cannot create directory '%s' [%!s].",Url->proto);
          return(1);
         }
      }
    else if(!S_ISDIR(buf.st_mode))
      {
       PrintMessage(Warning,"The file '%s' is not a directory.",Url->proto);
       return(1);
      }
   }

 if(chdir(Url->proto))
   {
    if(errors)
      PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);
    return(2);
   }

 /* Create the spool host directory if requested and change to it. */

 if(create)
   {
    struct stat buf;

    if(stat(URLToDirName(Url),&buf))
      {
       PrintMessage(Inform,"Directory '%s/%s' does not exist [%!s]; creating one.",Url->proto,URLToDirName(Url));
       if(mkdir(URLToDirName(Url),(mode_t)ConfigInteger(DirPerm)))
         {
          PrintMessage(Warning,"Cannot create directory '%s/%s' [%!s].",Url->proto,URLToDirName(Url));
          ChangeBackToSpoolDir();
          return(3);
         }
      }
    else if(!S_ISDIR(buf.st_mode))
      {
       PrintMessage(Warning,"The file '%s/%s' is not a directory.",Url->proto,URLToDirName(Url));
       ChangeBackToSpoolDir();
       return(3);
      }
   }

 if(chdir(URLToDirName(Url)))
   {
    if(errors)
      PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,URLToDirName(Url));
    ChangeBackToSpoolDir();
    return(4);
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Change directory to a special (outgoing, lasttime etc.) directory.

  int ChangeToSpecialDir Returns 0 if OK or an error.

  const char *dirname The URL of the directory to change to.

  int create A flag that should be set if the directory is to be created.

  int errors A flag to indicate that an error message should be printed.
  ++++++++++++++++++++++++++++++++++++++*/

static int ChangeToSpecialDir(const char *dirname,int create,int errors)
{
 struct stat buf;

 /* Create the spool host directory if requested and change to it. */

 if(create)
   {
    if(stat(dirname,&buf))
      {
       PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.",dirname);
       if(mkdir(dirname,(mode_t)ConfigInteger(DirPerm)))
         {
          PrintMessage(Warning,"Cannot create directory '%s' [%!s].",dirname);
          return(1);
         }
      }
    else if(!S_ISDIR(buf.st_mode))
      {
       PrintMessage(Warning,"The file '%s' is not a directory.",dirname);
       return(1);
      }
   }

 if(chdir(dirname))
   {
    if(errors)
       PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",dirname);
    return(2);
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Open the spool directory.

  int ChangeToSpoolDir Changes to the spool directory (and opens a file descriptor there for later).

  const char *dir The directory to change to (and open).
  ++++++++++++++++++++++++++++++++++++++*/

int ChangeToSpoolDir(const char *dir)
{
 int err=chdir(dir);

 if(err==-1)
    return(-1);

#if defined(__CYGWIN__)

 sSpoolDir=(char*)malloc(strlen(dir)+1);
 strcpy(sSpoolDir,dir);

#else

 fSpoolDir=open(dir,O_RDONLY);
 if(fSpoolDir==-1)
    return(-1);

#endif

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Change back to the spool directory.

  int ChangeBackToSpoolDir Return -1 in case of error.
  ++++++++++++++++++++++++++++++++++++++*/

int ChangeBackToSpoolDir(void)
{
 int err;

#if defined(__CYGWIN__)

 err=chdir(sSpoolDir);

#else

 err=fchdir(fSpoolDir);

#endif

 return(err);
}
