SUBDIRS = . man3 test

AM_CPPFLAGS		= -D_GNU_SOURCE -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include

AM_CFLAGS		= -Wall

lib_LTLIBRARIES		= libivykis.la


METHODS :=
if ENABLE_DEV_POLL
METHODS += iv_method_dev_poll.c
endif

if ENABLE_EPOLL
METHODS += iv_method_epoll.c
endif

if ENABLE_KQUEUE
METHODS += iv_method_kqueue.c
endif

libivykis_la_SOURCES	= iv_avl.c			\
			  iv_main.c			\
			  $(METHODS)			\
			  iv_method_poll.c		\
			  iv_method_select.c		\
			  iv_task.c			\
			  iv_timer.c

include_HEADERS		= include/iv.h			\
			  include/iv_avl.h		\
			  include/iv_compat.h		\
			  include/iv_list.h		\
			  include/iv_config.h

noinst_HEADERS		= iv_private.h			\
			  iv_thr.h
