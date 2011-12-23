AM_CPPFLAGS		= -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include -I$(top_srcdir)/modules/include

AM_CFLAGS		= -Wall

LDADD			= $(top_builddir)/lib/libivykis.la $(top_builddir)/modules/libivykis-modules.la -lssl -lcrypto

noinst_PROGRAMS		= test

test_SOURCES		= test.c iv_getaddrinfo.c iv_getaddrinfo.h
