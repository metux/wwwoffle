/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/certificates.h 1.7 2006/02/28 19:26:03 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Certificate handling functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 2005,06 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef CERTIFICATES_H
#define CERTIFICATES_H    /*+ To stop multiple inclusions. +*/

#if USE_GNUTLS

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

/* In certificates.c */

int LoadTrustedCertificates(void);
int LoadRootCredentials(void);

void FreeLoadedCredentials(void);

gnutls_certificate_credentials_t GetFakeCredentials(const char *hostname);
gnutls_certificate_credentials_t GetServerCredentials(const char *hostname);
gnutls_certificate_credentials_t GetClientCredentials(void);

void FreeCredentials(gnutls_certificate_credentials_t cred);

int PutRealCertificate(gnutls_session_t session,const char *hostname);

gnutls_x509_crt_t VerifyCertificates(const char *hostname);

gnutls_x509_crt_t LoadCertificate(const char *filename);
gnutls_x509_crt_t *LoadCertificates(const char *filename);


#endif /* USE_GNUTLS */

#endif /* CERTIFICATES_H */
