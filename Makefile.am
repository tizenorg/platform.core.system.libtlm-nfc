ACLOCAL_AMFLAGS = -I m4

AM_DISTCHECK_CONFIGURE_FLAGS = \
    --enable-gtk-doc 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libtlm-nfc.pc

if HAVE_CHECK
SUBDIRS = src docs test
else
SUBDIRS = src docs
endif

valgrind:
	cd test; make valgrind

lcov: check
	@rm -rf lcov-report
	@lcov -c --directory src/ --output-file lcov.output
	@genhtml lcov.output --output-directory lcov-report
	@rm lcov.output
	@echo "Coverage report is in file://$(abs_srcdir)/lcov-report/index.html"

