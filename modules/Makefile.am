SUBDIRS = . man3 test

AM_CPPFLAGS		= -D_GNU_SOURCE				\
			  -I$(top_srcdir)/lib/include		\
			  -I$(top_srcdir)/modules/include

AM_CFLAGS		= -Wall

lib_LTLIBRARIES		= libivykis-modules.la

libivykis_modules_la_SOURCES	= iv_event.c			\
				  iv_event_raw.c		\
				  iv_popen.c			\
				  iv_signal.c			\
				  iv_thread.c			\
				  iv_wait.c			\
				  iv_work.c

include_HEADERS		= include/iv_event.h		\
			  include/iv_event_raw.h	\
			  include/iv_popen.h		\
			  include/iv_signal.h		\
			  include/iv_thread.h		\
			  include/iv_wait.h		\
			  include/iv_work.h

noinst_HEADERS		= thr.h
