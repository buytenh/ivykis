AM_CPPFLAGS		= -D_LGPL_SOURCE -I$(top_srcdir)/lib/include

AM_CFLAGS		= -Wall

LDADD			= $(top_builddir)/libivykis.la -lurcu

noinst_PROGRAMS		= test

test_SOURCES		= test.c iv_urcu_call.c iv_urcu_call.h
