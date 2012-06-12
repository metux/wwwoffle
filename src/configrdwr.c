/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configrdwr.c 1.73 2006/01/07 16:10:38 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Configuration file reading and writing functions.
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

#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include "io.h"
#include "misc.h"
#include "proto.h"
#include "errors.h"
#include "configpriv.h"
#include "config.h"


#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif

#ifndef PATH_MAX
/*+ The maximum pathname length in characters. +*/
#define PATH_MAX 4096
#endif


/*+ The state of the parser +*/

typedef enum _ParserState
{
 OutsideSection,                /*+ Outside of a section, (a comment or blank). +*/

 StartSection,                  /*+ After seeing the sectio nname before deciding the type. +*/
 StartSectionCurly,             /*+ After seeing the curly bracket '{'. +*/
 StartSectionSquare,            /*+ After seeing the square bracket '['. +*/
 StartSectionIncluded,          /*+ After opening the included file. +*/

 InsideSectionCurly,            /*+ Parsing within a normal section delimited by '{' & '}'. +*/
 InsideSectionSquare,           /*+ Looking for the included filename within a section delimited by '[' & ']'. +*/
 InsideSectionIncluded,         /*+ Parsing within an included file. +*/

 Finished                       /*+ At end of file. +*/
}
ParserState;


/* Local functions */

static char *filename_or_symlink_target(const char *name);

static /*@null@*/ char *InitParser(void);
static /*@null@*/ char *ParseLine(/*@out@*/ char **line);
static /*@null@*/ char *ParseItem(char *line,/*@out@*/ char **url_str,/*@out@*/ char **key_str,/*@out@*/ char **val_str);
static /*@null@*/ char *ParseEntry(const ConfigItemDef *itemdef,/*@out@*/ ConfigItem *item,/*@null@*/ const char *url_str,const char *key_str,/*@null@*/ const char *val_str);

static int isanumber(const char *string);


/* Local variables */

static char *parse_name;        /*+ The name of the configuration file. +*/
static int parse_file;          /*+ The file descriptor of the configuration file. +*/
static int parse_line;          /*+ The line number in the configuration file. +*/

static char *parse_name_org;    /*+ The name of the original configuration file when parsing an included one. +*/
static int parse_file_org;      /*+ The file descriptor of the original configuration file when parsing an included one. +*/
static int parse_line_org;      /*+ The line number in the original configuration file when parsing an included one. +*/

static int parse_section;       /*+ The current section of the configuration file. +*/
static int parse_item;          /*+ The current item in the configuration file. +*/
static ParserState parse_state; /*+ The parser state. +*/


/*++++++++++++++++++++++++++++++++++++++
  Read the data from the file.

  char *ReadConfigFile Returns the error message or NULL if OK.

  int read_startup If true then only the startup section of the configuration file is read.
                   If false then only the other sections are read.
  ++++++++++++++++++++++++++++++++++++++*/

char *ReadConfigFile(int read_startup)
{
 char *errmsg=NULL;

 CreateBackupConfigFile();

 errmsg=InitParser();

 if(!errmsg)
   {
    char *line=NULL;

    do
      {
       if((errmsg=ParseLine(&line)))
          break;

       if((parse_section==0 && read_startup) ||
          (parse_section>0 && !read_startup))
         {
          char *url_str,*key_str,*val_str;

          if((errmsg=ParseItem(line,&url_str,&key_str,&val_str)))
             break;

          if(parse_item!=-1)
            {
             if((errmsg=ParseEntry(&CurrentConfig.sections[parse_section]->itemdefs[parse_item],
                                   CurrentConfig.sections[parse_section]->itemdefs[parse_item].item,
                                   url_str,key_str,val_str)))
                break;
            }
         }
      }
    while(parse_state!=Finished);

    if(line)
       free(line);
   }

 if(parse_file!=-1)
   {
    finish_io(parse_file);
    close(parse_file);
   }
 if(parse_file_org!=-1)
   {
    finish_io(parse_file_org);
    close(parse_file_org);
   }

 if(errmsg)
    RestoreBackupConfigFile();
 else
    PurgeBackupConfigFile(!read_startup);

 if(errmsg)
   {
    char *newerrmsg=(char*)malloc(strlen(errmsg)+64+MAX_INT_STR+strlen(parse_name));
    sprintf(newerrmsg,"Configuration file syntax error at line %d in '%s'; %s\n",parse_line,parse_name,errmsg); /* Used in wwwoffle.c */
    free(errmsg);
    errmsg=newerrmsg;
   }

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Dump the contents of the configuration file

  int fd The file descriptor to write to.
  ++++++++++++++++++++++++++++++++++++++*/

void DumpConfigFile(int fd)
{
 int s,i,e;

 write_string(fd,"# WWWOFFLE CURRENT CONFIGURATION\n");

 for(s=0;s<CurrentConfig.nsections;s++)
   {
    write_formatted(fd,"\n%s\n{\n",CurrentConfig.sections[s]->name);

    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;i++)
      {
       if(*CurrentConfig.sections[s]->itemdefs[i].name)
          write_formatted(fd,"# Item %s\n",CurrentConfig.sections[s]->itemdefs[i].name);
       else
          write_string(fd,"# Item [default]\n");

       if(*CurrentConfig.sections[s]->itemdefs[i].item)
          for(e=0;e<(*CurrentConfig.sections[s]->itemdefs[i].item)->nentries;e++)
            {
             char *string=ConfigEntryString(*CurrentConfig.sections[s]->itemdefs[i].item,e);
             write_formatted(fd,"    %s\n",string);
             free(string);
            }
      }

    write_string(fd,"}\n");
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Modify an entry in the configuration file.

  char *ModifyConfigFile Returns a string detailing the error if there is one.

  int section The section of the configuration file to modify.

  int item The item of the configuration to modify.

  char *newentry The new entry to insert or change to (or NULL if deleting one).

  char *preventry The previous entry in the current list (or NULL if not adding after one).

  char *sameentry The same entry in the current list (or NULL if not replacing one).

  char *nextentry The next entry in the current list (or NULL if not adding before one).
  ++++++++++++++++++++++++++++++++++++++*/

char *ModifyConfigFile(int section,int item,char *newentry,char *preventry,char *sameentry,char *nextentry)
{
 char *errmsg=NULL;
 char **names=(char**)calloc((size_t)(1+CurrentConfig.nsections),sizeof(char*));
 int file=-1,file_org=-1;
 ConfigItem dummy=NULL;
 int matched=0;
 int s,rename_failed=0;

 /* Initialise the parser and open the new file. */

 errmsg=InitParser();

 if(!errmsg)
   {
    names[0]=filename_or_symlink_target(parse_name);

    strcat(names[0],".new");

    file=open(names[0],O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0600);

    if(file==-1)
      {
       errmsg=(char*)malloc(64+strlen(names[0]));
       sprintf(errmsg,"Cannot open the configuration file '%s' for writing.",names[0]);
      }
    else
       init_io(file);
   }

 /* Parse the file */

 if(!errmsg)
   {
    char *line=NULL;

    do
      {
       if((errmsg=ParseLine(&line)))
          break;

       if(parse_section==section && line)
         {
          char *url_str,*key_str,*val_str;
          char *copy;

          /* Insert a new entry for a non-existing item. */

          if(newentry && !preventry && !sameentry && !nextentry && !matched &&
             (parse_state==InsideSectionCurly || parse_state==InsideSectionIncluded))
            {
             char *copyentry=(char*)malloc(strlen(newentry)+1);
             strcpy(copyentry,newentry);

             if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
                break;

             if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
                                   &dummy,
                                   url_str,key_str,val_str)))
                break;

             write_formatted(file,"\n# WWWOFFLE Configuration Edit Inserted: %s\n %s\n\n",RFC822Date(time(NULL),0),newentry);

             matched=1;

             free(copyentry);
            }

          copy=(char*)malloc(strlen(line)+1);
          strcpy(copy,line);

          if((errmsg=ParseItem(copy,&url_str,&key_str,&val_str)))
             break;

          if(parse_item==item)
            {
             char *thisentry;

             if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
                                   &dummy,
                                   url_str,key_str,val_str)))
                break;

             thisentry=ConfigEntryString(dummy,dummy->nentries-1);

             /* Insert a new entry before the current one */

             if(newentry && nextentry && !strcmp(thisentry,nextentry))
               {
                char *copyentry=(char*)malloc(strlen(newentry)+1);
                strcpy(copyentry,newentry);

                if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
                   break;

                if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
                                      &dummy,
                                      url_str,key_str,val_str)))
                   break;

                write_formatted(file,"\n# WWWOFFLE Configuration Edit Inserted: %s\n %s\n\n",RFC822Date(time(NULL),0),newentry);
                write_string(file,line);

                matched=1;

                free(copyentry);
               }

             /* Insert a new entry after the current one */

             else if(newentry && preventry && !strcmp(thisentry,preventry))
               {
                char *copyentry=(char*)malloc(strlen(newentry)+1);
                strcpy(copyentry,newentry);

                if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
                   break;

                if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
                                      &dummy,
                                      url_str,key_str,val_str)))
                   break;

                write_string(file,line);
                write_formatted(file,"\n# WWWOFFLE Configuration Edit Inserted: %s\n %s\n\n",RFC822Date(time(NULL),0),newentry);

                matched=1;

                free(copyentry);
               }

             /* Delete an entry */

             else if(!newentry && sameentry && !strcmp(thisentry,sameentry))
               {
                write_formatted(file,"\n# WWWOFFLE Configuration Edit Deleted: %s\n#%s",RFC822Date(time(NULL),0),line);

                matched=1;
               }

             /* Change an entry */

             else if(newentry && sameentry && !strcmp(thisentry,sameentry))
               {
                char *copyentry=(char*)malloc(strlen(newentry)+1);
                strcpy(copyentry,newentry);

                if(CurrentConfig.sections[section]->itemdefs[item].same_key==0 &&
                   CurrentConfig.sections[section]->itemdefs[item].url_type==0)
                  {
                   FreeConfigItem(dummy);
                   dummy=NULL;
                  }

                if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
                   break;

                if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
                                      &dummy,
                                      url_str,key_str,val_str)))
                   break;

                write_formatted(file,"# WWWOFFLE Configuration Edit Changed: %s\n#%s",RFC822Date(time(NULL),0),line);
                write_formatted(file," %s\n",newentry);

                matched=1;

                free(copyentry);
               }
             else
                write_string(file,line);

             free(thisentry);
            }
          else
             write_string(file,line);

          free(copy);
         }
       else if(line)
          write_string(file,line);

       if(parse_state==StartSectionIncluded && file_org==-1)
         {
          names[parse_section+1]=filename_or_symlink_target(parse_name);
          strcat(names[parse_section+1],".new");

          file_org=file;
          file=open(names[parse_section+1],O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0600);

          if(file==-1)
            {
             errmsg=(char*)malloc(48+strlen(names[parse_section+1]));
             sprintf(errmsg,"Cannot open the included file '%s' for writing.",names[parse_section+1]);
             break;
            }

          init_io(file);
         }
       else if(parse_state==StartSectionSquare && file_org!=-1)
         {
          finish_io(file);
          close(file);
          file=file_org;
          file_org=-1;
         }
      }
    while(parse_state!=Finished);

    if(line)
       free(line);
   }

 if(!errmsg && !matched && (preventry || sameentry || nextentry))
   {
    char *whichentry=sameentry?sameentry:preventry?preventry:nextentry;

    errmsg=(char*)malloc(64+strlen(whichentry));
    sprintf(errmsg,"No entry to match '%s' was found to make the change.",whichentry);
   }

 if(file!=-1)
   {
    finish_io(file);
    close(file);
   }
 if(file_org!=-1)
   {
    finish_io(file_org);
    close(file_org);
   }
 if(parse_file!=-1)
   {
    finish_io(parse_file);
    close(parse_file);
   }
 if(parse_file_org!=-1)
   {
    finish_io(parse_file_org);
    close(parse_file_org);
   }

 for(s=0;s<=CurrentConfig.nsections;s++)
    if(names[s])
      {
       if(!errmsg)
         {
          struct stat buf;
          char *name=(char*)malloc(strlen(names[s])+1);
          char *name_bak=(char*)malloc(strlen(names[s])+16);

          strcpy(name,names[s]);
          name[strlen(name)-4]=0;

          strcpy(name_bak,names[s]);
          strcpy(name_bak+strlen(name_bak)-3,"bak");

          if(!stat(name,&buf))
            {
             chown(names[s],buf.st_uid,buf.st_gid);
             chmod(names[s],buf.st_mode&(~S_IFMT));
            }

          if(rename(name,name_bak))
            {
             rename_failed++;
             PrintMessage(Warning,"Cannot rename '%s' to '%s' when modifying configuration entry [%!s].",name,name_bak);
            }
          if(rename(names[s],name))
            {
             rename_failed++;
             PrintMessage(Warning,"Cannot rename '%s' to '%s' when modifying configuration entry [%!s].",name[s],name);
            }

          free(name);
          free(name_bak);
         }
       else
          unlink(names[s]);

       free(names[s]);
      }

 if(rename_failed)
   {
    errmsg=(char*)malloc((size_t)120);
    strcpy(errmsg,"There were problems renaming files, check the error log (this might stop the change you tried to make).");
   }

 FreeConfigItem(dummy);

 free(names);

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Find the real filename of a potential symbolic link.

  char *filename_or_symlink_target Returns the real file name.

  const char *name The file name that may be a symbolic link.
  ++++++++++++++++++++++++++++++++++++++*/

static char *filename_or_symlink_target(const char *name)
{
 struct stat buf;
 char linkname[PATH_MAX+1];
 char *result=NULL;

 if(!stat(name,&buf) && buf.st_mode&&S_IFLNK)
   {
    int linklen=0;

    if((linklen=readlink(name,linkname,(size_t)PATH_MAX))!=-1)
      {
       linkname[linklen]=0;

       if(*linkname=='/')
         {
          result=(char*)malloc(linklen+8);
          strcpy(result,linkname);
         }
       else
         {
          char *p;
          result=(char*)malloc(strlen(name)+linklen+8);
          strcpy(result,name);
          p=result+strlen(result)-1;
          while(p>=result && *p!='/')
             p--;
          strcpy(p+1,linkname);
          CanonicaliseName(result);
         }
      }
   }

 if(!result)
   {
    result=(char*)malloc(strlen(name)+8);
    strcpy(result,name);
   }

 return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Initialise the file parser.

  char *InitParser Return an error string in case of error.
  ++++++++++++++++++++++++++++++++++++++*/

static char *InitParser(void)
{
 parse_name=CurrentConfig.name;
 parse_file=open(parse_name,O_RDONLY|O_BINARY);
 parse_line=0;

 parse_name_org=NULL;
 parse_file_org=-1;
 parse_line_org=0;

 parse_section=-1;
 parse_item=-1;
 parse_state=OutsideSection;

 if(parse_file==-1)
   {
    char *errmsg=(char*)malloc(64+strlen(parse_name));
    sprintf(errmsg,"Cannot open the configuration file '%s' for reading.",parse_name);
    return(errmsg);
   }

 init_io(parse_file);

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the current line of the configuration file.

  char *ParseLine Returns an error message if there is one.

  char **line The line just read from the file or NULL.
  ++++++++++++++++++++++++++++++++++++++*/

static char *ParseLine(char **line)
{
 char *errmsg=NULL;

 /* Read from the line and make a copy */

 *line=read_line(parse_file,*line);

 parse_line++;

 parse_item=-1;

 /* At the end of the file, finish, error or close included file. */

 if(!*line)
   {
    if(parse_state==OutsideSection)
       parse_state=Finished;
    else if(parse_state==StartSectionIncluded || parse_state==InsideSectionIncluded)
      {
       finish_io(parse_file);
       close(parse_file);
       free(parse_name);

       parse_file=parse_file_org;
       parse_file_org=-1;

       parse_name=parse_name_org;
       parse_name_org=NULL;

       parse_line=parse_line_org;
       parse_line_org=0;

       parse_state=StartSectionSquare;
      }
    else
      {
       errmsg=(char*)malloc((size_t)32);
       strcpy(errmsg,"Unexpected end of file.");
      }
   }
 else
   {
    char *l,*r;

    /* Trim the line. */

    l=*line;
    r=*line+strlen(*line)-1;

    while(isspace(*l))
       l++;

    while(r>l && isspace(*r))
       r--;
    r++;

    /* Outside of section, searching for a section. */

    if(parse_state==OutsideSection)
      {
       if(*l!='#' && *l!=0)
         {
          for(parse_section=0;parse_section<CurrentConfig.nsections;parse_section++)
             if(!strncmp(CurrentConfig.sections[parse_section]->name,l,r-l))
               {
                parse_state=StartSection;
                break;
               }

          if(parse_section==CurrentConfig.nsections)
            {
             errmsg=(char*)malloc(64+strlen(l));
             sprintf(errmsg,"Unrecognised text outside of section (not section label) '%s'.",l);
             parse_section=-1;
            }
         }
      }

    /* The start of a section, undecided which type. */

    else if(parse_state==StartSection)
      {
       if(*l=='{' && (l+1)==r)
         {
          parse_state=StartSectionCurly;
         }
       else if(*l=='[' && (l+1)==r)
         {
          parse_state=StartSectionSquare;
         }
       else if(*l!='{' && *l!='[')
         {
          errmsg=(char*)malloc((size_t)48);
          strcpy(errmsg,"Start of section must be '{' or '['.");
         }
       else
         {
          errmsg=(char*)malloc((size_t)48);
          sprintf(errmsg,"Start of section '%c' has trailing junk.",*l);
         }
      }

    /* Inside a normal '{...}' section. */

    else if(parse_state==StartSectionCurly || parse_state==InsideSectionCurly)
      {
       parse_state=InsideSectionCurly;

       if(*l=='}' && (l+1)==r)
         {
          parse_state=OutsideSection;
          parse_section=-1;
         }
       else if(*l=='}')
         {
          errmsg=(char*)malloc((size_t)48);
          sprintf(errmsg,"End of section '%c' has trailing junk.",*l);
         }
      }

    /* Inside a include '[...]' section. */

    else if(parse_state==StartSectionSquare || parse_state==InsideSectionSquare)
      {
       parse_state=InsideSectionSquare;

       if(*l==']' && (l+1)==r)
         {
          parse_state=OutsideSection;
          parse_section=-1;
         }
       else if(*l==']')
         {
          errmsg=(char*)malloc((size_t)48);
          sprintf(errmsg,"End of section '%c' has trailing junk.",*l);
         }
       else if(*l!='#' && *l!=0 && strchr(l,'/'))
         {
          errmsg=(char*)malloc((size_t)64);
          strcpy(errmsg,"Included file must be in same directory (no '/').");
         }
       else if(*l!='#' && *l!=0)
         {
          char *rr;
          char *inc_parse_name;
          int inc_parse_file;

          inc_parse_name=(char*)malloc(strlen(CurrentConfig.name)+(r-l)+1);

          strcpy(inc_parse_name,CurrentConfig.name);

          rr=inc_parse_name+strlen(inc_parse_name)-1;
          while(rr>inc_parse_name && *rr!='/')
             rr--;

          strncpy(rr+1,l,(size_t)(r-l));
          *((rr+1)+(r-l))=0;

          inc_parse_file=open(inc_parse_name,O_RDONLY|O_BINARY);

          if(inc_parse_file==-1)
            {
             errmsg=(char*)malloc(48+strlen(inc_parse_name));
             sprintf(errmsg,"Cannot open the included file '%s' for reading.",inc_parse_name);
            }
          else
            {
             init_io(inc_parse_file);

             parse_state=StartSectionIncluded;

             parse_name_org=parse_name;
             parse_file_org=parse_file;
             parse_line_org=parse_line;

             parse_name=inc_parse_name;
             parse_file=inc_parse_file;
             parse_line=0;
            }
         }
      }

    else if(parse_state==StartSectionIncluded || parse_state==InsideSectionIncluded)
      {
       parse_state=InsideSectionIncluded;
      }
   }

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the item from the current line.

  char *ParseItem Returns an error message string in case of a problem.

  char *line The line to parse (modified by the function).

  char **url_str Returns the URL string or NULL.

  char **key_str Returns the key string or NULL.

  char **val_str Returns the value string or NULL.
  ++++++++++++++++++++++++++++++++++++++*/

static char *ParseItem(char *line,char **url_str,char **key_str,char **val_str)
{
 *url_str=NULL;
 *key_str=NULL;
 *val_str=NULL;

 parse_item=-1;

 if(parse_state==InsideSectionCurly || parse_state==InsideSectionIncluded)
   {
    char *url=NULL,*key=NULL,*val=NULL;
    char *l,*r;

    l=line;

    while(isspace(*l))
       l++;

    if(!*l || *l=='#')
       return(NULL);

    r=line+strlen(line)-1;

    while(r>l && isspace(*r))
       *r--=0;

    if(*l=='<')
      {
       char *uu;

       uu=url=l+1;
       while(*uu && *uu!='>')
          uu++;
       if(!*uu)
         {
          char *errmsg=(char*)malloc((size_t)32);
          strcpy(errmsg,"No '>' to match the '<'.");
          return(errmsg);
         }

       *uu=0;
       key=uu+1;
       while(*key && isspace(*key))
          key++;
       if(!*key)
         {
          char *errmsg=(char*)malloc((size_t)48);
          strcpy(errmsg,"No configuration entry following the '<...>'.");
          return(errmsg);
         }
      }
    else
       key=l;

    for(parse_item=0;parse_item<CurrentConfig.sections[parse_section]->nitemdefs;parse_item++)
       if(!*CurrentConfig.sections[parse_section]->itemdefs[parse_item].name ||
          !strncmp(CurrentConfig.sections[parse_section]->itemdefs[parse_item].name,key,strlen(CurrentConfig.sections[parse_section]->itemdefs[parse_item].name)))
         {
          char *ll;

          if(*CurrentConfig.sections[parse_section]->itemdefs[parse_item].name)
            {
             ll=key+strlen(CurrentConfig.sections[parse_section]->itemdefs[parse_item].name);

             if(*ll && *ll!='=' && !isspace(*ll))
                continue;
            }
          else if(CurrentConfig.sections[parse_section]->itemdefs[parse_item].key_type==UrlSpecification)
            {
             char *equal;

             ll=key;

             while((equal=strchr(ll,'=')))
               {
                ll=equal;
                if(--equal>key && isspace(*equal))
                  {
                   while(isspace(*equal))
                      *equal--=0;
                   break;
                  }
                ll++;
               }

             while(*ll && *ll!='=' && !isspace(*ll))
                ll++;
            }
          else
            {
             ll=key;
             while(*ll && *ll!='=' && !isspace(*ll))
                ll++;
            }

          if(CurrentConfig.sections[parse_section]->itemdefs[parse_item].url_type==0 && url)
            {
             char *errmsg=(char*)malloc((size_t)48);
             strcpy(errmsg,"No URL context '<...>' allowed for this entry.");
             return(errmsg);
            }

          if(CurrentConfig.sections[parse_section]->itemdefs[parse_item].val_type==None)
            {
             if(strchr(ll,'='))
               {
                char *errmsg=(char*)malloc((size_t)40);
                strcpy(errmsg,"Equal sign seen but not expected.");
                return(errmsg);
               }

             *ll=0;
             val=NULL;
            }
          else
            {
             val=strchr(ll,'=');
             if(!val)
               {
                char *errmsg=(char*)malloc((size_t)40);
                strcpy(errmsg,"No equal sign seen but expected.");
                return(errmsg);
               }

             *ll=0;
             if(!*key)
               {
                char *errmsg=(char*)malloc((size_t)48);
                strcpy(errmsg,"Nothing to the left of the equal sign.");
                return(errmsg);
               }

             val++;
             while(isspace(*val))
                val++;
            }

          *url_str=url;
          *key_str=key;
          *val_str=val;

          break;
         }

    if(parse_item==CurrentConfig.sections[parse_section]->nitemdefs)
      {
       char *errmsg=(char*)malloc(32+strlen(l));
       sprintf(errmsg,"Unrecognised entry '%s'.",l);
       return(errmsg);
      }
   }

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse an entry from the file.

  char *ParseEntry Return a string containing an error message in case of error.

  const ConfigItemDef *itemdef The item definition for the item in the section.

  ConfigItem *item The item to add the entry to.

  const char *url_str The string for the URL specification.

  const char *key_str The string for the key.

  const char *val_str The string to the value.
  ++++++++++++++++++++++++++++++++++++++*/

static char *ParseEntry(const ConfigItemDef *itemdef,ConfigItem *item,const char *url_str,const char *key_str,const char *val_str)
{
 UrlSpec *url=NULL;
 KeyOrValue key,val;
 char *errmsg=NULL;

 key.string=NULL;
 val.string=NULL;

 if(itemdef->same_key==0 && itemdef->url_type==0 && (*item) && (*item)->nentries)
   {
    errmsg=(char*)malloc(32+strlen(key_str));
    sprintf(errmsg,"Duplicated entry: '%s'.",key_str);
    return(errmsg);
   }

 if(!itemdef->url_type || !url_str)
    url=NULL;
 else
    if((errmsg=ParseKeyOrValue(url_str,UrlSpecification,(KeyOrValue*)&url)))
       return(errmsg);

 if(itemdef->key_type==Fixed)
   {
    if(strcmp(key_str,itemdef->name))
      {
       errmsg=(char*)malloc(32+strlen(key_str));
       sprintf(errmsg,"Unexpected key string: '%s'.",key_str);
       if(url) free(url);
       return(errmsg);
      }
    key.string=itemdef->name;
   }
 else
    if((errmsg=ParseKeyOrValue(key_str,itemdef->key_type,&key)))
      {
       if(url) free(url);
       return(errmsg);
      }

 if(!val_str)
    val.string=NULL;
 else if(itemdef->val_type==None)
    val.string=NULL;
 else
    if((errmsg=ParseKeyOrValue(val_str,itemdef->val_type,&val)))
      {
       if(url) free(url);
       FreeKeyOrValue(&key,itemdef->key_type);
       return(errmsg);
      }

 if(!item)
   {
    if(url) free(url);
    FreeKeyOrValue(&key,itemdef->key_type);
    FreeKeyOrValue(&val,itemdef->val_type);
    return(NULL);
   }

 if(!(*item))
   {
    *item=(ConfigItem)malloc(sizeof(struct _ConfigItem));
    (*item)->itemdef=itemdef;
    (*item)->nentries=0;
    (*item)->url=NULL;
    (*item)->key=NULL;
    (*item)->val=NULL;
    (*item)->def_val=NULL;
   }
 if(!(*item)->nentries)
   {
    (*item)->nentries=1;
    if(itemdef->url_type!=0)
       (*item)->url=(UrlSpec**)malloc(sizeof(UrlSpec*));
    (*item)->key=(KeyOrValue*)malloc(sizeof(KeyOrValue));
    if(itemdef->val_type!=None)
       (*item)->val=(KeyOrValue*)malloc(sizeof(KeyOrValue));
   }
 else
   {
    (*item)->nentries++;
    if(itemdef->url_type!=0)
       (*item)->url=(UrlSpec**)realloc((void*)(*item)->url,(*item)->nentries*sizeof(UrlSpec*));
    (*item)->key=(KeyOrValue*)realloc((void*)(*item)->key,(*item)->nentries*sizeof(KeyOrValue));
    if(itemdef->val_type!=None)
       (*item)->val=(KeyOrValue*)realloc((void*)(*item)->val,(*item)->nentries*sizeof(KeyOrValue));
   }

 if(itemdef->url_type!=0)
    (*item)->url[(*item)->nentries-1]=url;
 (*item)->key[(*item)->nentries-1]=key;
 if(itemdef->val_type!=None)
    (*item)->val[(*item)->nentries-1]=val;

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the text and put a value into the location.

  char *ParseKeyOrValue Returns a string containing the error message.

  const char *text The text string to parse.

  ConfigType type The type we are looking for.

  KeyOrValue *pointer The location to store the key or value.
  ++++++++++++++++++++++++++++++++++++++*/

char *ParseKeyOrValue(const char *text,ConfigType type,KeyOrValue *pointer)
{
 char *errmsg=NULL;

 switch(type)
   {
   case Fixed:
   case None:
    break;

   case CfgMaxServers:
    if(!*text)
      {errmsg=(char*)malloc((size_t)56);strcpy(errmsg,"Expecting a maximum server count, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a maximum server count, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<=0 || pointer->integer>MAX_SERVERS)
         {errmsg=(char*)malloc((size_t)(36+MAX_INT_STR));sprintf(errmsg,"Invalid maximum server count: %d.",pointer->integer);}
      }
    break;

   case CfgMaxFetchServers:
    if(!*text)
      {errmsg=(char*)malloc((size_t)56);strcpy(errmsg,"Expecting a maximum fetch server count, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(56+strlen(text));sprintf(errmsg,"Expecting a maximum fetch server count, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<=0 || pointer->integer>MAX_FETCH_SERVERS)
         {errmsg=(char*)malloc((size_t)(40+MAX_INT_STR));sprintf(errmsg,"Invalid maximum fetch server count: %d.",pointer->integer);}
      }
    break;

   case CfgLogLevel:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a log level, got nothing.");}
    else if(strcasecmp(text,"debug")==0)
       pointer->integer=Debug;
    else if(strcasecmp(text,"info")==0)
       pointer->integer=Inform;
    else if(strcasecmp(text,"important")==0)
       pointer->integer=Important;
    else if(strcasecmp(text,"warning")==0)
       pointer->integer=Warning;
    else if(strcasecmp(text,"fatal")==0)
       pointer->integer=Fatal;
    else
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a log level, got '%s'.",text);}
    break;

   case Boolean:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a Boolean, got nothing.");}
    else if(!strcasecmp(text,"yes") || !strcasecmp(text,"true"))
       pointer->integer=1;
    else if(!strcasecmp(text,"no") || !strcasecmp(text,"false"))
       pointer->integer=0;
    else
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a Boolean, got '%s'.",text);}
    break;

   case PortNumber:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a port number, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a port number, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<=0 || pointer->integer>65535)
         {errmsg=(char*)malloc((size_t)(24+MAX_INT_STR));sprintf(errmsg,"Invalid port number %d.",pointer->integer);}
      }
    break;

   case AgeDays:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting an age in days, got nothing.");}
    else if(isanumber(text))
       pointer->integer=atoi(text);
    else
      {
       int val,len;
       char suffix;

       if(sscanf(text,"%d%1c%n",&val,&suffix,&len)==2 && len==strlen(text) &&
          (suffix=='d' || suffix=='w' || suffix=='m' || suffix=='y'))
         {
          if(suffix=='y')
             pointer->integer=val*365;
          else if(suffix=='m')
             pointer->integer=val*30;
          else if(suffix=='w')
             pointer->integer=val*7;
          else /* if(suffix=='d') */
             pointer->integer=val;
         }
       else
         {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting an age in days, got '%s'.",text);}
      }
    break;

   case TimeSecs:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a time in seconds, got nothing.");}
    else if(isanumber(text))
       pointer->integer=atoi(text);
    else
      {
       int val,len;
       char suffix;

       if(sscanf(text,"%d%1c%n",&val,&suffix,&len)==2 && len==strlen(text) &&
          (suffix=='s' || suffix=='m' || suffix=='h' || suffix=='d' || suffix=='w'))
         {
          if(suffix=='w')
             pointer->integer=val*3600*24*7;
          else if(suffix=='d')
             pointer->integer=val*3600*24;
          else if(suffix=='h')
             pointer->integer=val*3600;
          else if(suffix=='m')
             pointer->integer=val*60;
          else /* if(suffix=='s') */
             pointer->integer=val;
         }
       else
         {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a time in seconds, got '%s'.",text);}
      }
    break;

   case CacheSize:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a cache size in MB, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a cache size in MB, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<-1)
         {errmsg=(char*)malloc((size_t)(24+MAX_INT_STR));sprintf(errmsg,"Invalid cache size %d.",pointer->integer);}
      }
    break;

   case FileSize:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a file size in kB, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a file size in kB, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<0)
         {errmsg=(char*)malloc((size_t)(24+MAX_INT_STR));sprintf(errmsg,"Invalid file size %d.",pointer->integer);}
      }
    break;

   case Percentage:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a percentage, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a percentage, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<0 || pointer->integer>100)
         {errmsg=(char*)malloc((size_t)(24+MAX_INT_STR));sprintf(errmsg,"Invalid percentage %d.",pointer->integer);}
      }
    break;

   case UserId:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a username or uid, got nothing.");}
    else
      {
       uid_t uid;
       struct passwd *userInfo=getpwnam(text);
       if(userInfo)
          uid=userInfo->pw_uid;
       else
         {
          if(sscanf(text,"%d",&uid)!=1)
            {errmsg=(char*)malloc(24+strlen(text));sprintf(errmsg,"Invalid user %s.",text);}
          else if(uid!=-1 && !getpwuid(uid))
            {errmsg=(char*)malloc((size_t)(24+MAX_INT_STR));sprintf(errmsg,"Unknown user id %d.",uid);}
         }
       pointer->integer=uid;
      }
    break;

   case GroupId:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a group name or gid, got nothing.");}
    else
      {
       gid_t gid;
       struct group *groupInfo=getgrnam(text);
       if(groupInfo)
          gid=groupInfo->gr_gid;
       else
         {
          if(sscanf(text,"%d",&gid)!=1)
            {errmsg=(char*)malloc(24+strlen(text));sprintf(errmsg,"Invalid group %s.",text);}
          else if(gid!=-1 && !getgrgid(gid))
            {errmsg=(char*)malloc((size_t)(24+MAX_INT_STR));sprintf(errmsg,"Unknown group id %d.",gid);}
         }
       pointer->integer=gid;
      }
    break;

   case String:
    if(!*text || !strcasecmp(text,"none"))
       pointer->string=NULL;
    else
      {
       pointer->string=(char*)malloc(strlen(text)+1);
       strcpy(pointer->string,text);
      }
    break;

   case PathName:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a pathname, got nothing.");}
    else if(*text!='/')
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting an absolute pathname, got '%s'.",text);}
    else
      {
       pointer->string=(char*)malloc(strlen(text)+1);
       strcpy(pointer->string,text);
      }
    break;

   case FileExt:
    if(!*text)
      {errmsg=(char*)malloc((size_t)40);strcpy(errmsg,"Expecting a file extension, got nothing.");}
    else if(*text!='.')
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a file extension, got '%s'.",text);}
    else
      {
       pointer->string=(char*)malloc(strlen(text)+1);
       strcpy(pointer->string,text);
      }
    break;

   case FileMode:
    if(!*text)
      {errmsg=(char*)malloc((size_t)40);strcpy(errmsg,"Expecting a file permissions mode, got nothing.");}
    else if(!isanumber(text) || *text!='0')
      {errmsg=(char*)malloc(56+strlen(text));sprintf(errmsg,"Expecting an octal file permissions mode, got '%s'.",text);}
    else
      {
       sscanf(text,"%o",(unsigned *)&pointer->integer);
       pointer->integer&=07777;
      }
    break;

   case MIMEType:
     if(!*text)
       {errmsg=(char*)malloc((size_t)40);strcpy(errmsg,"Expecting a MIME Type, got nothing.");}
     else
       {
        char *slash=strchr(text,'/');
        if(!slash)
          {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a MIME Type/Subtype, got '%s'.",text);}
        pointer->string=(char*)malloc(strlen(text)+1);
        strcpy(pointer->string,text);
       }
    break;

   case HostOrNone:
    if(!*text || !strcasecmp(text,"none"))
      {
       pointer->string=NULL;
       break;
      }

    /*@fallthrough@*/

   case Host:
    if(!*text)
      {errmsg=(char*)malloc((size_t)40);strcpy(errmsg,"Expecting a hostname, got nothing.");}
    else
      {
       char *host,*colon;

       if(strchr(text,'*'))
         {errmsg=(char*)malloc(56+strlen(text));sprintf(errmsg,"Expecting a hostname without a wildcard, got '%s'.",text); break;}

       host=CanonicaliseHost(text);

       if(*host=='[')
         {
          char *square=strchr(host,']');
          colon=strchr(square,':');
         }
       else
          colon=strchr(host,':');

       if(colon)
         {errmsg=(char*)malloc(56+strlen(text));sprintf(errmsg,"Expecting a hostname without a port number, got '%s'.",text); free(host); break;}

       pointer->string=host;
      }
    break;

   case HostAndPortOrNone:
    if(!*text || !strcasecmp(text,"none"))
      {
       pointer->string=NULL;
       break;
      }

    /*@fallthrough@*/

   case HostAndPort:
    if(!*text)
      {errmsg=(char*)malloc((size_t)56);strcpy(errmsg,"Expecting a hostname and port number, got nothing.");}
    else
      {
       char *host,*colon;

       if(strchr(text,'*'))
         {errmsg=(char*)malloc(72+strlen(text));sprintf(errmsg,"Expecting a hostname and port number, without a wildcard, got '%s'.",text); break;}

       host=CanonicaliseHost(text);

       if(*host=='[')
         {
          char *square=strchr(host,']');
          colon=strchr(square,':');
         }
       else
          colon=strchr(host,':');

       if(!colon)
         {errmsg=(char*)malloc(56+strlen(text));sprintf(errmsg,"Expecting a hostname and port number, got '%s'.",text); free(host); break;}

       if(colon && (!isanumber(colon+1) || atoi(colon+1)<=0 || atoi(colon+1)>65535))
         {errmsg=(char*)malloc(32+strlen(colon+1));sprintf(errmsg,"Invalid port number %s.",colon+1); free(host); break;}

       pointer->string=host;
      }
    break;

   case HostWild:
    if(!*text)
      {errmsg=(char*)malloc((size_t)64);strcpy(errmsg,"Expecting a hostname (perhaps with wildcard), got nothing.");}
    else
      {
       const char *p;
       char *host;
       int wildcard=0,colons=0;

       p=text;
       while(*p)
         {
          if(*p=='*')
             wildcard++;
          else if(*p==':')
             colons++;
          p++;
         }

       /*
         This is tricky, we should check for a host:port combination and disallow it.
         But the problem is that a single colon could be an IPv6 wildcard or an IPv4 host and port.
         If there are 2 or more ':' then it is IPv6, if it starts with '[' it is IPv6.
       */

       if(wildcard)
         {
          char *p;
          host=(char*)malloc(strlen(text)+1);
          strcpy(host,text);
          for(p=host;*p;p++)
             *p=tolower(*p);
         }
       else
          host=CanonicaliseHost(text);

       if(colons==1 && *host!='[')
         {errmsg=(char*)malloc(80+strlen(text));sprintf(errmsg,"Expecting a hostname without a port number (perhaps with wildcard), got '%s'.",text); free(host); break;}

       if(*host=='[')
         {
          char *square=strchr(host,']');
          if(!square || *(square+1))
            {errmsg=(char*)malloc(80+strlen(text));sprintf(errmsg,"Expecting a hostname without a port number (perhaps with wildcard), got '%s'.",text); free(host); break;}
         }

       pointer->string=host;
      }
     break;

   case HostAndPortWild:
    if(!*text)
      {errmsg=(char*)malloc((size_t)80);strcpy(errmsg,"Expecting a hostname and port number (perhaps with wildcard), got nothing.");}
    else
      {
       const char *p;
       char *host;
       int wildcard=0,colons=0;

       p=text;
       while(*p)
         {
          if(*p=='*')
             wildcard++;
          else if(*p==':')
             colons++;
          p++;
         }

       /*
         This is tricky, we should check for a host:port combination and allow it.
         But the problem is that a single colon could be an IPv6 wildcard or an IPv4 host and port.
         If there are 2 or more ':' then it is IPv6, if it starts with '[' it is IPv6.
       */

       if(wildcard)
         {
          char *p;
          host=(char*)malloc(strlen(text)+1);
          strcpy(host,text);
          for(p=host;*p;p++)
             *p=tolower(*p);
         }
       else
          host=CanonicaliseHost(text);

       if(colons==0 || (colons>1 && *host!='['))
         {errmsg=(char*)malloc(80+strlen(text));sprintf(errmsg,"Expecting a hostname and a port number (perhaps with wildcard), got '%s'.",text); free(host); break;}

       if(*host=='[')
         {
          char *square=strchr(host,']');
          if(!square || *(square+1)!=':')
            {errmsg=(char*)malloc(80+strlen(text));sprintf(errmsg,"Expecting a hostname and a port number (perhaps with wildcard), got '%s'.",text); free(host); break;}
         }

       pointer->string=host;
      }
     break;

   case UserPass:
    if(!*text)
      {errmsg=(char*)malloc((size_t)48);strcpy(errmsg,"Expecting a username and password, got nothing.");}
    else if(!strchr(text,':'))
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a username and password, got '%s'.",text);}
    else
       pointer->string=Base64Encode(text,strlen(text));
    break;

   case Url:
    if(!*text || !strcasecmp(text,"none"))
      {errmsg=(char*)malloc((size_t)32);strcpy(errmsg,"Expecting a URL, got nothing.");}
    else
      {
       URL *tempUrl;

       if(strchr(text,'*'))
         {errmsg=(char*)malloc(56+strlen(text));sprintf(errmsg,"Expecting a URL without a wildcard, got '%s'.",text); break;}

       tempUrl=SplitURL(text);
       if(!IsProtocolHandled(tempUrl))
         {errmsg=(char*)malloc(32+strlen(text));sprintf(errmsg,"Expecting a URL, got '%s'.",text);}
       else
         {
          pointer->string=(char*)malloc(strlen(tempUrl->file)+1);
          strcpy(pointer->string,tempUrl->file);
         }
       FreeURL(tempUrl);
      }
    break;

   case UrlWild:
    if(!*text || !strcasecmp(text,"none"))
      {errmsg=(char*)malloc((size_t)64);strcpy(errmsg,"Expecting a URL (perhaps with wildcard), got nothing.");}
    else
      {
       URL *tempUrl=SplitURL(text);
       if(!IsProtocolHandled(tempUrl))
         {errmsg=(char*)malloc(32+strlen(text));sprintf(errmsg,"Expecting a URL, got '%s'.",text);}
       else
         {
          pointer->string=(char*)malloc(strlen(text)+1);
          strcpy(pointer->string,text);
         }
       FreeURL(tempUrl);
      }
    break;

   case UrlSpecification:
    if(!*text)
      {errmsg=(char*)malloc((size_t)64);strcpy(errmsg,"Expecting a URL-SPECIFICATION, got nothing.");}
    else
      {
       const char *p,*orgtext=text;

       pointer->urlspec=(UrlSpec*)malloc(sizeof(UrlSpec));
       pointer->urlspec->null=0;
       pointer->urlspec->negated=0;
       pointer->urlspec->nocase=0;
       pointer->urlspec->proto=0;
       pointer->urlspec->host=0;
       pointer->urlspec->port=-1;
       pointer->urlspec->path=0;
       pointer->urlspec->args=0;

       /* !~ */

       while(*text)
         {
          if(*text=='!')
             pointer->urlspec->negated=1;
          else if(*text=='~')
             pointer->urlspec->nocase=1;
          else
             break;

          text++;
         }

       /* protocol */

       if(!strncmp(text,"*://",(size_t)4))
          p=text+4;
       else if(!strncmp(text,"://",(size_t)3))
          p=text+3;
       else if((p=strstr(text,"://")))
         {
          pointer->urlspec->proto=sizeof(UrlSpec);

          pointer->urlspec=(UrlSpec*)realloc((void*)pointer->urlspec,
                                             pointer->urlspec->proto+(p-text)+1);

          strncpy(UrlSpecProto(pointer->urlspec),text,(size_t)(p-text));
          *(UrlSpecProto(pointer->urlspec)+(p-text))=0;
          p+=3;
         }
       else
         {errmsg=(char*)malloc(64+strlen(orgtext));sprintf(errmsg,"Expecting a URL-SPECIFICATION, got this '%s'.",orgtext);
          free(pointer->urlspec); break;}

       if(pointer->urlspec->proto)
         {
          char *q;

          for(q=UrlSpecProto(pointer->urlspec);*q;q++)
             *q=tolower(*q);
         }

       text=p;

       /* host */

       if(*text=='*' && (*(text+1)=='/' || !*(text+1)))
          p=text+1;
       else if(*text==':')
          p=text;
       else if(*text=='[' && (p=strchr(text,']')))
         {
          p++;
          pointer->urlspec->host=1;
         }
       else if((p=strchr(text,':')) && p<strchr(text,'/'))
         {
          pointer->urlspec->host=1;
         }
       else if((p=strchr(text,'/')))
         {
          pointer->urlspec->host=1;
         }
       else if(*text)
         {
          p=text+strlen(text);
          pointer->urlspec->host=1;
         }
       else
          p=text;

       if(pointer->urlspec->host)
         {
          char *q;

          pointer->urlspec->host=(unsigned short)(sizeof(UrlSpec)+
                                                  (pointer->urlspec->proto?1+strlen(UrlSpecProto(pointer->urlspec)):0));

          pointer->urlspec=(UrlSpec*)realloc((void*)pointer->urlspec,
                                             pointer->urlspec->host+(p-text)+1);

          strncpy(UrlSpecHost(pointer->urlspec),text,(size_t)(p-text));
          *(UrlSpecHost(pointer->urlspec)+(p-text))=0;

          for(q=UrlSpecHost(pointer->urlspec);*q;q++)
             *q=tolower(*q);
         }

       text=p;

       /* port */

       if(*text==':' && isdigit(*(text+1)))
         {
          pointer->urlspec->port=atoi(text+1);
          p=text+1;
          while(isdigit(*p))
             p++;
         }
       else if(*text==':' && (*(text+1)=='/' || *(text+1)==0))
         {
          pointer->urlspec->port=0;
          p=text+1;
         }
       else if(*text==':' && *(text+1)=='*')
         {
          p=text+2;
         }
       else if(*text==':')
         {errmsg=(char*)malloc(64+strlen(orgtext));sprintf(errmsg,"Expecting a URL-SPECIFICATION, got this '%s'.",orgtext);
          free(pointer->urlspec); break;}

       text=p;

       /* path */

       if(!*text)
          ;
       else if(*text=='?')
          ;
       else if(*text=='/' && (p=strchr(text,'?')))
         {
          if(strncmp(text,"/*?",(size_t)3))
             pointer->urlspec->path=1;
         }
       else if(*text=='/')
         {
          p=text+strlen(text);
          if(strcmp(text,"/*"))
             pointer->urlspec->path=1;
         }
       else
         {errmsg=(char*)malloc(64+strlen(orgtext));sprintf(errmsg,"Expecting a URL-SPECIFICATION, got this '%s'.",orgtext);
          free(pointer->urlspec); break;}

       if(pointer->urlspec->path)
         {
          char *temppath,*path;

          if(*p)
            {
             char *temptemppath=(char*)malloc(1+(p-text));

             strncpy(temptemppath,text,p-text);
             temptemppath[p-text]=0;

             temppath=URLDecodeGeneric(temptemppath);

             free(temptemppath);
            }
          else
             temppath=URLDecodeGeneric(text);

          path=URLEncodePath(temppath);
          free(temppath);

          pointer->urlspec->path=(unsigned short)(sizeof(UrlSpec)+
                                                  (pointer->urlspec->proto?1+strlen(UrlSpecProto(pointer->urlspec)):0)+
                                                  (pointer->urlspec->host ?1+strlen(UrlSpecHost (pointer->urlspec)):0));

          pointer->urlspec=(UrlSpec*)realloc((void*)pointer->urlspec,
                                             pointer->urlspec->path+strlen(path)+1);

          strcpy(UrlSpecPath(pointer->urlspec),path);

          free(path);
         }

       text=p;

       /* args */

       if(!*text)
          ;
       else if(*text=='?')
         {
          p=text+1;

          pointer->urlspec->args=1;
         }
       else
         {errmsg=(char*)malloc(64+strlen(orgtext));sprintf(errmsg,"Expecting a URL-SPECIFICATION, got this '%s'.",orgtext);
          free(pointer->urlspec); break;}

       if(pointer->urlspec->args)
         {
          char *args=NULL;

          if(*p)
             args=URLRecodeFormArgs(p);
          else
             args="";

          pointer->urlspec->args=(unsigned short)(sizeof(UrlSpec)+
                                                  (pointer->urlspec->proto?1+strlen(UrlSpecProto(pointer->urlspec)):0)+
                                                  (pointer->urlspec->host ?1+strlen(UrlSpecHost (pointer->urlspec)):0)+
                                                  (pointer->urlspec->path ?1+strlen(UrlSpecPath (pointer->urlspec)):0));

          pointer->urlspec=(UrlSpec*)realloc((void*)pointer->urlspec,
                                             pointer->urlspec->args+strlen(args)+1);

          strcpy(UrlSpecArgs(pointer->urlspec),args);

          if(*args)
             free(args);
         }
      }
    break;
   }

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if a string is an integer.

  int isanumber Returns 1 if it is, 0 if not.

  const char *string The string that may be an integer.
  ++++++++++++++++++++++++++++++++++++++*/

static int isanumber(const char *string)
{
 int i=0;

 if(string[i]=='-' || string[i]=='+')
    i++;

 for(;string[i];i++)
    if(!isdigit(string[i]))
       return(0);

 return(1);
}
