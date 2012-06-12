/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/wwwoffle-tools.c,v 1.61 2006-04-21 18:46:41 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Tools for use in the cache for version 2.x.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
#include <grp.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "version.h"
#include "config.h"


#ifndef PATH_MAX
/*+ The maximum pathname length in characters. +*/
#define PATH_MAX 4096
#endif

#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif

#define LS         1            /*+ For the 'ls' operation. +*/
#define LS_DIR     2            /*+ For the 'ls' operation on directories. +*/
#define LS_SPECIAL 3            /*+ For the 'ls' operation on special directories. +*/
#define MV         4            /*+ For the 'mv' operation. +*/
#define RM         5            /*+ For the 'rm' operation. +*/
#define READ       6            /*+ For the 'read' operation. +*/
#define WRITE      7            /*+ For the 'write' operation. +*/
#define HASH       8            /*+ For the 'hash' operation. +*/
#define GZIP       9            /*+ For the 'gzip' operation. +*/
#define GUNZIP    10            /*+ For the 'gunzip' operation. +*/
#define FSCK      11            /*+ For the 'fsck' operation. +*/

/*+ A compile time option to not actually make any changes to files for debugging. +*/
#define MAKE_CHANGES 1


/* Local functions */

static int wwwoffle_ls_url(URL *Url);
static int wwwoffle_ls_dir(char *name);
static int wwwoffle_ls_special(char *name);
static int wwwoffle_mv(URL *Url1,URL *Url2);
static int wwwoffle_rm(URL *Url);
static int wwwoffle_read(URL *Url);
static int wwwoffle_write(URL *Url);
static int wwwoffle_hash(URL *Url);
static int wwwoffle_gzip(URL *Url,int compress);
static int wwwoffle_fsck(void);

static int ls(char *file);

#if USE_ZLIB
static void gzip_file(char *proto,char *hostport,char *file,int compress);
#endif

static void wwwoffle_fsck_check_proto(char *proto);
static void wwwoffle_fsck_check_dir(char *proto,char *host,char *special);
static char *FileNameTo_url(char *file);


/*++++++++++++++++++++++++++++++++++++++
  The main program
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char **argv)
{
 struct stat buf;
 URL **Url=NULL;
 int mode=0,exitval=0;
 int i;
 char *argv0,*config_file=NULL;

 /* Check which program we are running */

 argv0=argv[0]+strlen(argv[0])-1;
 while(argv0>=argv[0] && *argv0!='/')
    argv0--;

 argv0++;

 if(!strcmp(argv0,"wwwoffle-ls"))
    mode=LS;
 else if(!strcmp(argv0,"wwwoffle-mv"))
    mode=MV;
 else if(!strcmp(argv0,"wwwoffle-rm"))
    mode=RM;
 else if(!strcmp(argv0,"wwwoffle-read"))
    mode=READ;
 else if(!strcmp(argv0,"wwwoffle-write"))
    mode=WRITE;
 else if(!strcmp(argv0,"wwwoffle-hash"))
    mode=HASH;
 else if(!strcmp(argv0,"wwwoffle-gzip"))
    mode=GZIP;
 else if(!strcmp(argv0,"wwwoffle-gunzip"))
    mode=GUNZIP;
 else if(!strcmp(argv0,"wwwoffle-fsck"))
    mode=FSCK;
 else if(!strcmp(argv0,"wwwoffle-tools"))
   {
    if(argc>1 && !strcmp(argv[1],"-ls"))
      {mode=LS; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-mv"))
      {mode=MV; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-rm"))
      {mode=RM; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-read"))
      {mode=READ; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-write"))
      {mode=WRITE; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-hash"))
      {mode=HASH; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-gzip"))
      {mode=GZIP; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-gunzip"))
      {mode=GUNZIP; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-fsck"))
      {mode=FSCK; argv++; argc--;}
   }

 if(mode==0)
   {
    fprintf(stderr,"wwwoffle-tools version %s\n"
                   "To select the mode of operation choose:\n"
                   "        wwwoffle-ls     ( = wwwoffle-tools -ls )\n"
                   "        wwwoffle-mv     ( = wwwoffle-tools -mv )\n"
                   "        wwwoffle-rm     ( = wwwoffle-tools -rm )\n"
                   "        wwwoffle-read   ( = wwwoffle-tools -read )\n"
                   "        wwwoffle-write  ( = wwwoffle-tools -write )\n"
                   "        wwwoffle-hash   ( = wwwoffle-tools -hash )\n"
                   "        wwwoffle-gzip   ( = wwwoffle-tools -gzip )\n"
                   "        wwwoffle-gunzip ( = wwwoffle-tools -gunzip )\n"
                   "        wwwoffle-fsck   ( = wwwoffle-tools -fsck )\n",
                   WWWOFFLE_VERSION);
    exit(1);
   }

 if(mode==LS)
    for(i=1;i<argc;i++)
       if(!strcmp(argv[i],"outgoing") || !strcmp(argv[i],"monitor") ||
          !strcmp(argv[i],"lasttime") || (!strncmp(argv[i],"prevtime",(size_t)8) && isdigit(argv[i][8])) || 
          !strcmp(argv[i],"lastout")  || (!strncmp(argv[i],"prevout",(size_t)7)  && isdigit(argv[i][7])))
          mode=LS_SPECIAL;
       else
         {
          int c=0;
          char *p=argv[i];

          while(*p)
             if(*p++=='/')
                c++;

          if(c==1)
             mode=LS_DIR;
         }

 /* Find the configuration file */

 for(i=1;i<argc;i++)
    if(config_file)
      {
       argv[i-2]=argv[i];
      }
    else if(!strcmp(argv[i],"-c"))
      {
       if(++i>=argc)
         {fprintf(stderr,"%s: The '-c' argument requires a configuration file name.\n",argv0); exit(1);}

       config_file=argv[i];
      }

 if(config_file)
    argc-=2;

 if(!config_file)
   {
    char *env=getenv("WWWOFFLE_PROXY");

    if(env && *env=='/')
       config_file=env;
   }

 /* Print the usage instructions */

 if(argc>1 && !strcmp(argv[1],"--version"))
   {fprintf(stderr,"%s version %s\n",argv0,WWWOFFLE_VERSION);exit(0);}

 else if((mode==LS && (argc<2 || (argc>1 && !strcmp(argv[1],"--help")))) ||
         (mode==LS_DIR && (argc!=2 || (argc>1 && !strcmp(argv[1],"--help")))) ||
         (mode==LS_SPECIAL && (argc!=2 || (argc>1 && !strcmp(argv[1],"--help")))))
   {fprintf(stderr,"Usage: wwwoffle-ls [-c <config-file>]\n"
                   "                   <URL> ...\n"
                   "       wwwoffle-ls [-c <config-file>]\n"
                   "                   <dir>/<subdir>\n"
                   "       wwwoffle-ls [-c <config-file>]\n"
                   "                   ( outgoing | lastout | prevout[0-9] |\n"
                   "                     monitor | lasttime | prevtime[0-9] )\n");exit(0);}
 else if(mode==MV && (argc!=3 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-mv [-c <config-file>]\n"
                   "                   (<dir1>/<subdir1> | <protocol1>://<host1>)\n"
                   "                   (<dir2>/<subdir2> | <protocol2>://<host2>)\n");exit(0);}
 else if(mode==RM && (argc<2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-rm [-c <config-file>] <URL> ...\n");exit(0);}
 else if(mode==READ && (argc!=2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-read [-c <config-file>] <URL>\n");exit(0);}
 else if(mode==WRITE && (argc!=2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-write [-c <config-file>] <URL>\n");exit(0);}
 else if(mode==HASH && (argc!=2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-hash [-c <config-file>] <URL>\n");exit(0);}
 else if(mode==GZIP && (argc<2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-gzip [-c <config-file>] \n"
                   "                     ( <dir>/<subdir> | <protocol>://<host> ) ...\n");exit(0);}
 else if(mode==GUNZIP && (argc<2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-gunzip [-c <config-file>] \n"
                   "                       ( <dir>/<subdir> | <protocol>://<host> ) ...\n");exit(0);}
 else if(mode==FSCK && (argc!=1 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-fsck [-c <config-file>]\n");exit(0);}

 /* Initialise */

 InitErrorHandler(argv0,0,1);

 InitConfigurationFile(config_file);

 if(config_file)
   {
    init_io(STDERR_FILENO);

    if(ReadConfigurationFile(STDERR_FILENO))
       PrintMessage(Fatal,"Error in configuration file '%s'.",config_file);

    finish_io(STDERR_FILENO);
   }

 umask(0);

 if(mode==HASH)
    ;
 else if((stat("outgoing",&buf) || !S_ISDIR(buf.st_mode)) ||
         (stat("http",&buf) || !S_ISDIR(buf.st_mode)))
   {
    if(ChangeToSpoolDir(ConfigString(SpoolDir)))
      {fprintf(stderr,"The %s program must be started from the spool directory\n"
                      "Cannot change to the '%s' directory.\n",argv0,ConfigString(SpoolDir));exit(1);}
    if((stat("outgoing",&buf) || !S_ISDIR(buf.st_mode)) ||
       (stat("http",&buf) || !S_ISDIR(buf.st_mode)))
      {fprintf(stderr,"The %s program must be started from the spool directory\n"
                      "There is no accessible 'outgoing' directory here so it can't be right.\n",argv0);exit(1);}
   }
 else
   {
    char cwd[PATH_MAX+1];

    getcwd(cwd,(size_t)PATH_MAX);

    ChangeToSpoolDir(cwd);
   }

 /* Get the arguments */

 if(mode!=LS_DIR && mode!=LS_SPECIAL)
   {
    Url=(URL**)malloc(argc*sizeof(URL*));

    for(i=1;i<argc;i++)
      {
       char *arg,*colon,*slash;

       if(!strncmp(ConfigString(SpoolDir),argv[i],strlen(ConfigString(SpoolDir))) &&
          argv[i][strlen(ConfigString(SpoolDir))]=='/')
          arg=argv[i]+strlen(ConfigString(SpoolDir))+1;
       else
          arg=argv[i];

       colon=strchr(arg,':');
       slash=strchr(arg,'/');

       if((colon && slash && colon<slash) ||
          !slash)
         {
          Url[i]=SplitURL(arg);
         }
       else
         {
          *slash=0;
          Url[i]=CreateURL(arg,slash+1,"/",NULL,NULL,NULL);
         }
      }
   }

 if(mode==LS)
    for(i=1;i<argc;i++)
       exitval+=wwwoffle_ls_url(Url[i]);
 else if(mode==LS_DIR)
    exitval=wwwoffle_ls_dir(argv[1]);
 else if(mode==LS_SPECIAL)
    exitval=wwwoffle_ls_special(argv[1]);
 else if(mode==MV)
    exitval=wwwoffle_mv(Url[1],Url[2]);
 else if(mode==RM)
    for(i=1;i<argc;i++)
       exitval+=wwwoffle_rm(Url[i]);
 else if(mode==READ)
    exitval=wwwoffle_read(Url[1]);
 else if(mode==WRITE)
   {
    if(config_file)
      {
       /* Change the user and group. */

       int gid=ConfigInteger(WWWOFFLE_Gid);
       int uid=ConfigInteger(WWWOFFLE_Uid);

       if(uid!=-1)
          seteuid(0);

       if(gid!=-1)
         {
#if HAVE_SETGROUPS
          if(getuid()==0 || geteuid()==0)
             if(setgroups(0,NULL)<0)
                PrintMessage(Fatal,"Cannot clear supplementary group list [%!s].");
#endif

#if HAVE_SETRESGID
          if(setresgid((gid_t)gid,(gid_t)gid,(gid_t)gid)<0)
             PrintMessage(Fatal,"Cannot set real/effective/saved group id to %d [%!s].",gid);
#else
          if(geteuid()==0)
            {
             if(setgid((gid_t)gid)<0)
                PrintMessage(Fatal,"Cannot set group id to %d [%!s].",gid);
            }
          else
            {
#if HAVE_SETREGID
             if(setregid(getegid(),(gid_t)gid)<0)
                PrintMessage(Fatal,"Cannot set effective group id to %d [%!s].",gid);
             if(setregid((gid_t)gid,(gid_t)~1)<0)
                PrintMessage(Fatal,"Cannot set real group id to %d [%!s].",gid);
#else
             PrintMessage(Fatal,"Must be root to totally change group id.");
#endif
            }
#endif
         }

       if(uid!=-1)
         {
#if HAVE_SETRESUID
          if(setresuid((uid_t)uid,(uid_t)uid,(uid_t)uid)<0)
             PrintMessage(Fatal,"Cannot set real/effective/saved user id to %d [%!s].",uid);
#else
          if(geteuid()==0)
            {
             if(setuid((uid_t)uid)<0)
                PrintMessage(Fatal,"Cannot set user id to %d [%!s].",uid);
            }
          else
            {
#if HAVE_SETREUID
             if(setreuid(geteuid(),(uid_t)uid)<0)
                PrintMessage(Fatal,"Cannot set effective user id to %d [%!s].",uid);
             if(setreuid((uid_t)uid,(uid_t)~1)<0)
                PrintMessage(Fatal,"Cannot set real user id to %d [%!s].",uid);
#else
             PrintMessage(Fatal,"Must be root to totally change user id.");
#endif
            }
#endif
         }

       if(uid!=-1 || gid!=-1)
          PrintMessage(Inform,"Running with uid=%d, gid=%d.",geteuid(),getegid());
      }

    exitval=wwwoffle_write(Url[1]);
   }
 else if(mode==HASH)
    exitval=wwwoffle_hash(Url[1]);
 else if(mode==GZIP)
    for(i=1;i<argc;i++)
       exitval+=wwwoffle_gzip(Url[i],1);
 else if(mode==GUNZIP)
    for(i=1;i<argc;i++)
       exitval+=wwwoffle_gzip(Url[i],0);
 else if(mode==FSCK)
    exitval=wwwoffle_fsck();

 exit(exitval);
}


/*++++++++++++++++++++++++++++++++++++++
  List the single specified URL from the cache.

  int wwwoffle_ls_url Return 1 in case of error or 0 if OK.

  URL *Url The URL to list.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_ls_url(URL *Url)
{
 int retval=0;

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return(1);}

 if(chdir(URLToDirName(Url)))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,URLToDirName(Url));ChangeBackToSpoolDir();return(1);}

 retval=ls(URLToFileName(Url,'D',0));

 ChangeBackToSpoolDir();

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  List the URLs within a directory of the cache.

  int wwwoffle_ls_dir Return 1 in case of error or 0 if OK.

  char *name The directory name (protocol/hostname) to list.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_ls_dir(char *name)
{
 int retval=0;
 struct dirent* ent;
 DIR *dir;
 char *slash;

 slash=strchr(name,'/');
 *slash++=0;

 if(chdir(name))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",name);return(1);}

 if(chdir(slash))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",name,slash);ChangeBackToSpoolDir();return(1);}

 dir=opendir(".");

 if(!dir)
   {PrintMessage(Warning,"Cannot open current directory '%s/%s' [%!s].",name,slash);ChangeBackToSpoolDir();return(1);}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read current directory '%s/%s' [%!s].",name,slash);closedir(dir);ChangeBackToSpoolDir();return(1);}

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(*ent->d_name=='D' && ent->d_name[strlen(ent->d_name)-1]!='~')
       retval+=ls(ent->d_name);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  List the URLs within the outgoing, monitor or lasttime/prevtime special directory of the cache.

  int wwwoffle_ls_special Return 1 in case of error or 0 if OK.

  char *name The name of the directory to list.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_ls_special(char *name)
{
 struct dirent* ent;
 DIR *dir;
 int retval=0;

 if(chdir(name))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",name);return(1);}

 dir=opendir(".");

 if(!dir)
   {PrintMessage(Warning,"Cannot open current directory '%s' [%!s].",name);ChangeBackToSpoolDir();return(1);}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read current directory '%s' [%!s].",name);closedir(dir);ChangeBackToSpoolDir();return(1);}

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if((*ent->d_name=='D' || *ent->d_name=='O') && ent->d_name[strlen(ent->d_name)-1]!='~')
       retval+=ls(ent->d_name);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  List one file.

  int ls Return 1 in case of error or 0 if OK.

  char *file The name of the file to ls.
  ++++++++++++++++++++++++++++++++++++++*/

static int ls(char *file)
{
 struct stat buf;
 time_t now=-1;

 if(now==-1)
    now=time(NULL);

 if(stat(file,&buf))
   {PrintMessage(Warning,"Cannot stat the file '%s' [%!s].",file);return(1);}
 else
   {
    URL *Url=FileNameToURL(file);

    if(Url)
      {
       char month[4];
       struct tm *tim=localtime(&buf.st_mtime);

       strftime(month,(size_t)4,"%b",tim);

       if(buf.st_mtime<(now-(180*24*3600)))
          printf("%s %7ld %3s %2d %5d %s\n",file,(long)buf.st_size,month,tim->tm_mday,tim->tm_year+1900,Url->file);
       else
          printf("%s %7ld %3s %2d %2d:%02d %s\n",file,(long)buf.st_size,month,tim->tm_mday,tim->tm_hour,tim->tm_min,Url->file);

       FreeURL(Url);
      }
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Move one URL or host to another.

  int wwwoffle_mv Return 1 in case of error or 0 if OK.

  URL *Url1 The source URL.

  URL *Url2 The destination URL.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_mv(URL *Url1,URL *Url2)
{
 struct dirent* ent;
 DIR *dir;

 if(chdir(Url1->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url1->proto);return(1);}

 if(chdir(URLToDirName(Url1)))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url1->proto,URLToDirName(Url1));ChangeBackToSpoolDir();return(1);}

 dir=opendir(".");

 if(!dir)
   {PrintMessage(Warning,"Cannot open current directory '%s/%s' [%!s].",Url1->proto,URLToDirName(Url1));ChangeBackToSpoolDir();return(1);}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read current directory '%s/%s' [%!s].",Url1->proto,URLToDirName(Url1));closedir(dir);ChangeBackToSpoolDir();return(1);}

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(*ent->d_name=='D')
      {
       URL *old_Url=FileNameToURL(ent->d_name);

       if(old_Url && !strncmp(Url1->name,old_Url->name,strlen(Url1->name)))
         {
          URL *new_Url;
          char *new_path,*old_name,*new_name;
          int newlen,oldlen;
#if MAKE_CHANGES
          int fd2;
#endif

          oldlen=strlen(Url1->path);
          newlen=strlen(Url2->path);

          new_path=(char*)malloc(newlen-oldlen+strlen(old_Url->path)+2);
          strcpy(new_path,Url2->path);

          if(Url2->path[newlen-1]!='/' && Url1->path[oldlen-1]=='/')
             strcat(new_path,"/");
          if(Url2->path[newlen-1]=='/' && Url1->path[oldlen-1]!='/')
             new_path[newlen-1]=0;

          strcat(new_path,old_Url->path+oldlen);

          new_Url=CreateURL(Url2->proto,Url2->hostport,new_path,old_Url->args,old_Url->user,old_Url->pass);
          free(new_path);

          old_name=URLToFileName(old_Url,'D',0);
          new_name=URLToFileName(new_Url,'D',0);

          printf("  - %s %s\n",old_name,old_Url->file);
          printf("  + %s %s\n",new_name,new_Url->file);

          new_path=(char*)malloc(strlen(new_Url->proto)+strlen(URLToDirName(new_Url))+strlen(new_name)+16);

          sprintf(new_path,"../../%s",new_Url->proto);
          mkdir(new_path,DEF_DIR_PERM);

          sprintf(new_path,"../../%s/%s",new_Url->proto,URLToDirName(new_Url));
          mkdir(new_path,DEF_DIR_PERM);

          *old_name=*new_name='D';
          sprintf(new_path,"../../%s/%s/%s",new_Url->proto,URLToDirName(new_Url),new_name);

#if MAKE_CHANGES
          rename(old_name,new_path);
#endif

          old_name=URLToFileName(old_Url,'U',0);
          new_name=URLToFileName(new_Url,'U',0);

          sprintf(new_path,"../../%s/%s/%s",new_Url->proto,URLToDirName(new_Url),new_name);

#if MAKE_CHANGES
          fd2=open(new_path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,DEF_FILE_PERM);

          if(fd2!=-1)
            {
             struct stat buf;
             struct utimbuf utbuf;

             init_io(fd2);
             write_string(fd2,new_Url->name);
             finish_io(fd2);
             close(fd2);

             if(!stat(old_name,&buf))
               {
                utbuf.actime=time(NULL);
                utbuf.modtime=buf.st_mtime;
                utime(new_path,&utbuf);
               }

             unlink(old_name);
            }
#endif

          free(new_path);
          FreeURL(new_Url);
         }

       if(old_Url)
          FreeURL(old_Url);
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a URL.

  int wwwoffle_rm Return 1 in case of error or 0 if OK.

  URL *Url The URL to delete.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_rm(URL *Url)
{
 char *error=NULL;

 printf("  - %s\n",Url->file);

#if MAKE_CHANGES
 error=DeleteWebpageSpoolFile(Url,0);
#endif

 return(!!error);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a URL and output on stdout.

  int wwwoffle_read Return 1 in case of error or 0 if OK.

  URL *Url The URL to read.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_read(URL *Url)
{
 char *line=NULL,buffer[IO_BUFFER_SIZE];
 int n,spool=OpenWebpageSpoolFile(1,Url);
#if USE_ZLIB
 int compression=0;
#endif

 if(spool==-1)
    return(1);

 init_io(1);
 init_io(spool);

 while((line=read_line(spool,line)))
   {
#if USE_ZLIB
    if(!strncmp(line,"Pragma: wwwoffle-compressed",(size_t)27))
       compression=2;
    else
#endif
       write_string(1,line);

    if(*line=='\r' || *line=='\n')
       break;
   }

#if USE_ZLIB
 if(compression)
    configure_io_zlib(spool,compression,-1);
#endif

 while((n=read_data(spool,buffer,IO_BUFFER_SIZE))>0)
    write_data(1,buffer,n);

 finish_io(1);
 finish_io(spool);
 close(spool);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Write a URL from the input on stdin.

  int wwwoffle_write Return 1 in case of error or 0 if OK.

  URL *Url The URL to write.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_write(URL *Url)
{
 char buffer[IO_BUFFER_SIZE];
 int n,spool=OpenWebpageSpoolFile(0,Url);

 if(spool==-1)
    return(1);

 init_io(0);
 init_io(spool);

 while((n=read_data(0,buffer,IO_BUFFER_SIZE))>0)
    write_data(spool,buffer,n);

 finish_io(0);
 finish_io(spool);
 close(spool);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Print the hash pattern for an URL
  
  int wwwoffle_hash Return 1 in case of error or 0 if OK.

  URL *Url The URL to be hashed.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_hash(URL *Url)
{
 char *name=URLToFileName(Url,'X',0);

 printf("%s\n",name+1);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Compress or uncompress the cached copy of the specified URLs.

  int wwwoffle_gzip Return 1 in case of error or 0 if OK.

  URL *Url The URL to be compressed.

  int compress A flag to indicate compression (1) or uncompression (0).
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_gzip(URL *Url,int compress)
{
 char *direction=compress?"compression":"uncompression";

#if USE_ZLIB
 DIR *dir;
 struct dirent* ent;
 struct stat buf;

 /* Change to the spool directory. */

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; %s failed.",Url->proto,direction);return(1);}

 if(chdir(URLToDirName(Url)))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s]; %s failed.",Url->proto,URLToDirName(Url),direction);ChangeBackToSpoolDir();return(1);}

 dir=opendir(".");

 if(!dir)
   {PrintMessage(Warning,"Cannot open current directory '%s/%s' [%!s]; %s failed.",Url->proto,URLToDirName(Url),direction);ChangeBackToSpoolDir();return(1);}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read current directory '%s/%s' [%!s]; %s failed.",Url->proto,URLToDirName(Url),direction);closedir(dir);ChangeBackToSpoolDir();return(1);}

 /* Go through each entry. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(!stat(ent->d_name,&buf) && S_ISREG(buf.st_mode) && *ent->d_name=='D')
       gzip_file(Url->proto,URLToDirName(Url),ent->d_name,compress);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 return(0);

#else

 fprintf(stderr,"Error: wwwoffle-tools was compiled without zlib, no %s possible.\n",direction);

 return(1);

#endif /* USE_ZLIB */
}


#if USE_ZLIB
/*++++++++++++++++++++++++++++++++++++++
  Uncompress the named file (if already compressed).

  char *proto The protocol directory to uncompress.

  char *hostport The host and port number directory to uncompress.

  char *file The name of the file in the current directory to uncompress.

  int compress Set to 1 if the file is to be compressed, 0 for uncompression.
  ++++++++++++++++++++++++++++++++++++++*/

static void gzip_file(char *proto,char *hostport,char *file,int compress)
{
 int ifd,ofd;
 char *zfile;
 Header *spool_head;
 char *head,buffer[IO_BUFFER_SIZE];
 int n;
 struct stat buf;
 struct utimbuf ubuf;
 char *direction=compress?"compress":"uncompress";

 if(!stat(file,&buf))
   {
    ubuf.actime=buf.st_atime;
    ubuf.modtime=buf.st_mtime;
   }
 else
   {
    PrintMessage(Inform,"Cannot stat file '%s/%s/%s' to %s it [%!s]; race condition?",proto,hostport,file,direction);
    return;
   }

 ifd=open(file,O_RDONLY|O_BINARY);

 if(ifd==-1)
   {
    PrintMessage(Inform,"Cannot open file '%s/%s/%s' to %s it [%!s]; race condition?",proto,hostport,file,direction);
    return;
   }

 init_io(ifd);

 ParseReply(ifd,&spool_head);

 if(!spool_head ||
    (compress && (GetHeader(spool_head,"Content-Encoding") ||
                  GetHeader2(spool_head,"Pragma","wwwoffle-compressed") ||
                  NotCompressed(GetHeader(spool_head,"Content-Type"),NULL))) ||
    (!compress && !GetHeader2(spool_head,"Pragma","wwwoffle-compressed")))
   {
    if(spool_head)
       FreeHeader(spool_head);

    finish_io(ifd);
    close(ifd);

    utime(file,&ubuf);
    return;
   }

 printf("    %s\n",file);

 if(compress)
   {
    AddToHeader(spool_head,"Content-Encoding","x-gzip");
    AddToHeader(spool_head,"Pragma","wwwoffle-compressed");
    RemoveFromHeader(spool_head,"Content-Length");
   }
 else
   {
    RemoveFromHeader(spool_head,"Content-Encoding");
    RemoveFromHeader2(spool_head,"Pragma","wwwoffle-compressed");
   }

 zfile=(char*)malloc(strlen(file)+4);
 strcpy(zfile,file);
 strcat(zfile,".z");

 ofd=open(zfile,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,buf.st_mode&07777);

 if(ofd==-1)
   {
    PrintMessage(Inform,"Cannot open file '%s/%s/%s' to %s to [%!s].",proto,hostport,zfile,direction);

    FreeHeader(spool_head);
    free(zfile);

    finish_io(ifd);
    close(ifd);

    utime(file,&ubuf);
    return;
   }

 init_io(ofd);

 head=HeaderString(spool_head);
 FreeHeader(spool_head);

 write_string(ofd,head);
 free(head);

 if(compress)
    configure_io_zlib(ofd,-1,2);
 else
    configure_io_zlib(ifd,2,-1);

 while((n=read_data(ifd,buffer,IO_BUFFER_SIZE))>0)
    write_data(ofd,buffer,n);

 finish_io(ifd);
 close(ifd);

 finish_io(ofd);
 close(ofd);

#if MAKE_CHANGES
 if(rename(zfile,file))
   {
    PrintMessage(Inform,"Cannot rename file '%s/%s/%s' to '%s/%s/%s' [%!s].",proto,hostport,zfile,proto,hostport,file);
    unlink(zfile);
   }
#else
 unlink(zfile);
#endif

 utime(file,&ubuf);

 free(zfile);
}
#endif


/*++++++++++++++++++++++++++++++++++++++
  Check the cache for inconsistent filenames and fix them.

  int wwwoffle_fsck Return 1 in case of any files corrected or 0 if OK.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_fsck(void)
{
 int i;
 struct stat buf;

 for(i=0;i<3;i++)
   {
    char *proto;

    if(i==0)
       proto="http";
    else if(i==1)
       proto="ftp";
    else
       proto="finger";

    if(stat(proto,&buf))
       PrintMessage(Inform,"Cannot stat the '%s' directory [%!s]; not checked.",proto);
    else
      {
       printf("Checking %s\n",proto);

       if(chdir(proto))
          PrintMessage(Warning,"Cannot change to the '%s' directory [%!s]; not checked.",proto);
       else
         {
          wwwoffle_fsck_check_proto(proto);

          ChangeBackToSpoolDir();
         }
      }
   }

 for(i=0;i<3;i++)
   {
    char *special;

    if(i==0)
       special="outgoing";
    else if(i==1)
       special="lasttime";
    else
       special="monitor";

    if(stat(special,&buf))
       PrintMessage(Inform,"Cannot stat the '%s' directory [%!s]; not checked.",special);
    else
      {
       printf("Checking %s\n",special);

       if(chdir(special))
          PrintMessage(Warning,"Cannot change to the '%s' directory [%!s]; not checked.",special);
       else
         {
          wwwoffle_fsck_check_dir(NULL,NULL,special);

          ChangeBackToSpoolDir();
         }
      }
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Check a complete protocol directory.

  char *proto The protocol of the spool directory we are in.
  ++++++++++++++++++++++++++++++++++++++*/

static void wwwoffle_fsck_check_proto(char *proto)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open spool directory '%s' [%!s]; checking failed.",proto);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s' [%!s]; checking failed.",proto);closedir(dir);return;}

 /* Go through each entry. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(stat(ent->d_name,&buf))
       PrintMessage(Warning,"Cannot stat file '%s/%s' [%!s] not checked.",proto,ent->d_name);
    else if(S_ISDIR(buf.st_mode))
      {
       if(chdir(ent->d_name))
          PrintMessage(Warning,"Cannot change to the '%s/%s' directory [%!s]; not checked.",proto,ent->d_name);
       else
         {
          printf("  Checking %s\n",ent->d_name);

          wwwoffle_fsck_check_dir(proto,ent->d_name,NULL);

          ChangeBackToSpoolDir();
          chdir(proto);
         }
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Check a complete directory directory, either one host or a special directory.

  char *proto The protocol of the spool directory we are in.

  char *host The hostname of the spool directory we are in.

  char *special The name of the special directory (either special or proto and host are non-NULL).
  ++++++++++++++++++++++++++++++++++++++*/

static void wwwoffle_fsck_check_dir(char *proto,char *host,char *special)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {
    if(special)
       PrintMessage(Warning,"Cannot open spool directory '%s' [%!s]; checking failed.",special);
    else
       PrintMessage(Warning,"Cannot open spool directory '%s/%s' [%!s]; checking failed.",proto,host);
    return;
   }

 ent=readdir(dir);
 if(!ent)
   {
    if(special)
       PrintMessage(Warning,"Cannot read spool directory '%s' [%!s]; checking failed.",special);
    else
       PrintMessage(Warning,"Cannot read spool directory '%s/%s' [%!s]; checking failed.",proto,host);
    closedir(dir);
    return;
   }

 /* Go through each entry. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(!stat(ent->d_name,&buf) && (*ent->d_name=='O' || *ent->d_name=='D')  && S_ISREG(buf.st_mode))
      {
       char *url=FileNameTo_url(ent->d_name);

       if(url)
         {
          URL *Url=SplitURL(url);
          char *newname=URLToFileName(Url,'X',0);
          char *oldname=ent->d_name;

          if(strncmp(oldname+1,newname+1,strlen(newname+1)) || strcmp(url,Url->file))
            {
#if MAKE_CHANGES
             int ufd;
#endif

             printf("  - %s %s\n",oldname+1,url);
             printf("  + %s %s\n",newname+1,Url->file);

#if MAKE_CHANGES
             *oldname=*newname='D';
             rename(oldname,newname);
             *oldname=*newname='U';
             rename(oldname,newname);
             if(special)
               {
                *oldname=*newname='O';
                rename(oldname,newname);
                *oldname=*newname='M';
                rename(oldname,newname);
               }

             ufd=open(newname,O_WRONLY|O_TRUNC|O_BINARY);

             if(ufd!=-1)
               {
                init_io(ufd);

                write_string(ufd,Url->file);

                finish_io(ufd);
                close(ufd);
               }
#endif
            }

          FreeURL(Url);
          free(url);
         }
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a filename to a URL.

  char *FileNameTo_url Returns the URL.

  char *file The file name.

  A copy of the function from spool.c but returning the actual string, not a canonicalised version of it.
  ++++++++++++++++++++++++++++++++++++++*/

static char *FileNameTo_url(char *file)
{
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

 return(url);
}
