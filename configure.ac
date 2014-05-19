AC_INIT([libtlm-nfc], [0.0.0],[],[],[http://tizen.org])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([subdir-objects])

LT_INIT([disable-static])

# Checks for libraries.
PKG_CHECK_MODULES([TLM_NFC], 
                  [glib-2.0 >= 2.30
                   gio-2.0
                   gio-unix-2.0
                   gmodule-2.0
                   ])
AC_SUBST(TLM_NFC_CFLAGS)
AC_SUBST(TLM_NFC_LIBS)


# AM_PATH_CHECK() is deprecated, but check documentation fails to tell that :-/
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4], [have_check=yes], [have_check=no])
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)
AM_CONDITIONAL(HAVE_CHECK, [test x$have_check = xyes])

# gtk-doc
GTK_DOC_CHECK([1.18],[--flavour no-tmpl])

AC_ARG_ENABLE([coverage],
    [AS_HELP_STRING([--enable-coverage], [compile with coverage info])])
AS_IF([test "x$enable_coverage" = "xyes"],
    [CFLAGS="$CFLAGS -fprofile-arcs -ftest-coverage"])

# Checks for typedefs, structures, and compiler characteristics.
TLM_NFC_CFLAGS="$TLM_NFC_CFLAGS -Wall -Werror -DG_LOG_DOMAIN=\\\"tlm-nfc\\\""

AC_OUTPUT([
Makefile
${PACKAGE_NAME}.pc
${PACKAGE_NAME}-uninstalled.pc
docs/Makefile
src/Makefile
test/Makefile
])

