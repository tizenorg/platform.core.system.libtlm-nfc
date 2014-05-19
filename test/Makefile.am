TESTS = tlmnfctest
TESTS_ENVIRONMENT= CK_FORK=no

VALGRIND_TESTS_DISABLE=

check_PROGRAMS = tlmnfctest
tlmnfctest_SOURCES = tlmnfctest.c
tlmnfctest_CFLAGS = \
    $(TLM_NFC_CFLAGS) \
    $(CHECK_CFLAGS) \
    -I$(top_builddir) \
    -I$(top_srcdir)/src/

tlmnfctest_LDADD = \
    $(top_builddir)/src/libtlm-nfc.la \
    $(GSIGNOND_LIBS) \
    $(CHECK_LIBS)

include $(top_srcdir)/test/valgrind_common.mk

