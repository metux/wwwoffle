
dnl Checks for zlib libraries.

AC_DEFUN([PKG_CHECK_ZLIB],
[
    ZLIB_LIBS=-lz
    ZLIB_CFLAGS=

    AC_CHECK_HEADERS(
	[zlib.h],
	[],
	AC_MSG_ERROR([Cannot find zlib.h header file])
    )

    AC_CHECK_LIB(
	[z],
	[zlibVersion],
	[WITH_ZLIB=yes],
	AC_MSG_ERROR([Cannot find working libz library])
	[$ZLIB_LIB]
    )

    AC_SUBST(ZLIB_LIBS)
    AC_SUBST(ZLIB_CFLAGS)
])
