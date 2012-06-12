
dnl Checks for using gnutls.

AC_DEFUN([PKG_CHECK_GNUTLS],
[
    GNUTLS_CFLAGS=
    GNUTLS_LIBS="-lgnutls -lgcrypt"

    AC_CHECK_HEADERS(
	[gnutls/gnutls.h],
	[],
	AC_MSG_ERROR([Cannot find gnutls/gnutls.h header file])
    )

    AC_CHECK_LIB(
	[gnutls],
	[gnutls_check_version],
	[],
	AC_MSG_ERROR([Cannot find working libgnutls library])
    )

    AC_SUBST(GNUTLS_LIBS)
    AC_SUBST(GNUTLS_CFLAGS)
])
