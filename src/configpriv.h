/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/configpriv.h,v 1.27 2006-01-07 16:10:38 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Configuration file management functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef CONFIGPRIV_H
#define CONFIGPRIV_H    /*+ To stop multiple inclusions. +*/


/*          How the Configuration data structures relate to each other
 *          ==========================================================
 *
 *  Static Part
 *  -----------      -> char[]                                 -> char[]
 *                  /   ? B; static                           /   ? B; static
 * ConfigSection   /                         ConfigItemDef   /
 * +-----------+  /                          +-----------+  / 
 * | name      | -     ConfigItemDef[]   --> | name      | -       ConfigItem
 * +-----------+        +-----------+   /    +-----------+        +-----------+
 * | nitemdefs |    --> | item[0]   | --|    | item      | -----> |           |
 * +-----------+   /    +-----------+   |    +-----------+        +-----------+
 * | itemdef   | --     | item[1]   |   |    | url_type  |         4 B; static
 * +-----------+        +-----------+   |    +-----------+
 * 12 B; static         :           :   |    | same_key  |
 *                      +-----------+   |    +-----------+
 *                      | item[n-1] |   |    | key_type  |
 *                      +-----------+   |    +-----------+
 *                      4N B; static    |    | val_type  |
 *                                      |    +-----------+
 *                                      |    | def_val   |
 *                                      |    +-----------+
 *                                      |    28 B; static
 *   Dynamic Part                       |
 *   ------------                       |
 *                                      |                                                UrlSpec      opt.
 *                                      |                              optional        +-----------+ - - -> char[]
 *                                      |                        - - - - - - - - - - > +-----------+ - - -> char[]
 *                                      |                       /                      :           : - - -> char[]
 *                                      |                                              +-----------+ - - -> char[]
 *                        struct        |                     /                        +-----------+        ? B; realloc
 *  ConfigItem           _ConfigItem    |      *UrlSpec[]                              16 B; malloc              UrlSpec
 * +-----------+  opt.  +-----------+  /     +-----------+  /
 * |           | - - -> | itemdef   | -  t-> | url[0]    | -      KeyOrValue[]
 * +-----------+        +-----------+   p/   +-----------+        +-----------+  opt.    UrlSpec      opt.
 *  4 B; static         | nentries  |  o     :           :     -> | key[0]    | - - -> +-----------+ - - -> char[]
 *                      +-----------+  /     +-----------+    /   +-----------+        +-----------+ - - -> char[]
 *                      | url       | -      | url[n-1]  |   /    :           :        :           : - - -> char[]
 *                      +-----------+        +-----------+  /     +-----------+        +-----------+ - - -> char[]
 *                      | key       | ----.  4N B; malloc  /      | key[2]    |        +-----------+        ? B; realloc
 *                      +-----------+      `---------------       +-----------+        16 B; malloc              UrlSpec
 *                      | val       | - - - - - - - - - - -       4N B; malloc
 *                      +-----------+                       \o
 *                      | def_val   | --       KeyOrValue     p   KeyOrValue[]
 *                      +-----------+   \    +-----------+    \t  +-----------+  opt.    UrlSpec      opt.
 *                      24 B; malloc     --> |           |     -> | val[0]    | - - -> +-----------+ - - -> char[]
 *                                           +-----------+        +-----------+        +-----------+ - - -> char[]
 *                                            4 B; malloc         :           :        :           : - - -> char[]
 *                                                                +-----------+        +-----------+ - - -> char[]
 *                                                                | val[n-1]  |        +-----------+        ? B; realloc
 *                                                                +-----------+        16 B; malloc              UrlSpec
 *                                                                4N B; malloc
 */


/* Type definitions */

/*+ The type of value to expect for a value. +*/
typedef enum _ConfigType
{
 Fixed,                         /*+ When the left hand side is fixed. +*/
 None,                          /*+ When there is no right hand side. +*/

 CfgMaxServers,                 /*+ Max number of servers to fork (>0, <MAX_SERVERS). +*/
 CfgMaxFetchServers,            /*+ Max number of servers for fetching pages (>0, <MAX_FETCH_SERVERS). +*/

 CfgLogLevel,                   /*+ A log level (debug, info, important, warning or fatal). +*/

 Boolean,                       /*+ A boolean response (yes/no 1/0 true/false). +*/

 PortNumber,                    /*+ For port numbers (>0). +*/

 AgeDays,                       /*+ An age in days (can be -ve). +*/
 TimeSecs,                      /*+ A time in seconds (can be -ve). +*/

 CacheSize,                     /*+ The cache size (must be >=0). +*/
 FileSize,                      /*+ A file size (must be >=0) +*/

 Percentage,                    /*+ A percentage (must be >=0 and <=100) +*/

 UserId,                        /*+ For user IDs, (numeric or string). +*/
 GroupId,                       /*+ For group IDs, (numeric or string). +*/

 String,                        /*+ For an arbitrary string. +*/

 PathName,                      /*+ For pathname values (string starting with '/'). +*/
 FileExt,                       /*+ A file extension (.string). +*/
 FileMode,                      /*+ The mode for dir/file creation. +*/

 MIMEType,                      /*+ A MIME type (string/string). +*/

 Host,                          /*+ For host names (string). +*/
 HostOrNone,                    /*+ For host names (string) or nothing. +*/
 HostAndPort,                   /*+ For host name and port numbers (string[:port]). +*/
 HostAndPortOrNone,             /*+ For host name and port numbers (string[:port]) or nothing. +*/
 HostWild,                      /*+ For host names that could be wildcards (string). +*/
 HostAndPortWild,               /*+ For host names and port numbers that could be wildcards (string[:port]). +*/

 UserPass,                      /*+ A username and password (string:string) +*/

 Url,                           /*+ For a URL ([proto://host[:port]]/path). +*/
 UrlWild,                       /*+ For a URL that could contain wildcards ([proto://host[:port]]/path). +*/

 UrlSpecification               /*+ A URL specification as described in README.CONF. +*/
}
ConfigType;


/*+ The reference to one item in a section of the configuration file as used by the rest of the program. +*/
typedef struct _ConfigItem* ConfigItem;

/*+ The definition of an item in a section of the configuration file. +*/
typedef struct _ConfigItemDef
{
            char       *name;     /*+ The name of the entry. +*/
            ConfigItem *item;     /*+ A pointer to the item containing the values for the entry. +*/
            int         url_type; /*+ Set to true if there is the option of having a URL present. +*/
            int         same_key; /*+ Set to true if the entry can repeat with the same key. +*/
            ConfigType  key_type; /*+ The type of the key on the left side of the equals sign. +*/
            ConfigType  val_type; /*+ The type of the value on the right side of the equals sign. +*/
 /*@null@*/ char       *def_val;  /*+ The default value if no other is specified. +*/
}
ConfigItemDef;

/*+ A section in the configuration file. +*/
typedef struct _ConfigSection
{
 char          *name;           /*+ The name of the section. +*/
 int            nitemdefs;      /*+ The number of item definitions in the section. +*/
 ConfigItemDef *itemdefs;       /*+ The item definitions in the section. +*/
}
ConfigSection;

/*+ A whole configuration file. +*/
typedef struct _ConfigFile
{
 char           *name;          /*+ The name of the file. +*/
 int             nsections;     /*+ The number of sections in the file. +*/
 ConfigSection **sections;      /*+ The sections in the file. +*/
}
ConfigFile;

/*+ A URL-SPECIFICATION as described in README.CONF. +*/
typedef struct _UrlSpec
{
          char  null;           /*+ A null character for parts that are not set. +*/
          char  negated;        /*+ Set to true if this is a negated URL-SPECIFICATION +*/
          char  nocase;         /*+ A flag that is set if case is ignored in the path.  +*/
 unsigned short proto;          /*+ The protocol or 0 (specified as an offset from start of UrlSpec). +*/
 unsigned short host;           /*+ The hostname or 0 (specified as an offset from start of UrlSpec). +*/
          int   port;           /*+ The port number (or 0 or -1). +*/
 unsigned short path;           /*+ The pathname or 0 (specified as an offset from start of UrlSpec). +*/
 unsigned short args;           /*+ The arguments or 0 (specified as an offset from start of UrlSpec). +*/
}
UrlSpec;

#define HasUrlSpecProto(xxx) ((xxx)->proto)
#define HasUrlSpecHost(xxx)  ((xxx)->host)
#define HasUrlSpecPath(xxx)  ((xxx)->path)
#define HasUrlSpecArgs(xxx)  ((xxx)->args)

#define UrlSpecProto(xxx) (char*)((char*)(xxx)+(int)(xxx)->proto)
#define UrlSpecHost(xxx)  (char*)((char*)(xxx)+(int)(xxx)->host)
#define UrlSpecPath(xxx)  (char*)((char*)(xxx)+(int)(xxx)->path)
#define UrlSpecArgs(xxx)  (char*)((char*)(xxx)+(int)(xxx)->args)

#define UrlSpecPort(xxx)  ((xxx)->port)

/*+ A key or a value. +*/
typedef union _KeyOrValue
{
 char    *string;               /*+ A string value. +*/
 int      integer;              /*+ An integer value. +*/
 UrlSpec *urlspec;              /*+ A URL-SPECIFICATION +*/
}
KeyOrValue;

/*+ One item in a section of the configuration file. +*/
struct _ConfigItem
{
 const ConfigItemDef *itemdef;  /*+ The corresponding item definition. +*/
       int            nentries; /*+ The number of entries in the lists. +*/
       UrlSpec      **url;      /*+ The list of URL-SPECIFICATIONs if present. +*/
       KeyOrValue    *key;      /*+ The list of keys. +*/
       KeyOrValue    *val;      /*+ The list of values. +*/
       KeyOrValue    *def_val;  /*+ The default value. +*/
};


/* in configrdwr.c */

char /*@only@*/ /*@null@*/ *ReadConfigFile(int read_startup);

char /*@only@*/ /*@null@*/ *ModifyConfigFile(int section,int item,/*@null@*/ char *newentry,/*@null@*/ char *preventry,/*@null@*/ char *sameentry,/*@null@*/ char *nextentry);

char /*@only@*/ /*@null@*/ *ParseKeyOrValue(const char *text,ConfigType type,/*@out@*/ KeyOrValue *pointer);


/* In configmisc.c */

void DefaultConfigFile(void);

void CreateBackupConfigFile(void);
void RestoreBackupConfigFile(void);

void PurgeBackupConfigFile(int restore_startup);
void PurgeConfigFile(void);

void FreeConfigItem(/*@null@*/ /*@special@*/ ConfigItem item) /*@releases item@*/;
void FreeKeyOrValue(/*@only@*/ KeyOrValue *keyval,ConfigType type);

int MatchUrlSpecification(const UrlSpec *spec,const char *proto,const char *host,int port,const char *path,/*@null@*/ const char *args);
int WildcardMatch(const char *string,const char *pattern,int nocase);

char *ConfigEntryString(ConfigItem item,int which);
void ConfigEntryStrings(ConfigItem item,int which,/*@out@*/ char **url,/*@out@*/ char **key,/*@out@*/ char **val);
char *MakeConfigEntryString(const ConfigItemDef *itemdef,/*@null@*/ const char *url,/*@null@*/ const char *key,/*@null@*/ const char *val);

/*@observer@*/ char *ConfigTypeString(ConfigType type);


/* In configfunc.c */

char *DefaultFTPPassWord(void);


/* Variable definitions */

/*+ The contents of the whole configuration file. +*/
extern ConfigFile CurrentConfig;


#endif /* CONFIGPRIV_H */
