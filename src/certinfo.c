/***************************************
  $Header: /home/amb/CVS/wwwoffle/src/certinfo.c,v 1.13 2007-11-27 17:32:09 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9d.
  Generate information about the contents of the web pages that are cached in the system.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 2002,03,04,05,06,07 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

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

#include <sys/stat.h>
#include <fcntl.h>

#if USE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#endif

#include "wwwoffle.h"
#include "errors.h"
#include "io.h"
#include "misc.h"
#include "config.h"
#include "certificates.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif


#if USE_GNUTLS

/*+ The trusted root certificate authority certificates. +*/
extern gnutls_x509_crt_t *trusted_x509_crts;

/*+ The number of trusted root certificate authority certificates. +*/
extern int n_trusted_x509_crts;


static void CertificatesIndexPage(int fd);
static void CertificatesRootPage(int fd,int download);
static void CertificatesServerPage(int fd,URL *Url);
static void CertificatesFakePage(int fd,URL *Url);
static void CertificatesRealPage(int fd,URL *Url);
static void CertificatesTrustedPage(int fd,URL *Url);

static void load_display_certificate(int fd,char *certfile,char *type,char *name);
static void display_certificate(int fd,gnutls_x509_crt_t crt);


/*++++++++++++++++++++++++++++++++++++++
  Display the certificates that are stored.

  int fd The file descriptor to write the output to.

  URL *Url The URL that was requested for the info.

  Header *request_head The header of the original request.
  ++++++++++++++++++++++++++++++++++++++*/

void CertificatesPage(int fd,URL *Url,/*@unused@*/ Header *request_head)
{
 if(!strcmp(Url->path,"/certificates") || !strcmp(Url->path,"/certificates/"))
    CertificatesIndexPage(fd);
 else if(!strcmp(Url->path,"/certificates/root"))
    CertificatesRootPage(fd,0);
 else if(!strcmp(Url->path,"/certificates/root-cert.pem"))
    CertificatesRootPage(fd,1);
 else if(!strcmp(Url->path,"/certificates/server") && Url->args)
    CertificatesServerPage(fd,Url);
 else if(!strcmp(Url->path,"/certificates/fake") && Url->args)
    CertificatesFakePage(fd,Url);
 else if(!strcmp(Url->path,"/certificates/real") && Url->args)
    CertificatesRealPage(fd,Url);
 else if(!strcmp(Url->path,"/certificates/trusted") && Url->args)
    CertificatesTrustedPage(fd,Url);
 else
    HTMLMessage(fd,404,"WWWOFFLE Illegal Certificates Page",NULL,"CertIllegal",
                "url",Url->pathp,
                NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the main certificates listing page.

  int fd The file descriptor to write the output to.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesIndexPage(int fd)
{
 DIR *dir;
 struct dirent* ent;
 int i;

 HTMLMessageHead(fd,200,"WWWOFFLE Certificates Index",
                 NULL);
 HTMLMessageBody(fd,"CertIndex-Head",
                 NULL);

 HTMLMessageBody(fd,"CertIndex-Body",
                 "type","root",
                 "name","WWWOFFLE CA",
                 NULL);

 /* Read in all of the certificates in the server directory. */

 dir=opendir("certificates/server");
 if(dir)
   {
    ent=readdir(dir);
    if(ent)
      {
       do
         {
          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

          if(strlen(ent->d_name)>9 && !strcmp(ent->d_name+strlen(ent->d_name)-9,"-cert.pem"))
            {
             char *server=(char*)malloc(strlen(ent->d_name)+1);
             strcpy(server,ent->d_name);
             server[strlen(ent->d_name)-9]=0;

#if defined(__CYGWIN__)
             for(i=0;server[i];i++)
                if(server[i]=='!')
                   server[i]=':';
#endif

             HTMLMessageBody(fd,"CertIndex-Body",
                             "type","server",
                             "name",server,
                             NULL);

             free(server);
            }
         }
       while((ent=readdir(dir)));
      }
    closedir(dir);
   }

 /* Read in all of the certificates in the fake directory. */

 dir=opendir("certificates/fake");
 if(dir)
   {
    ent=readdir(dir);
    if(ent)
      {
       do
         {
          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

          if(strlen(ent->d_name)>9 && !strcmp(ent->d_name+strlen(ent->d_name)-9,"-cert.pem"))
            {
             char *fake=(char*)malloc(strlen(ent->d_name)+1);
             strcpy(fake,ent->d_name);
             fake[strlen(ent->d_name)-9]=0;

#if defined(__CYGWIN__)
             for(i=0;fake[i];i++)
                if(fake[i]=='!')
                   fake[i]=':';
#endif

             HTMLMessageBody(fd,"CertIndex-Body",
                             "type","fake",
                             "name",fake,
                             NULL);

             free(fake);
            }
         }
       while((ent=readdir(dir)));
      }
    closedir(dir);
   }

 /* Read in all of the certificates in the real directory. */

 dir=opendir("certificates/real");
 if(dir)
   {
    ent=readdir(dir);
    if(ent)
      {
       do
         {
          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

          if(strlen(ent->d_name)>9 && !strcmp(ent->d_name+strlen(ent->d_name)-9,"-cert.pem"))
            {
             char *real=(char*)malloc(strlen(ent->d_name)+1);
             strcpy(real,ent->d_name);
             real[strlen(ent->d_name)-9]=0;

#if defined(__CYGWIN__)
             for(i=0;real[i];i++)
                if(real[i]=='!')
                   real[i]=':';
#endif

             HTMLMessageBody(fd,"CertIndex-Body",
                             "type","real",
                             "name",real,
                             NULL);

             free(real);
            }
         }
       while((ent=readdir(dir)));
      }
    closedir(dir);
   }

 /* List all of the trusted certificates */

 for(i=0;i<n_trusted_x509_crts;i++)
   {
    char dn[256];
    size_t size;

    size=sizeof(dn);
    gnutls_x509_crt_get_dn(trusted_x509_crts[i],dn,&size);
    
    HTMLMessageBody(fd,"CertIndex-Body",
                    "type","trusted",
                    "name",dn,
                    NULL);
   }

 HTMLMessageBody(fd,"CertIndex-Tail",
                 NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the certificate for the certificate authority.

  int fd The file descriptor to write the output to.

  int download If set true then download the raw certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesRootPage(int fd,int download)
{
 static char *certfile="certificates/root/root-cert.pem";

 if(download)
   {
    int cert_fd;
    char buffer[IO_BUFFER_SIZE];
    int nbytes;

    cert_fd=open(certfile,O_RDONLY|O_BINARY);

    if(cert_fd<0)
      {
       PrintMessage(Warning,"Could not open certificate file '%s' for writing [%!s].",certfile);
       HTMLMessage(fd,500,"WWWOFFLE Certificate Page Error",NULL,"ServerError",
                   "error","Cannot open specified certificate file",
                   NULL);
       return;
      }

    HTMLMessageHead(fd,200,"WWWOFFLE Root Certificate",
                    "Content-Type",WhatMIMEType("*.pem"),
                    NULL);

    while((nbytes=read(cert_fd,buffer,IO_BUFFER_SIZE))>0)
       write_data(fd,buffer,nbytes);

    close(cert_fd);
   }
 else
    load_display_certificate(fd,certfile,"root","root");
}


/*++++++++++++++++++++++++++++++++++++++
  Display the certificate for one of the WWWOFFLE server aliases.

  int fd The file descriptor to write the output to.

  URL *Url The exact URL given to reach this page, specifies the server certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesServerPage(int fd,URL *Url)
{
 char *name=Url->args;
 char *certfile=(char*)malloc(32+strlen(name));
#if defined(__CYGWIN__)
 int i;
#endif

 sprintf(certfile,"certificates/server/%s-cert.pem",name);

#if defined(__CYGWIN__)
 for(i=0;certfile[i];i++)
    if(certfile[i]==':')
       certfile[i]='!';
#endif

 load_display_certificate(fd,certfile,"server",name);

 free(certfile);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the fake certificate for one of the cached servers.

  int fd The file descriptor to write the output to.

  URL *Url The exact URL given to reach this page, specifies the fake certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesFakePage(int fd,URL *Url)
{
 char *name=Url->args;
 char *certfile=(char*)malloc(32+strlen(name));
#if defined(__CYGWIN__)
 int i;
#endif

 sprintf(certfile,"certificates/fake/%s-cert.pem",name);

#if defined(__CYGWIN__)
 for(i=0;certfile[i];i++)
    if(certfile[i]==':')
       certfile[i]='!';
#endif

 load_display_certificate(fd,certfile,"fake",name);

 free(certfile);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the real certificate for one of the cached pages.

  int fd The file descriptor to write the output to.

  URL *Url The exact URL given to reach this page, specifies the real certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesRealPage(int fd,URL *Url)
{
 gnutls_x509_crt_t *crt_list,trusted;
 int i=0;
 char *name=Url->args;
 char *certfile=(char*)malloc(32+strlen(name));
 char dn[256];

 sprintf(certfile,"certificates/real/%s-cert.pem",name);

#if defined(__CYGWIN__)
 for(i=0;certfile[i];i++)
    if(certfile[i]==':')
       certfile[i]='!';
#endif

 crt_list=LoadCertificates(certfile);

 free(certfile);

 if(!crt_list || !crt_list[0])
   {
    HTMLMessage(fd,500,"WWWOFFLE Certificate Page Error",NULL,"ServerError",
                "error","Cannot open specified certificate file",
                NULL);
    return;
   }

 HTMLMessageHead(fd,200,"WWWOFFLE Certificate Information",
                 NULL);
 HTMLMessageBody(fd,"CertInfo-Head",
                 "type","real",
                 "name",name,
                 NULL);

 while(crt_list[i])
   {
    display_certificate(fd,crt_list[i]);

    gnutls_x509_crt_deinit(crt_list[i]);

    i++;
   }

 /* Check if certificate is trusted */

 *dn=0;

 trusted=VerifyCertificates(name);

 if(trusted)
   {
    size_t size;

    size=sizeof(dn);
    gnutls_x509_crt_get_dn(trusted,dn,&size);
   }

 HTMLMessageBody(fd,"CertInfo-Tail",
                 "trustedby",dn,
                 NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the certificate for one of the trusted certificate authorities.

  int fd The file descriptor to write the output to.

  URL *Url The exact URL given to reach this page, specifies the trusted certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesTrustedPage(int fd,URL *Url)
{
 int i;
 char *name=URLDecodeFormArgs(Url->args);

 for(i=0;i<n_trusted_x509_crts;i++)
   {
    char dn[256];
    size_t size;

    size=sizeof(dn);
    gnutls_x509_crt_get_dn(trusted_x509_crts[i],dn,&size);

    if(!strcmp(name,dn))
      {
       HTMLMessageHead(fd,200,"WWWOFFLE Certificate Information",
                       NULL);
       HTMLMessageBody(fd,"CertInfo-Head",
                       "type","trusted",
                       "name",name,
                       NULL);

       display_certificate(fd,trusted_x509_crts[i]);

       HTMLMessageBody(fd,"CertInfo-Tail",
                       NULL);

       free(name);
       return;
      }
   }

 HTMLMessage(fd,500,"WWWOFFLE Certificate Page Error",NULL,"ServerError",
             "error","Cannot find specified certificate",
             NULL);

 free(name);
}


/*++++++++++++++++++++++++++++++++++++++
  Load a certificate from a file and display the information about it.

  int fd The file descriptor to write the output to.

  char *certfile The name of the file containing the certificate.

  char *type The type of the certificate.

  char *name The name of the server, fake, real or trusted certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void load_display_certificate(int fd,char *certfile,char *type,char *name)
{
 gnutls_x509_crt_t crt;

 crt=LoadCertificate(certfile);

 if(!crt)
   {
    HTMLMessage(fd,500,"WWWOFFLE Certificate Page Error",NULL,"ServerError",
                "error","Cannot open specified certificate file",
                NULL);
    return;
   }

 HTMLMessageHead(fd,200,"WWWOFFLE Certificate Information",
                 NULL);
 HTMLMessageBody(fd,"CertInfo-Head",
                 "type",type,
                 "name",name,
                 NULL);

 display_certificate(fd,crt);

 HTMLMessageBody(fd,"CertInfo-Tail",
                 NULL);

 gnutls_x509_crt_deinit(crt);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the information about a certificate.

  int fd The file descriptor to write the output to.

  gnutls_x509_crt_t crt The certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void display_certificate(int fd,gnutls_x509_crt_t crt)
{
 gnutls_datum_t txt={NULL,0};
 char *dn,*issuer_dn;
 time_t activation,expiration;
 char *activation_str,*expiration_str;
 char key_algo[80],key_usage[160],*key_ca;
 unsigned int bits,usage,critical;
 int algo;
 size_t size;
 int err;

 /* Certificate name */

 size=256;
 dn=(char*)malloc(size);
 err=gnutls_x509_crt_get_dn(crt,dn,&size);
 if(err==GNUTLS_E_SHORT_MEMORY_BUFFER)
   {
    dn=(char*)realloc((void*)dn,size);
    err=gnutls_x509_crt_get_dn(crt,dn,&size);
   }

 if(err)
    strcpy(dn,"Error");

 /* Issuer's name */

 size=256;
 issuer_dn=(char*)malloc(size);
 err=gnutls_x509_crt_get_issuer_dn(crt,issuer_dn,&size);
 if(err==GNUTLS_E_SHORT_MEMORY_BUFFER)
   {
    issuer_dn=(char*)realloc((void*)issuer_dn,size);
    err=gnutls_x509_crt_get_issuer_dn(crt,issuer_dn,&size);
   }

 if(err)
    strcpy(issuer_dn,"Error");

 /* Activation time */

 activation=gnutls_x509_crt_get_activation_time(crt);

 if(activation==-1)
    activation_str="Error";
 else
    activation_str=RFC822Date(activation,1);

 /* Expiration time */

 expiration=gnutls_x509_crt_get_expiration_time(crt);

 if(expiration==-1)
    expiration_str="Error";
 else
    expiration_str=RFC822Date(expiration,1);

 /* Algorithm type. */

 algo=gnutls_x509_crt_get_pk_algorithm(crt,&bits);

 if(algo<0)
    strcpy(key_algo,"Error");
 else
    sprintf(key_algo,"%s (%u bits)",gnutls_pk_algorithm_get_name(algo),bits);

 /* Key usage. */

 err=gnutls_x509_crt_get_key_usage(crt,&usage,&critical);

 if(err==GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
    strcpy(key_usage,"Unknown");
 else if(err<0)
    strcpy(key_usage,"Error");
 else
   {
    strcpy(key_usage,"");
    if(usage&GNUTLS_KEY_DIGITAL_SIGNATURE) {if(*key_usage)strcat(key_usage,", "); strcat(key_usage,"Digital Signature");}
    if(usage&GNUTLS_KEY_NON_REPUDIATION)   {if(*key_usage)strcat(key_usage,", "); strcat(key_usage,"Non-repudiation");}
    if(usage&GNUTLS_KEY_KEY_ENCIPHERMENT)  {if(*key_usage)strcat(key_usage,", "); strcat(key_usage,"Key Encipherment");}
    if(usage&GNUTLS_KEY_DATA_ENCIPHERMENT) {if(*key_usage)strcat(key_usage,", "); strcat(key_usage,"Data Encipherment");}
    if(usage&GNUTLS_KEY_KEY_AGREEMENT)     {if(*key_usage)strcat(key_usage,", "); strcat(key_usage,"Key Agreement");}
    if(usage&GNUTLS_KEY_KEY_CERT_SIGN)     {if(*key_usage)strcat(key_usage,", "); strcat(key_usage,"Key Cert Sign");}
    if(usage&GNUTLS_KEY_CRL_SIGN)          {if(*key_usage)strcat(key_usage,", "); strcat(key_usage,"CRL Sign");}
    if(usage&GNUTLS_KEY_ENCIPHER_ONLY)     {if(*key_usage)strcat(key_usage,", "); strcat(key_usage,"Encipher Only");}
    if(usage&GNUTLS_KEY_DECIPHER_ONLY)     {if(*key_usage)strcat(key_usage,", "); strcat(key_usage,"Decipher Only");}
   }

 /* Certificate authority */

 err=gnutls_x509_crt_get_ca_status(crt,&critical);

 if(err>0)
    key_ca="Yes";
 else if(err==0)
    key_ca="No";
 else if(err==GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
    key_ca="Unknown";
 else
    key_ca="Error";

 /* Formatted certificate */

 gnutls_x509_crt_print(crt,GNUTLS_X509_CRT_FULL,&txt);

 /* Output the information. */

 HTMLMessageBody(fd,"CertInfo-Body",
                 "dn",dn,
                 "issuer_dn",issuer_dn,
                 "activation",activation_str,
                 "expiration",expiration_str,
                 "key_algo",key_algo,
                 "key_usage",key_usage,
                 "key_ca",key_ca,
                 "info",txt.data,
                 NULL);

 /* Tidy up and exit */

 free(dn);
 free(issuer_dn);

 free(txt.data);
}


#endif /* USE_GNUTLS */
