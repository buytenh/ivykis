AM_CPPFLAGS		= -I$(top_srcdir)/lib/include

AM_CFLAGS		= -Wall

LDADD			= $(top_srcdir)/lib/libivykis.a -lssl -lcrypto

noinst_PROGRAMS		= echo

echo_SOURCES		= echo.c iv_openssl.c iv_openssl.h

EXTRA_DIST		= mkkeycert
