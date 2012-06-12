
dnl Checks for using gcrypt.

AC_DEFUN([PKG_CHECK_GCRYPT],
[
    GCRYPT_CFLAGS=
    GCRYPT_LIBS="-lgcrypt"

    AC_CHECK_HEADERS(
	[gcrypt.h],
	[],
	AC_MSG_WARN([Cannot find gcrypt.h header file])
    )

    AC_CHECK_LIB(
	[gcrypt],
	[gcry_check_version],
	[],
	AC_MSG_WARN([Cannot find working libgcrypt library])
    )

    AC_SUBST(GCRYPT_LIBS)
    AC_SUBST(GCRYPT_CFLAGS)
])
