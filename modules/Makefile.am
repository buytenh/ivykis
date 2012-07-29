SUBDIRS = . man3

AM_CPPFLAGS		= -D_GNU_SOURCE				\
			  -I$(top_srcdir)/lib			\
			  -I$(top_srcdir)/lib/include		\
			  -I$(top_builddir)/lib/include		\
			  -I$(top_srcdir)/modules/include

noinst_LTLIBRARIES	= libivykis-modules.la

if HAVE_INOTIFY
INOTIFY_SRC = iv_inotify.c
INOTIFY_INCLUDE = include/iv_inotify.h
else
INOTIFY_SRC =
INOTIFY_INCLUDE =
endif

libivykis_modules_la_SOURCES	= iv_event.c			\
				  iv_event_raw.c		\
				  iv_fd_pump.c			\
				  $(INOTIFY_SRC)		\
				  iv_popen.c			\
				  iv_signal.c			\
				  iv_thread.c			\
				  iv_wait.c			\
				  iv_work.c

include_HEADERS		= include/iv_event.h		\
			  include/iv_event_raw.h	\
			  include/iv_fd_pump.h		\
			  $(INOTIFY_INCLUDE)		\
			  include/iv_popen.h		\
			  include/iv_signal.h		\
			  include/iv_thread.h		\
			  include/iv_wait.h		\
			  include/iv_work.h

noinst_HEADERS		= spinlock.h
