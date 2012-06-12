/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/javaclass.c 1.11 2005/03/13 13:55:35 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Inspect a .class Object and look for other Objects.
  ******************/ /******************
  Written by W. Pfannenmueller

  This file Copyright 1998,99,2000,01,02,03,04,05 W. Pfannenmueller & Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "errors.h"
#include "document.h"


#define DEBUG_HTML 0

/*
#ifndef DEBUG
#define DEBUG
#endif
*/


typedef unsigned char Byte;
typedef unsigned long Int;
typedef unsigned int Short;

static Int getInt(int fd);
static Short getShort(int fd);
static Byte getByte(int fd);
static int readCONSTANTS(int fd);
static int isStandardClass(char *className);
static char *className;
static int read_byte(int fd);
static char *readUtf8(int fd); 
static char *readUnicode(int fd); 
static char *addclass(char *name);

static struct CONSTANTS
{
    Byte type;
    Byte isClassName;
    char *name;
} *constants;


static const char class[] = ".class";
static const char *standardClasses[] = {"java/", "javax/", 
                /* add more standard packages if necessary */ };


/*++++++++++++++++++++++++++++++++++++++
  Look for referenced classes 

  int InspectJavaClass Returns 1 if it is a valid class File and
                                 other class References have been found,
                               0 if not.
  ++++++++++++++++++++++++++++++++++++++*/

int InspectJavaClass(int fd,URL *Url)
{
 PrintMessage(Debug,"Parsing document using JavaClass parser.");

    className = Url->name;
    #ifdef DEBUG
    PrintMessage(Debug,"InspectClass %s",className);
    #endif
    /* test for valid class file */
    if(getInt(fd) == 0xcafebabeL)
    {
        /* Subversionsnummer */
        getShort(fd);
        /* Versionsnummer: */
        getShort(fd);
        /* now read all Constants including class names */    
        return readCONSTANTS(fd);
    }
    #ifdef DEBUG
    PrintMessage(Debug,"No Java Bytecode");
    #endif
    return 0;
}            


static Int getInt(int fd)
{
     return 
          read_byte(fd) * 256 * 256 * 256 +
          read_byte(fd) * 256 * 256 +
          read_byte(fd) * 256 +
          read_byte(fd);
}

static Short getShort(int fd)
{
     return 
          read_byte(fd) * 256 +
          read_byte(fd);
}

static Byte getByte(int fd)
{
     return 
          read_byte(fd);
}

#define CONSTANT_Utf8 1 
#define CONSTANT_Unicode 2 
#define CONSTANT_Integer 3 
#define CONSTANT_Float 4 
#define CONSTANT_Long 5 
#define CONSTANT_Double 6 
#define CONSTANT_Class 7 
#define CONSTANT_String 8 
#define CONSTANT_Fieldref 9 
#define CONSTANT_Methodref 10 
#define CONSTANT_InterfaceMethodref 11 
#define CONSTANT_NameAndType 12 

static char *readUtf8(int fd) 
{
    char *ret;
    Short i;
    Short len = getShort(fd);
    ret = (char *)malloc(len + sizeof('\0'));
    for(i = 0; i < len; i++)
    {
        ret[i] = (char)getByte(fd);
    }
    ret[i] = '\0';    
    return ret;
}

static unsigned char *UnicodeToUTF8(unsigned char *uni,int len)
{
    int i,j;
    unsigned char *ret = (unsigned char *)malloc(3 * len + 1);
    for(i = 0, j = 0; i < len*2; i+= 2,j++)
    {
        /* 1 byte */
        if(uni[i] == 0 && !(uni[i+1] & 0x80) && uni[i+1])
        {
			ret[j] = uni[i+1];
        }
        /* 2 bytes */
        else if(!(uni[i] & ~0x07)) 
        {
            ret[j] = 0xC0 | (uni[i] << 2) | (uni[i+1] >> 6); 
            ret[j+1] = 0xBF & (0x80 | uni[i+1]);
            j++;
        }
        /* 3 bytes */
        else 
        {
            ret[j] = 0xE0 | (uni[i] >> 4);
            ret[j+1] = 0xBF & (0x80 | (uni[i] << 2) | (uni[i+1] >> 6)); 
            ret[j+2] = 0xBF & (0x80 | uni[i+1]);
            j += 2;
        }
    }
    free(uni);
    return ret;
}

static char *readUnicode(int fd) 
{
    Short i;
    Short len = getShort(fd);
    unsigned char *ret = (unsigned char *)malloc(len);
    for(i = 0; i < len; i++)
    {
        ret[i] = getByte(fd);
    }
    return (char*)UnicodeToUTF8(ret,(int)len);
}


static int readCONSTANTS(int fd)
{
    int ret = 0;
    int i;
    Short s;
    int nclasses = 0;
    /* Anzahl Konstanten - 1 */
    Short nconstants = getShort(fd); 
    constants = 
        (struct CONSTANTS *)malloc(sizeof(struct CONSTANTS) * nconstants);
    memset(constants,'\0',sizeof(struct CONSTANTS) * nconstants);
    for(i = 1; i < nconstants; i++)
    {
        Byte b = getByte(fd);
        constants[i].type = b;
        switch(b)
        {
            case CONSTANT_Utf8:
                constants[i].name = readUtf8(fd);
                break;
            case CONSTANT_Unicode:
                constants[i].name = readUnicode(fd);
                break;
            case CONSTANT_Integer: 
            case CONSTANT_Float: 
                getInt(fd);
                break;
            case CONSTANT_Long: 
            case CONSTANT_Double: 
                getInt(fd);
                getInt(fd);
                i++;
                break;
            case CONSTANT_Class: 
                s = getShort(fd);
                constants[s].isClassName = 1;
                nclasses++; 
                break;
            case CONSTANT_String: 
                getShort(fd);
                break;
            case CONSTANT_Methodref: 
            case CONSTANT_Fieldref: 
            case CONSTANT_InterfaceMethodref: 
            case CONSTANT_NameAndType: 
                getShort(fd);
                getShort(fd);
                break;
            default:
                #ifdef DEBUG
                PrintMessage(Debug,"unknown CONSTANT type: %d",b);
                #endif
                #if DEBUG_HTML
                PrintMessage(Warning,"invalid class file \"%s\"",className);
                #endif
                return 0;
        } 
    }
    for(i = 0; i < nconstants; i++)
    {
        char *name = constants[i].name;
        if(name != NULL)
        {
            if(constants[i].isClassName != 0 && 
               !isStandardClass(name)
            ) 
            {
                name = addclass(name);  
                if(strstr(className,name) == NULL)
                {
                    #ifdef DEBUG
                    PrintMessage(Debug,"Class: %s",name);
                    #endif
                    AddReference(name,RefInlineObject);
                    ret = 1;
                }
            }
            #ifdef DEBUG
            else
            {
                PrintMessage(Debug,"Not fetched: %s",name);
            }
            #endif
            free(name);
        }
    }
    return ret;
}

static char *addclass(char *name)
{
    char *ret = (char *)malloc(strlen(name) + sizeof(class));
    strcpy(ret,name);
    strcat(ret,class);
    free(name);
    return ret;
}

static int isStandardClass(char *name)
{
    unsigned i;
    for(i = 0; i < (sizeof(standardClasses)/sizeof(char *)); i++)
    {
         if(!strncmp(name,
            standardClasses[i],
            strlen(standardClasses[i]))
         )
         {
            return 1;
         } 
    }
    return 0;
}

static int read_byte(int fd)
{
   unsigned char byte;
   if(read_data(fd,(char*)&byte,1) != 1)
   {
        #if DEBUG_HTML
        PrintMessage(Warning,"garbled class file \"%s\"",className);
        #endif
        return EOF;
   }
   return byte;
}
