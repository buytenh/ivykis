AM_CPPFLAGS		= -I$(top_srcdir)/lib/include -I$(top_srcdir)/modules/include

AM_CFLAGS		= -Wall

LDADD			= $(top_srcdir)/lib/libivykis.a $(top_srcdir)/modules/libivykis-modules.a -lssl -lcrypto

noinst_PROGRAMS		= test

test_SOURCES		= test.c iv_getaddrinfo.c iv_getaddrinfo.h
