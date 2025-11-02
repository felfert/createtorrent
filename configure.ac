AC_INIT([createtorrent],[1.1.5])
AC_CONFIG_SRCDIR([main.c])
AM_INIT_AUTOMAKE([foreign])
AC_PREREQ([2.72])
AC_USE_SYSTEM_EXTENSIONS
AC_CONFIG_HEADERS([config.h])
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strdup])
AC_CHECK_LIB([crypto],[SHA1],,[AC_MSG_ERROR([error, OpenSSL required])])
AC_HEADER_DIRENT
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP

AC_CHECK_HEADER([openssl/sha.h],,[AC_MSG_ERROR([Error, OpenSSL header file sha.h not found.])])
AC_C_CONST
AC_TYPE_SIZE_T
AC_CHECK_HEADERS_ONCE([sys/time.h])

AC_FUNC_STAT
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
