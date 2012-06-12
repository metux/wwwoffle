/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/wwwoffled.c,v 2.83 2009-03-13 19:30:39 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9f.
  A demon program to maintain the database and spawn the servers.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996-2009 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

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

#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <grp.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "certificates.h"
#include "version.h"


static void usage(int verbose);
static void demoninit(void);
static void install_sighandlers(void);
static void sigchild(int signum);
static void sigexit(int signum);
static void sighup(int signum);


/*+ The server sockets that we listen on +*/
int http_fd[2]={-1,-1},          /*+ for the HTTP connections. +*/
    https_fd[2]={-1,-1},         /*+ for the HTTPS connections. +*/
    wwwoffle_fd[2]={-1,-1};      /*+ for the WWWOFFLE connections. +*/

/*+ The online / offline /autodial status. +*/
int online=0;

/*+ The current number of active servers +*/
int n_servers=0,                /*+ in total. +*/
    n_fetch_servers=0;          /*+ fetching a page. +*/

/*+ The maximum number of servers +*/
int max_servers=0,              /*+ in total. +*/
    max_fetch_servers=0;        /*+ fetching a page. +*/

/*+ The wwwoffle client file descriptor when fetching. +*/
int fetch_fd=-1;

/*+ The pids of the servers. +*/
int server_pids[MAX_SERVERS];

/*+ The pids of the servers that are fetching. +*/
int fetch_pids[MAX_FETCH_SERVERS];

/*+ The current purge status. +*/
int purging=0;

/*+ The pid of the purge process. +*/
int purge_pid=0;

/*+ The current status, fetching or not. +*/
int fetching=0;

/*+ Set when the demon is to shutdown due to a sigexit. +*/
static sig_atomic_t got_sigexit=0;

/*+ Set when the demon is to re-read config file due to a sighup. +*/
static sig_atomic_t got_sighup=0;

/*+ Remember that we got a SIGCHLD +*/
static sig_atomic_t got_sigchld=0;

/*+ True if run as a demon +*/
static int detached=1;

/*+ True if the pid of the daemon should be printed at startup +*/
static int print_pid=0;

/*+ The name of the configuration file specified. +*/
static char *config_file=NULL;

/*+ The name of the log file specified. +*/
char *log_file=NULL;

/*+ True if the -f option was passed on the command line. +*/
int nofork=0;


/*++++++++++++++++++++++++++++++++++++++
  The main program.
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc, char** argv)
{
 int i;
 int err;
 struct stat buf;
 int uid,gid;

 /* Parse the command line options */

 for(i=1;i<argc;i++)
   {
    if(!strcmp(argv[i],"-h") || !strcmp(argv[i],"--help"))
       usage(1);

    if(!strcmp(argv[i],"--version"))
       usage(2);

    if(!strcmp(argv[i],"-d"))
      {
       detached=0;

       if(i<(argc-1) && isdigit(argv[i+1][0]))
         {
          StderrLevel=Fatal+1-atoi(argv[++i]);

          if(StderrLevel<0 || StderrLevel>Fatal)
            {fprintf(stderr,"wwwoffled: The '-d' option requires a number between 0 and %d.\n",Fatal+1); exit(1);}
         }

       continue;
      }

    if(!strcmp(argv[i],"-l"))
      {
       if(++i>=argc)
         {fprintf(stderr,"wwwoffled: The '-l' option requires a filename.\n"); exit(1);}
       if(argv[i][0]!='/')
         {fprintf(stderr,"wwwoffled: The '-l' option requires an absolute pathname.\n"); exit(1);}

       log_file=argv[i];
       continue;
      }

    if(!strcmp(argv[i],"-p"))
      {
       print_pid=1;
       continue;
      }

    if(!strcmp(argv[i],"-f"))
      {
       nofork=1;
       detached=0;
       continue;
      }

    if(!strcmp(argv[i],"-c"))
      {
       if(++i>=argc)
         {fprintf(stderr,"wwwoffled: The '-c' option requires a filename.\n"); exit(1);}

       config_file=argv[i];
       continue;
      }

    fprintf(stderr,"wwwoffled: Unknown option '%s'.\n",argv[i]); exit(1);
   }

 /* Combination options */

 if(log_file)
   {
    if(nofork)
       log_file=NULL;           /* -f overrides -l */
    else
       detached=1;              /* -l overrides -d */

    if(StderrLevel==-1)
       StderrLevel=Inform;      /* -l sets log level if no -d */
   }

 /* Initialise things. */

 for(i=0;i<MAX_FETCH_SERVERS;i++)
    fetch_pids[i]=0;

 for(i=0;i<MAX_SERVERS;i++)
    server_pids[i]=0;

 if(log_file)
    OpenErrorLog(log_file);

 InitErrorHandler("wwwoffled",0,1); /* use stderr and not syslog to start with. */

 /* Read the configuration file. */

 InitConfigurationFile(config_file);

 init_io(STDERR_FILENO);

 if(ReadConfigurationFile(STDERR_FILENO))
    PrintMessage(Fatal,"Error in configuration file '%s'.",ConfigurationFileName());

 finish_io(STDERR_FILENO);

 InitErrorHandler("wwwoffled",ConfigInteger(UseSyslog),1); /* enable syslog if requested. */

 /* Print a startup message. */

#if USE_IPV6
#define IPV6_STRING "with ipv6"
#else
#define IPV6_STRING "without ipv6"
#endif

#define ZLIB_STRING "with zlib"

#define GNUTLS_STRING "with gnutls"

 PrintMessage(Important,"WWWOFFLE Demon Version %s (%s,%s,%s) started.",WWWOFFLE_VERSION,IPV6_STRING,ZLIB_STRING,GNUTLS_STRING);
 PrintMessage(Inform,"WWWOFFLE Read Configuration File '%s'.",ConfigurationFileName());

 /* Change the user and group. */

 gid=ConfigInteger(WWWOFFLE_Gid);
 uid=ConfigInteger(WWWOFFLE_Uid);

 if(uid!=-1)
    seteuid(0);

 if(log_file && (uid!=-1 || gid!=-1))
    chown(log_file,(uid_t)uid,(gid_t)gid);

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

 if(geteuid()==0 || getegid()==0)
    PrintMessage(Warning,"Running with root user or group privileges is not recommended.");

 /* Create, Change to and open the spool directory. */

 umask(0);

 if(stat(ConfigString(SpoolDir),&buf))
   {
    err=mkdir(ConfigString(SpoolDir),(mode_t)ConfigInteger(DirPerm));
    if(err==-1 && errno!=EEXIST)
       PrintMessage(Fatal,"Cannot create spool directory %s [%!s].",ConfigString(SpoolDir));
    stat(ConfigString(SpoolDir),&buf);
   }

 if(!S_ISDIR(buf.st_mode))
    PrintMessage(Fatal,"The spool directory %s is not a directory.",SpoolDir);

 err=ChangeToSpoolDir(ConfigString(SpoolDir));
 if(err==-1)
    PrintMessage(Fatal,"Cannot change to spool directory %s [%!s].",ConfigString(SpoolDir));

 /* Bind the HTTP proxy socket(s). */

#if USE_IPV6
 if(ConfigString(Bind_IPv6))
   {
    http_fd[1]=OpenServerSocket(ConfigString(Bind_IPv6),ConfigInteger(HTTP_Port));
    if(http_fd[1]==-1)
       PrintMessage(Fatal,"Cannot create HTTP IPv6 server socket.");
   }
#endif

 if(ConfigString(Bind_IPv4))
   {
    http_fd[0]=OpenServerSocket(ConfigString(Bind_IPv4),ConfigInteger(HTTP_Port));
    if(http_fd[0]==-1)
      {
#if USE_IPV6
       if(http_fd[1]!=-1 && /* PS 2003-01-13 redundant? If IPv6 is listening to 0 then it doesn't matter what address IPv4 is listening to... (at least on linux?) */
                            /* ConfigString(Bind_IPv4) && !strcmp(ConfigString(Bind_IPv4),"0.0.0.0") && */
                            ConfigString(Bind_IPv6) && !strcmp(ConfigString(Bind_IPv6),"[0:0:0:0:0:0:0:0]"))
          PrintMessage(Warning,"Cannot create HTTP IPv4 server socket (but the IPv6 one might accept IPv4 connections).");
       else
          PrintMessage(Fatal,"Cannot create HTTP IPv4 server socket.");
#else
       PrintMessage(Fatal,"Cannot create HTTP server socket.");
#endif
      }
   }

 if(http_fd[0]==-1 && http_fd[1]==-1)
   {
#if USE_IPV6
    PrintMessage(Fatal,"The IPv4 and IPv6 HTTP sockets were not bound; are they disabled in the config file?");
#else
    PrintMessage(Fatal,"The HTTP socket was not bound; is it disabled in the config file?");
#endif
   }

 /* Bind the HTTPS socket(s). */

 if(LoadRootCredentials())
   {
    PrintMessage(Warning,"Failed to read (or create if needed) the WWWOFFLE root certificates.");
   }

 if(LoadTrustedCertificates())
   {
    PrintMessage(Warning,"Failed to read in any trusted certificates.");
   }

#if USE_IPV6
 if(ConfigString(Bind_IPv6))
   {
    https_fd[1]=OpenServerSocket(ConfigString(Bind_IPv6),ConfigInteger(HTTPS_Port));
    if(https_fd[1]==-1)
       PrintMessage(Fatal,"Cannot create HTTPS IPv6 server socket.");
   }
#endif

 if(ConfigString(Bind_IPv4))
   {
    https_fd[0]=OpenServerSocket(ConfigString(Bind_IPv4),ConfigInteger(HTTPS_Port));
    if(https_fd[0]==-1)
      {
#if USE_IPV6
       if(https_fd[1]!=-1 && ConfigString(Bind_IPv4) && !strcmp(ConfigString(Bind_IPv4),"0.0.0.0") &&
                             ConfigString(Bind_IPv6) && !strcmp(ConfigString(Bind_IPv6),"[0:0:0:0:0:0:0:0]"))
          PrintMessage(Warning,"Cannot create HTTPS IPv4 server socket (but the IPv6 one might accept IPv4 connections).");
       else
          PrintMessage(Fatal,"Cannot create HTTPS IPv4 server socket.");
#else
       PrintMessage(Fatal,"Cannot create HTTPS server socket.");
#endif
      }
   }

 if(https_fd[0]==-1 && https_fd[1]==-1)
   {
#if USE_IPV6
    PrintMessage(Fatal,"The IPv4 and IPv6 HTTPS sockets were not bound; are they disabled in the config file?");
#else
    PrintMessage(Fatal,"The HTTPS socket was not bound; is it disabled in the config file?");
#endif
   }

 /* Bind the WWWOFFLE control socket(s). */

#if USE_IPV6
 if(ConfigString(Bind_IPv6))
   {
    wwwoffle_fd[1]=OpenServerSocket(ConfigString(Bind_IPv6),ConfigInteger(WWWOFFLE_Port));
    if(wwwoffle_fd[1]==-1)
       PrintMessage(Fatal,"Cannot create WWWOFFLE IPv6 server socket.");
   }
#endif

 if(ConfigString(Bind_IPv4))
   {
    wwwoffle_fd[0]=OpenServerSocket(ConfigString(Bind_IPv4),ConfigInteger(WWWOFFLE_Port));
    if(wwwoffle_fd[0]==-1)
      {
#if USE_IPV6
       if(wwwoffle_fd[1]!=-1 && ConfigString(Bind_IPv4) && !strcmp(ConfigString(Bind_IPv4),"0.0.0.0") &&
                                ConfigString(Bind_IPv6) && !strcmp(ConfigString(Bind_IPv6),"[0:0:0:0:0:0:0:0]")) {
          PrintMessage(Warning,"Cannot create WWWOFFLE IPv4 server socket (but the IPv6 one might accept IPv4 connections).");
          PrintMessage(Warning,"Consider adding \"bind-ipv4 = none\" to wwwoffle.conf if using IPv6.");
       }
       else
          PrintMessage(Fatal,"Cannot create WWWOFFLE IPv4 server socket.");
#else
       PrintMessage(Fatal,"Cannot create WWWOFFLE server socket.");
#endif
      }
   }

 if(wwwoffle_fd[0]==-1 && wwwoffle_fd[1]==-1)
   {
#if USE_IPV6
    PrintMessage(Fatal,"The IPv4 and IPv6 WWWOFFLE sockets were not bound; are they disabled in the config file?");
#else
    PrintMessage(Fatal,"The WWWOFFLE socket was not bound; is it disabled in the config file?");
#endif
   }

 /* Detach from terminal */

 if(detached)
   {
    demoninit();

    PrintMessage(Important,"Detached from terminal and changed pid to %d.",getpid());

    if(log_file)
       InitErrorHandler("wwwoffled",-1,-1); /* pid changes after detaching, keep stderr as was. */
    else
       InitErrorHandler("wwwoffled",-1, 0); /* pid changes after detaching, disable stderr. */

    if(!log_file)
       close(STDERR_FILENO);
   }

 close(STDIN_FILENO);
 close(STDOUT_FILENO);

 install_sighandlers();

 max_servers=ConfigInteger(MaxServers);
 max_fetch_servers=ConfigInteger(MaxFetchServers);

 /* Loop around waiting for connections. */

 PrintMessage(Inform,"WWWOFFLE Ready to accept connections.");

 do
   {
    struct timeval tv;
    fd_set readfd;
    int nfds=0,nfd=0;

    FD_ZERO(&readfd);

#if USE_IPV6
       for(nfd=0;nfd<=1;nfd++)
         {
#endif
          if(http_fd[nfd]>=nfds)
             nfds=http_fd[nfd]+1;
          if(https_fd[nfd]>=nfds)
             nfds=https_fd[nfd]+1;
          if(wwwoffle_fd[nfd]>=nfds)
             nfds=wwwoffle_fd[nfd]+1;

          if(n_servers<max_servers)
            {
             if(http_fd[nfd]!=-1)
                FD_SET(http_fd[nfd],&readfd);
             if(https_fd[nfd]!=-1)
                FD_SET(https_fd[nfd],&readfd);
            }

          if(wwwoffle_fd[nfd]!=-1)
             FD_SET(wwwoffle_fd[nfd],&readfd);
#if USE_IPV6
         }
#endif

    tv.tv_sec=10;
    tv.tv_usec=0;

    if(select(nfds,&readfd,NULL,NULL,&tv)!=-1)
      {
#if USE_IPV6
       for(nfd=0;nfd<=1;nfd++)
         {
#endif
          if(wwwoffle_fd[nfd]!=-1 && FD_ISSET(wwwoffle_fd[nfd],&readfd))
            {
             char *host,*ip;
             int port,client;

             client=AcceptConnect(wwwoffle_fd[nfd]);

             if(client>=0)
               {
                init_io(client);
                configure_io_timeout(client,ConfigInteger(SocketTimeout),ConfigInteger(SocketTimeout));

                if(SocketRemoteName(client,&host,&ip,&port))
                  {
                   finish_io(client);
                   CloseSocket(client);
                  }
                else
                  {
                   char *canonical_ip=CanonicaliseHost(ip);

                   if(IsAllowedConnectHost(host) || IsAllowedConnectHost(canonical_ip))
                     {
                      PrintMessage(Important,"WWWOFFLE Connection from host %s (%s).",host,canonical_ip); /* Used in audit-usage.pl */

                      CommandConnect(client);

                      if(fetch_fd!=client)
                        {
                         finish_io(client);
                         CloseSocket(client);
                        }
                     }
                   else
                     {
                      PrintMessage(Warning,"WWWOFFLE Connection rejected from host %s (%s).",host,canonical_ip); /* Used in audit-usage.pl */
                      finish_io(client);
                      CloseSocket(client);
                     }

                   free(canonical_ip);
                  }
               }
            }

          if(http_fd[nfd]!=-1 && FD_ISSET(http_fd[nfd],&readfd))
            {
             char *host,*ip;
             int port,client;

             client=AcceptConnect(http_fd[nfd]);

             if(client>=0)
               {
                init_io(client);
                configure_io_timeout(client,ConfigInteger(SocketTimeout),ConfigInteger(SocketTimeout));

                if(!SocketRemoteName(client,&host,&ip,&port))
                  {
                   char *canonical_ip=CanonicaliseHost(ip);

                   if(IsAllowedConnectHost(host) || IsAllowedConnectHost(canonical_ip))
                     {
                      PrintMessage(Inform,"HTTP Proxy connection from host %s (%s).",host,canonical_ip); /* Used in audit-usage.pl */
                      ForkServer(client);
                     }
                   else
                      PrintMessage(Warning,"HTTP Proxy connection rejected from host %s (%s).",host,canonical_ip); /* Used in audit-usage.pl */

                   free(canonical_ip);
                  }

                if(nofork)
                   got_sigexit=1;

                if(!nofork)
                  {
                   finish_io(client);
                   CloseSocket(client);
                  }
               }
            }

          if(https_fd[nfd]!=-1 && FD_ISSET(https_fd[nfd],&readfd))
            {
             char *host,*ip;
             int port,client;

             client=AcceptConnect(https_fd[nfd]);

             if(client>=0)
               {
                init_io(client);
                configure_io_timeout(client,ConfigInteger(SocketTimeout),ConfigInteger(SocketTimeout));

                if(!SocketRemoteName(client,&host,&ip,&port))
                  {
                   char *canonical_ip=CanonicaliseHost(ip);

                   if(IsAllowedConnectHost(host) || IsAllowedConnectHost(canonical_ip))
                     {
                      PrintMessage(Inform,"HTTPS Proxy connection from host %s (%s).",host,canonical_ip); /* Used in audit-usage.pl */
                      ForkServer(client);
                     }
                   else
                      PrintMessage(Warning,"HTTPS Proxy connection rejected from host %s (%s).",host,canonical_ip); /* Used in audit-usage.pl */

                   free(canonical_ip);
                  }

                if(nofork)
                   got_sigexit=1;

                if(!nofork)
                  {
                   finish_io(client);
                   CloseSocket(client);
                  }
               }
            }

#if USE_IPV6
         }
#endif
      }

    if(got_sighup)
      {
       got_sighup=0;

       PrintMessage(Important,"SIGHUP signalled.");

       if(log_file)
         {
          PrintMessage(Important,"Closing and opening log file.");

          OpenErrorLog(log_file);
         }

       PrintMessage(Important,"WWWOFFLE Re-reading Configuration File.");

       if(ReadConfigurationFile(-1))
          PrintMessage(Warning,"Error in configuration file; keeping old values.");

       PrintMessage(Important,"WWWOFFLE Finished Re-reading Configuration File.");
      }

    if(got_sigchld)
      {
       int pid, status;
       int isserver=0;

       /* To avoid race conditions, reset the flag before fetching the status */

       got_sigchld=0;

       while((pid=waitpid(-1,&status,WNOHANG))>0)
         {
          int i;
          int exitval=0;

          if(WIFEXITED(status))
             exitval=WEXITSTATUS(status);
          else if(WIFSIGNALED(status))
             exitval=-WTERMSIG(status);

          if(purging)
             if(purge_pid==pid)
               {
                if(exitval>=0)
                   PrintMessage(Inform,"Purge process exited with status %d (pid=%d).",exitval,pid);
                else
                   PrintMessage(Important,"Purge process terminated by signal %d (pid=%d).",-exitval,pid);

                purging=0;
               }

          for(i=0;i<max_servers;i++)
             if(server_pids[i]==pid)
               {
                n_servers--;
                server_pids[i]=0;
                isserver=1;

                if(exitval>=0)
                   PrintMessage(Inform,"Child wwwoffles exited with status %d (pid=%d).",exitval,pid);
                else
                   PrintMessage(Important,"Child wwwoffles terminated by signal %d (pid=%d).",-exitval,pid);

                break;
               }

          /* Check if the child that terminated is one of the fetching wwwoffles */

          for(i=0;i<max_fetch_servers;i++)
             if(fetch_pids[i]==pid)
               {
                n_fetch_servers--;
                fetch_pids[i]=0;
                break;
               }

          if(exitval==3)
             fetching=0;

          if(exitval==4 && online!=0)
             fetching=1;

          if(online==0)
             fetching=0;
         }

       if(isserver)
          PrintMessage(Debug,"Currently running: %d servers total, %d fetchers.",n_servers,n_fetch_servers);
      }

    /* The select timed out or we got a signal. If we are currently fetching,
       start fetch servers to look for jobs in the spool directory. */

    while(fetching && n_fetch_servers<max_fetch_servers && n_servers<max_servers)
       ForkServer(fetch_fd);

    if(fetch_fd!=-1 && !fetching && n_fetch_servers==0)
      {
       write_string(fetch_fd,"WWWOFFLE No more to fetch.\n");

       finish_io(fetch_fd);
       CloseSocket(fetch_fd);
       fetch_fd=-1;

       PrintMessage(Important,"WWWOFFLE Fetch finished.");

       ForkRunModeScript(ConfigString(RunFetch),"fetch","stop",-1);
      }
   }
 while(!got_sigexit);

 /* Close down and exit. */

 /* These four sockets don't need finish_io() calling because they never
    had init_io() called, they are just bound to a port listening. */

 if(http_fd[0]!=-1) CloseSocket(http_fd[0]);
 if(http_fd[1]!=-1) CloseSocket(http_fd[1]);
 if(wwwoffle_fd[0]!=-1) CloseSocket(wwwoffle_fd[0]);
 if(wwwoffle_fd[1]!=-1) CloseSocket(wwwoffle_fd[1]);

 if(!nofork)
   {
    if(n_servers)
       PrintMessage(Important,"Exit signalled - waiting for %d child wwwoffles servers.",n_servers);
    else
       PrintMessage(Important,"Exit signalled.");
   }

 while(n_servers)
   {
    int i;
    int pid,status,exitval=0;

    while((pid=waitpid(-1,&status,0))>0)
      {
       if(WIFEXITED(status))
          exitval=WEXITSTATUS(status);
       else if(WIFSIGNALED(status))
          exitval=-WTERMSIG(status);

       for(i=0;i<max_servers;i++)
          if(server_pids[i]==pid)
            {
             n_servers--;
             server_pids[i]=0;

             if(exitval>=0)
                PrintMessage(Inform,"Child wwwoffles exited with status %d (pid=%d).",exitval,pid);
             else
                PrintMessage(Important,"Child wwwoffles terminated by signal %d (pid=%d).",-exitval,pid);

             break;
            }

       if(purging)
          if(purge_pid==pid)
            {
             if(exitval>=0)
                PrintMessage(Inform,"Purge process exited with status %d (pid=%d).",exitval,pid);
             else
                PrintMessage(Important,"Purge process terminated by signal %d (pid=%d).",-exitval,pid);

             purging=0;
            }
      }
   }

 PrintMessage(Important,"Exiting.");

 FinishConfigurationFile();

 FreeLoadedCredentials();

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Print the program usage in long or short format.

  int verbose Set to 0 for short format, 1 for long format, 2 for version.
  ++++++++++++++++++++++++++++++++++++++*/

static void usage(int verbose)
{
 if(verbose==2)
   {
    fprintf(stderr,
            "wwwoffled version %s\n",
            WWWOFFLE_VERSION);
    exit(0);
   }

 fprintf(stderr,
         "\n"
         "WWWOFFLED - World Wide Web Offline Explorer (Daemon) - Version %s\n"
         "\n",WWWOFFLE_VERSION);

 if(verbose)
    fprintf(stderr,
            "(c) Andrew M. Bishop 1996,97,98,99,2000,01,02,03 [       amb@gedanken.demon.co.uk ]\n"
            "                                                 [http://www.gedanken.demon.co.uk/]\n"
            "\n");

 fprintf(stderr,
         "Usage: wwwoffled -h | --help | --version\n"
         "       wwwoffled [-c <config-file>] [-d [<log-level>]] [-l <log-file>] [-f] [-p]\n"
         "\n");

 if(verbose)
    fprintf(stderr,
            "   -h | --help      : Display this help.\n"
            "\n"
            "   --version        : Display the version number of wwwoffled.\n"
            "\n"
            "   -c <config-file> : The name of the configuration file to use.\n"
            "                      (Default %s).\n"
            "\n"
            "   -d [<log-level>] : Do not detach from the terminal and use stderr.\n"
            "                      0 <= log-level <= %d (default in config file).\n"
            "\n"
            "   -l <log-file>    : Detach from the terminal and use specified log file.\n"
            "                      (Using log-level = 4 unless -d specifies different.)\n"
            "\n"
            "   -f               : Do not fork children for each HTTP request.\n"
            "                      (Exits after first request, implies -d, overrides -l.)\n"
            "\n"
            "   -p               : Print the pid of wwwoffled on stdout (unless -d).\n"
            "\n",ConfigurationFileName(),Fatal+1);

 exit(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Detach ourself from the controlling terminal.
  ++++++++++++++++++++++++++++++++++++++*/

static void demoninit(void)
{
 pid_t pid=0;

 pid=fork();
 if(pid==-1)
    PrintMessage(Fatal,"Cannot fork() to detach(1) [%!s].");
 else if(pid)
    exit(0);

 setsid();

 pid=fork();
 if(pid==-1)
    PrintMessage(Fatal,"Cannot fork() to detach(2) [%!s].");
 else if(pid)
   {
    /* print the child pid on stdout as requested */
    if(print_pid)
       printf("%d\n", pid);

    exit(0);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Install the signal handlers.
  ++++++++++++++++++++++++++++++++++++++*/

static void install_sighandlers(void)
{
 struct sigaction action;

 /* SIGCHLD */
 action.sa_handler = sigchild;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGCHLD, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot install SIGCHLD handler.");

 /* SIGINT, SIGQUIT, SIGTERM */
 action.sa_handler = sigexit;
 sigemptyset(&action.sa_mask);
 sigaddset(&action.sa_mask, SIGINT);           /* Block all of them */
 sigaddset(&action.sa_mask, SIGQUIT);
 sigaddset(&action.sa_mask, SIGTERM);
 action.sa_flags = 0;
 if(sigaction(SIGINT, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot install SIGINT handler.");
 if(sigaction(SIGQUIT, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot install SIGQUIT handler.");
 if(sigaction(SIGTERM, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot install SIGTERM handler.");

 /* SIGHUP */
 action.sa_handler = sighup;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGHUP, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot install SIGHUP handler.");

 /* SIGPIPE */
 action.sa_handler = SIG_IGN;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGPIPE, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot ignore SIGPIPE.");
}


/*++++++++++++++++++++++++++++++++++++++
  The signal handler for the child exiting.

  int signum The signal number.
  ++++++++++++++++++++++++++++++++++++++*/

static void sigchild(/*@unused@*/ int signum)
{
 got_sigchld=1;
}


/*++++++++++++++++++++++++++++++++++++++
  The signal handler for the signals to tell us to exit.

  int signum The signal number.
  ++++++++++++++++++++++++++++++++++++++*/

static void sigexit(/*@unused@*/ int signum)
{
 got_sigexit=1;
}


/*++++++++++++++++++++++++++++++++++++++
  The signal handler for the signals to tell us to re-read the config file.

  int signum The signal number.
  ++++++++++++++++++++++++++++++++++++++*/

static void sighup(/*@unused@*/ int signum)
{
 got_sighup=1;
}
