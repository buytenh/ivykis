AM_CPPFLAGS		= -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include -I$(top_srcdir)/modules/include

AM_CFLAGS		= -Wall

LDADD			= $(top_builddir)/libivykis.la -lssl -lcrypto

noinst_PROGRAMS		= echo stress

echo_SOURCES		= echo.c iv_openssl.c iv_openssl.h

stress_SOURCES		= stress.c iv_openssl.c iv_openssl.h

EXTRA_DIST		= mkkeycert
