/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/connect.c,v 2.54 2010-01-22 19:00:53 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9f.
  Handle WWWOFFLE connections received by the demon.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996-2010 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

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

#include <signal.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "version.h"


/*+ The time that the program went online. +*/
extern time_t OnlineTime;

/*+ The time that the program went offline. +*/
extern time_t OfflineTime;

/*+ The server sockets that we listen on +*/
extern int http_fd[2],          /*+ for the HTTP connections. +*/
           https_fd[2],         /*+ for the HTTPS connections. +*/
           wwwoffle_fd[2];      /*+ for the WWWOFFLE connections. +*/

/*+ The online / offline / autodial status. +*/
extern int online;

/*+ The current number of active servers +*/
extern int n_servers,           /*+ in total. +*/
           n_fetch_servers;     /*+ fetching a previously requested page. +*/

/*+ The maximum number of servers +*/
extern int max_servers,                /*+ in total. +*/
           max_fetch_servers;          /*+ fetching a page. +*/

/*+ The wwwoffle client file descriptor when fetching. +*/
extern int fetch_fd;

/*+ The pids of the servers. +*/
extern int server_pids[MAX_SERVERS];

/*+ The pids of the servers that are fetching. +*/
extern int fetch_pids[MAX_FETCH_SERVERS];

/*+ The purge status. +*/
extern int purging;

/*+ The pid of the purge process. +*/
extern int purge_pid;

/*+ The current status, fetching or not. +*/
extern int fetching;

/*+ True if the -f option was passed on the command line. +*/
extern int nofork;

/*+ The name of the log file specified. +*/
extern char *log_file;


/*++++++++++++++++++++++++++++++++++++++
  Parse a request that comes from wwwoffle.

  int client The file descriptor that corresponds to the wwwoffle connection.
  ++++++++++++++++++++++++++++++++++++++*/

void CommandConnect(int client)
{
 char *line=NULL;

 if(!(line=read_line(client,line)))
   {PrintMessage(Warning,"Nothing to read from the wwwoffle control socket [%!s]."); return;}

 if(strncmp(line,"WWWOFFLE ",(size_t)9))
   {
    PrintMessage(Warning,"WWWOFFLE Not a command."); /* Used in audit-usage.pl */
    free(line);
    return;
   }

 if(ConfigString(PassWord) || !strncmp(&line[9],"PASSWORD ",(size_t)9))
   {
    char *password=&line[18];
    int i;

    for(i=18;line[i];i++)
       if(line[i]=='\r' || line[i]=='\n')
          line[i]=0;

    if(!ConfigString(PassWord) || strcmp(password,ConfigString(PassWord)))
      {
       write_string(client,"WWWOFFLE Incorrect Password\n"); /* Used in wwwoffle.c */
       PrintMessage(Warning,"WWWOFFLE Incorrect Password."); /* Used in audit-usage.pl */
       free(line);
       return;
      }

    if(!(line=read_line(client,line)))
      {PrintMessage(Warning,"Unexpected end of wwwoffle control command [%!s]."); return;}

    if(strncmp(line,"WWWOFFLE ",(size_t)9))
      {
       PrintMessage(Warning,"WWWOFFLE Not a command."); /* Used in audit-usage.pl */
       free(line);
       return;
      }
   }

 if(!strncmp(&line[9],"ONLINE",(size_t)6))
   {
    if(online==1)
       write_string(client,"WWWOFFLE Already Online\n"); /* Used in wwwoffle.c */
    else
      {
       write_string(client,"WWWOFFLE Now Online\n");
       PrintMessage(Important,"WWWOFFLE Online."); /* Used in audit-usage.pl */

       CycleLastTimeSpoolFile();

       ForkRunModeScript(ConfigString(RunOnline),"online",NULL,client);
      }

    OnlineTime=time(NULL);
    online=1;

    if(fetch_fd!=-1)
       fetching=1;
   }
 else if(!strncmp(&line[9],"AUTODIAL",(size_t)8))
   {
    if(online==-1)
       write_string(client,"WWWOFFLE Already in Autodial Mode\n"); /* Used in wwwoffle.c */
    else
      {
       write_string(client,"WWWOFFLE Now In Autodial Mode\n");
       PrintMessage(Important,"WWWOFFLE In Autodial Mode."); /* Used in audit-usage.pl */

       ForkRunModeScript(ConfigString(RunAutodial),"autodial",NULL,client);
      }

    OnlineTime=time(NULL);
    online=-1;
   }
 else if(!strncmp(&line[9],"OFFLINE",(size_t)7))
   {
    if(!online)
       write_string(client,"WWWOFFLE Already Offline\n"); /* Used in wwwoffle.c */
    else
      {
       write_string(client,"WWWOFFLE Now Offline\n");
       PrintMessage(Important,"WWWOFFLE Offline."); /* Used in audit-usage.pl */

       ForkRunModeScript(ConfigString(RunOffline),"offline",NULL,client);
      }

    OfflineTime=time(NULL);
    online=0;
   }
 else if(!strncmp(&line[9],"FETCH",(size_t)5))
   {
    if(fetch_fd!=-1)
      {
       fetching=1;
       write_string(client,"WWWOFFLE Already Fetching.\n"); /* Used in wwwoffle.c */
      }
    else if(online==0)
       write_string(client,"WWWOFFLE Must be online or autodial to fetch.\n"); /* Used in wwwoffle.c */
    else
      {
       write_string(client,"WWWOFFLE Now Fetching.\n");
       PrintMessage(Important,"WWWOFFLE Fetch."); /* Used in audit-usage.pl */

       RequestMonitoredPages();

       CycleLastOutSpoolFile();

       fetch_fd=client;
       fetching=1;

       ForkRunModeScript(ConfigString(RunFetch),"fetch","start",fetch_fd);
      }
   }
 else if(!strncmp(&line[9],"CONFIG",(size_t)6))
   {
    write_string(client,"WWWOFFLE Reading Configuration File.\n");
    PrintMessage(Important,"WWWOFFLE Re-reading Configuration File."); /* Used in audit-usage.pl */

    if(ReadConfigurationFile(client))
      {
       PrintMessage(Warning,"Error in configuration file; keeping old values.");
       write_string(client,"WWWOFFLE Error Reading Configuration File.\n");
      }
    else
       write_string(client,"WWWOFFLE Read Configuration File.\n");

    PrintMessage(Important,"WWWOFFLE Finished Re-reading Configuration File.");
   }
 else if(!strncmp(&line[9],"DUMP",(size_t)4))
   {
    PrintMessage(Important,"WWWOFFLE Dumping Configuration File."); /* Used in audit-usage.pl */

    DumpConfigFile(client);

    PrintMessage(Important,"WWWOFFLE Finished Dumping Configuration File.");
   }
 else if(!strncmp(&line[9],"CYCLELOG",(size_t)8))
   {
    if(log_file)
      {
       write_string(client,"WWWOFFLE Cycling Log File.\n");
       PrintMessage(Important,"Closing and opening log file.");

       OpenErrorLog(log_file);
      }
    else
       write_string(client,"WWWOFFLE Has No Log File.\n"); /* Used in wwwoffle.c */
   }
 else if(!strncmp(&line[9],"PURGE",(size_t)5))
   {
    pid_t pid;

    if(nofork)
      {
       write_string(client,"WWWOFFLE Purge Starting.\n");
       PrintMessage(Important,"WWWOFFLE Purge."); /* Used in audit-usage.pl */

       PurgeCache(client);

       write_string(client,"WWWOFFLE Purge Finished.\n");
       PrintMessage(Important,"WWWOFFLE Purge finished.");
      }
    else if((pid=fork())==-1)
       PrintMessage(Warning,"Cannot fork to do a purge [%!s].");
    else if(pid)
      {
       purging=1;
       purge_pid=pid;
      }
    else
      {
       if(fetch_fd!=-1)
         {
          finish_io(fetch_fd);
          CloseSocket(fetch_fd);
         }

       /* These six sockets don't need finish_io() calling because they never
          had init_io() called, they are just bound to a port listening. */

       if(http_fd[0]!=-1) CloseSocket(http_fd[0]);
       if(http_fd[1]!=-1) CloseSocket(http_fd[1]);
       if(https_fd[0]!=-1) CloseSocket(https_fd[0]);
       if(https_fd[1]!=-1) CloseSocket(https_fd[1]);
       if(wwwoffle_fd[0]!=-1) CloseSocket(wwwoffle_fd[0]);
       if(wwwoffle_fd[1]!=-1) CloseSocket(wwwoffle_fd[1]);

       write_string(client,"WWWOFFLE Purge Starting.\n");
       PrintMessage(Important,"WWWOFFLE Purge."); /* Used in audit-usage.pl */

       PurgeCache(client);

       write_string(client,"WWWOFFLE Purge Finished.\n");
       PrintMessage(Important,"WWWOFFLE Purge finished.");

       finish_io(client);
       CloseSocket(client);

       exit(0);
      }
   }
 else if(!strncmp(&line[9],"STATUS",(size_t)6))
   {
    int i;

    PrintMessage(Important,"WWWOFFLE Status."); /* Used in audit-usage.pl */

    write_string(client,"WWWOFFLE Server Status\n");
    write_string(client,"----------------------\n");

    write_formatted(client,"Version      : %s\n",WWWOFFLE_VERSION);

    if(online==1)
       write_string(client,"State        : online\n");
    else if(online==-1)
       write_string(client,"State        : autodial\n");
    else
       write_string(client,"State        : offline\n");

    if(fetch_fd!=-1)
       write_string(client,"Fetch        : active (wwwoffle -fetch)\n");
    else if(fetching==1)
       write_string(client,"Fetch        : active (recursive request)\n");
    else
       write_string(client,"Fetch        : inactive\n");

    if(purging)
       write_string(client,"Purge        : active\n");
    else
       write_string(client,"Purge        : inactive\n");

    if(OnlineTime)
       write_formatted(client,"Last-Online  : %s\n",RFC822Date(OnlineTime,0));
    else
       write_formatted(client,"Last-Online  : not since started\n");

    if(OfflineTime)
       write_formatted(client,"Last-Offline : %s\n",RFC822Date(OfflineTime,0));
    else
       write_formatted(client,"Last-Offline : not since started\n");

    write_formatted(client,"Total-Servers: %d\n",n_servers);
    write_formatted(client,"Fetch-Servers: %d\n",n_fetch_servers);

    if(n_servers)
      {
       write_string(client,"Server-PIDs  : ");
       for(i=0;i<max_servers;i++)
          if(server_pids[i])
             write_formatted(client," %d",server_pids[i]);
       write_string(client,"\n");
      }
    if(n_fetch_servers)
      {
       write_string(client,"Fetch-PIDs   : ");
       for(i=0;i<max_fetch_servers;i++)
          if(fetch_pids[i])
             write_formatted(client," %d",fetch_pids[i]);
       write_string(client,"\n");
      }
    if(purging)
       write_formatted(client,"Purge-PID    : %d\n",purge_pid);
   }
 else if(!strncmp(&line[9],"KILL",(size_t)4))
   {
    write_string(client,"WWWOFFLE Kill Signalled.\n");
    PrintMessage(Important,"WWWOFFLE Kill."); /* Used in audit-usage.pl */

    raise(SIGINT);
   }
 else
   {
    while(line[strlen(line)-1]=='\r' || line[strlen(line)-1]=='\n')
       line[strlen(line)-1]=0;

    write_formatted(client,"WWWOFFLE Unknown Command '%s'.",line);
    PrintMessage(Warning,"WWWOFFLE Unknown control command '%s'.",line); /* Used in audit-usage.pl */
   }

 if(line)
    free(line);
}


/*++++++++++++++++++++++++++++++++++++++
  Run the associated program when changing mode.

  char *filename The name of the program to run or NULL if none.

  char *mode The new mode to use as the argument.

  char *arg An additional argument (used for fetch mode).

  int client The current client socket to be closed.
  ++++++++++++++++++++++++++++++++++++++*/

void ForkRunModeScript(char *filename,char *mode,char *arg,int client)
{
 pid_t pid;

 if(!filename)
    return;

 if((pid=fork())==-1)
   {PrintMessage(Warning,"Cannot fork to run the run-%s program [%!s].",mode);return;}
 else if(!pid) /* The child */
   {
    if(fetch_fd!=-1)
      {
       finish_io(fetch_fd);
       CloseSocket(fetch_fd);
      }

    /* These six sockets don't need finish_io() calling because they never
       had init_io() called, they are just bound to a port listening. */

    if(http_fd[0]!=-1) CloseSocket(http_fd[0]);
    if(http_fd[1]!=-1) CloseSocket(http_fd[1]);
    if(https_fd[0]!=-1) CloseSocket(https_fd[0]);
    if(https_fd[1]!=-1) CloseSocket(https_fd[1]);
    if(wwwoffle_fd[0]!=-1) CloseSocket(wwwoffle_fd[0]);
    if(wwwoffle_fd[1]!=-1) CloseSocket(wwwoffle_fd[1]);

    if(client!=fetch_fd)
      {
       finish_io(client);
       CloseSocket(client);
      }

    if(arg)
       execl(filename,filename,mode,arg,NULL);
    else
       execl(filename,filename,mode,NULL);

    PrintMessage(Warning,"Cannot exec the run-%s program '%s' [%!s].",mode,filename);
    exit(1);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Fork a wwwoffles server.

  int fd The file descriptor that the data is transfered on.
  ++++++++++++++++++++++++++++++++++++++*/

void ForkServer(int fd)
{
 pid_t pid;
 int i;
 int fetcher=0;

 if(fetching && fetch_fd==fd)
    fetcher=1;

 if(nofork)
    wwwoffles(online,fetcher,fd);
 else if(!nofork && (pid=fork())==-1)
    PrintMessage(Warning,"Cannot fork a server [%!s].");
 else if(pid) /* The parent */
   {
    for(i=0;i<max_servers;i++)
       if(server_pids[i]==0)
         {server_pids[i]=pid;break;}

    n_servers++;

    if(online!=0 && fetcher)
      {
       for(i=0;i<max_fetch_servers;i++)
          if(fetch_pids[i]==0)
            {fetch_pids[i]=pid;break;}

       n_fetch_servers++;
      }

    /* Used in audit-usage.pl */
    PrintMessage(Inform,"Forked wwwoffles -%s (pid=%d).",online==1?(fetcher?"fetch":"real"):(online==-1?"autodial":"spool"),pid);
   }
 else /* The child */
   {
    int status;

    if(fetch_fd!=-1 && fetch_fd!=fd)
      {
       finish_io(fetch_fd);
       CloseSocket(fetch_fd);
      }

    /* These six sockets don't need finish_io() calling because they never
       had init_io() called, they are just bound to a port listening. */

    if(http_fd[0]!=-1) CloseSocket(http_fd[0]);
    if(http_fd[1]!=-1) CloseSocket(http_fd[1]);
    if(https_fd[0]!=-1) CloseSocket(https_fd[0]);
    if(https_fd[1]!=-1) CloseSocket(https_fd[1]);
    if(wwwoffle_fd[0]!=-1) CloseSocket(wwwoffle_fd[0]);
    if(wwwoffle_fd[1]!=-1) CloseSocket(wwwoffle_fd[1]);

    status=wwwoffles(online,fetcher,fd);

    exit(status);
   }
}
