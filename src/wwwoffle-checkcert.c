/***************************************
  Check certificate for expiry
  ******************/ /******************
  Written by Paul Slootman for the Debian wwwoffle package.
  Much copied from certificates.c, written and copyright by Andrew M. Bishop.

  This file Copyright 2010 Paul Slootman
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#if USE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gcrypt.h>
#endif

// #include "wwwoffle.h"
#include "errors.h"
#include "config.h"
#include "certificates.h"

/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

/*+ The number of bits for RSA private keys. +*/
#define RSA_BITS 512


/* Local functions */

static gnutls_x509_privkey_t    LoadPrivateKey(const char *filename);


/*+ The WWWOFFLE root certificate authority certificate. +*/
static gnutls_x509_crt_t root_crt;

/*+ The WWWOFFLE root certificate authority private key. +*/
static gnutls_x509_privkey_t root_privkey;

/* for errors.o: */
int gai_errno = 0;
int io_errno = 0;
int io_strerror = 0;

/*++++++++++++++++++++++++++++++++++++++
  Load in the WWWOFFLE root certificate authority certificate and private key.

  Check whether the certificate is usable.
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc, char*argv[])
{
 int err;
 struct stat buf;
 time_t activation,expiration,now;

 InitErrorHandler("wwwoffle-checkcert", 0, 1);

 if (chdir("/etc/wwwoffle") < 0)
     PrintMessage(Fatal, "Failed to chdir(/etc/wwwoffle)");

 /* Initialise the gnutls library */

 gnutls_global_init();

 /* Use faster but less secure key generation. */

 /* Read in the root private key in this directory. */

 if(access("certificates/root/root-key.pem",F_OK))
   {
    PrintMessage(Fatal,"The WWWOFFLE root CA private key file 'certificates/root/root-key.pem' does not exist.");
    exit(1);
   }

 root_privkey=LoadPrivateKey("certificates/root/root-key.pem");
 if(!root_privkey)
    PrintMessage(Fatal,"The WWWOFFLE root CA private key file 'certificates/root/root-key.pem' cannot be loaded; delete problem file and (re)start WWWOFFLE to recreate it.");

 /* Read in the root certificate in this directory. */

 if(access("certificates/root/root-cert.pem",F_OK))
   {
    PrintMessage(Fatal,"The WWWOFFLE root CA certificate file 'certificates/root/root-cert.pem' does not exist.");
   }

 root_crt=LoadCertificate("certificates/root/root-cert.pem");
 if(!root_crt)
    PrintMessage(Fatal,"The WWWOFFLE root CA certificate file 'certificates/root/root-cert.pem' cannot be loaded; delete problem file and (re)start WWWOFFLE to recreate it.");

 /* Check for expired root certificate and replace it */

 now=time(NULL);
 activation=gnutls_x509_crt_get_activation_time(root_crt);
 expiration=gnutls_x509_crt_get_expiration_time(root_crt);

 if(activation==-1 || expiration==-1 || activation>now || expiration<now)
   {
    PrintMessage(Fatal,"The WWWOFFLE root CA certificate has expired.");
   }

 if(expiration<now+7*86400)
   {
    if (expiration<now+2*86400)
      {
       PrintMessage(Warning,"The WWWOFFLE root CA certificate will expire in a day.");
      }
    else
      {
       PrintMessage(Warning,"The WWWOFFLE root CA certificate will expire in %d days.", expiration-now/86400);
      }
   }
 else
   {
    PrintMessage(Debug,"Expires: %d sec", expiration-now);
   }

 return(0);
}    


/*++++++++++++++++++++++++++++++++++++++
  Load in a single certificate from a file (the first if multiple).

  gnutls_x509_crt_t LoadCertificate Returns the loaded certificate.

  const char *filename The name of the file to load the certificate from.
  ++++++++++++++++++++++++++++++++++++++*/

gnutls_x509_crt_t LoadCertificate(const char *filename)
{
 gnutls_x509_crt_t *crt_list;
 int i=0;

 crt_list=LoadCertificates(filename);

 if(!crt_list || !crt_list[0])
    return(NULL);

 while(crt_list[++i])
    gnutls_x509_crt_deinit(crt_list[i]);

 return(crt_list[0]);
}


/*++++++++++++++++++++++++++++++++++++++
  Load in a certificate from a file.

  gnutls_x509_crt_t LoadCertificates Returns the loaded certificates (NULL terminated list).

  const char *filename The name of the file to load the certificates from.
  ++++++++++++++++++++++++++++++++++++++*/

gnutls_x509_crt_t *LoadCertificates(const char *filename)
{
 static gnutls_x509_crt_t crt_list[257];
 unsigned int n_crt=sizeof(crt_list)/sizeof(gnutls_x509_crt_t)-1;
 int fd,err;

 crt_list[0]=NULL;

 /* Load the certificates from the file. */

 fd=open(filename,O_RDONLY|O_BINARY);
 if(fd<0)
   {PrintMessage(Warning,"Could not open certificate file '%s' for reading [%!s].",filename);return(NULL);}
 else
   {
    struct stat buf;
    unsigned char *buffer;
    size_t buffer_size;
    gnutls_datum_t datum;

    if(fstat(fd,&buf))
      {PrintMessage(Warning,"Could not determine length of certificate file '%s' [%!s].",filename);return(NULL);}

    buffer=(unsigned char*)malloc(buf.st_size);
    buffer_size=buf.st_size;

    if(read(fd,buffer,buffer_size)!=buffer_size)
      {PrintMessage(Warning,"Could not read certificate file '%s' [%!s].",filename); free(buffer);return(NULL);}

    datum.data=buffer;
    datum.size=buffer_size;

    err=gnutls_x509_crt_list_import(crt_list,&n_crt,&datum,GNUTLS_X509_FMT_PEM,GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED);
    if(err<0)
      {PrintMessage(Warning,"Could not import certificates [%s].",gnutls_strerror(err)); free(buffer);return(NULL);}

    crt_list[err]=NULL;

    free(buffer);

    close(fd);
   }

 return(crt_list);
}


/*++++++++++++++++++++++++++++++++++++++
  Load in a private key from a file.

  gnutls_x509_privkey_t LoadPrivateKey Returns the loaded private key.

  const char *filename The name of the file to load the private key from.
  ++++++++++++++++++++++++++++++++++++++*/

static gnutls_x509_privkey_t LoadPrivateKey(const char *filename)
{
 gnutls_x509_privkey_t privkey;
 unsigned char buffer[2*RSA_BITS]; /* works for 256 bit keys or longer. */
 size_t buffer_size=sizeof(buffer);
 gnutls_datum_t datum;
 int fd,err;

 /* Load the private key from the file. */

 fd=open(filename,O_RDONLY|O_BINARY);
 if(fd<0)
   {PrintMessage(Warning,"Could not open private key file '%s' for reading [%!s].",filename);return(NULL);}
 else
   {
    buffer_size=read(fd,buffer,buffer_size);

    if(buffer_size==sizeof(buffer))
      {PrintMessage(Warning,"Could not read private key file '%s' [buffer not big enough].",filename);return(NULL);}

    datum.data=buffer;
    datum.size=buffer_size;

    err=gnutls_x509_privkey_init(&privkey);
    if(err<0)
      {PrintMessage(Warning,"Could not initialise private key [%s].",gnutls_strerror(err));return(NULL);}

    err=gnutls_x509_privkey_import(privkey,&datum,GNUTLS_X509_FMT_PEM);
    if(err<0)
      {PrintMessage(Warning,"Could not import private key [%s].",gnutls_strerror(err));return(NULL);}

    close(fd);
   }

 return(privkey);
}
