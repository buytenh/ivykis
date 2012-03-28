AM_CPPFLAGS		= -D_GNU_SOURCE -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include -I$(top_srcdir)/modules/include

AM_CFLAGS		= -Wall

LDADD			= $(top_builddir)/libivykis.la -lssl -lcrypto

noinst_PROGRAMS		= test

test_SOURCES		= test.c iv_getaddrinfo.c

noinst_HEADERS		= iv_getaddrinfo.h
