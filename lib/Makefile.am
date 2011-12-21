SUBDIRS = . man3 test

AM_CPPFLAGS		= -D_GNU_SOURCE -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include

AM_CFLAGS		= -Wall

noinst_LTLIBRARIES	= libivykis.la

if HAVE_DEV_POLL
DEV_POLL_SRC = iv_method_dev_poll.c
else
DEV_POLL_SRC =
endif

if HAVE_EPOLL
EPOLL_SRC = iv_method_epoll.c
else
EPOLL_SRC =
endif

if HAVE_KQUEUE
KQUEUE_SRC = iv_method_kqueue.c
else
KQUEUE_SRC =
endif

if HAVE_PORT
PORT_SRC = iv_method_port.c
else
PORT_SRC =
endif

libivykis_la_SOURCES	= iv_avl.c			\
			  iv_main.c			\
			  $(DEV_POLL_SRC)		\
			  $(EPOLL_SRC)			\
			  $(KQUEUE_SRC)			\
			  $(PORT_SRC)			\
			  iv_method_poll.c		\
			  iv_method_select.c		\
			  iv_task.c			\
			  iv_timer.c			\
			  iv_tls.c

include_HEADERS		= include/iv.h			\
			  include/iv_avl.h		\
			  include/iv_list.h		\
			  include/iv_tls.h

noinst_HEADERS		= iv_private.h
