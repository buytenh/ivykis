AM_CPPFLAGS		= -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include -I$(top_srcdir)/modules/include

AM_CFLAGS		= -Wall

LDADD			= $(top_builddir)/lib/libivykis.la $(top_builddir)/modules/libivykis-modules.la

noinst_PROGRAMS		= kojines

kojines_SOURCES		= main.c kojines.c kojines.h

EXTRA_DIST		= 00README.txt
