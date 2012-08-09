AM_CPPFLAGS		= -D_GNU_SOURCE				\
			  -I$(top_srcdir)/src/include		\
			  -I$(top_builddir)/src/include

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

if HAVE_POLL
POLL_SRC = iv_method_poll.c
else
POLL_SRC =
endif

if HAVE_PORT
PORT_SRC = iv_method_port.c
else
PORT_SRC =
endif

if NEED_SELECT
SELECT_SRC = iv_method_select.c
else
SELECT_SRC =
endif

if HAVE_INOTIFY
INOTIFY_SRC = iv_inotify.c
INOTIFY_INCLUDE = include/iv_inotify.h
else
INOTIFY_SRC =
INOTIFY_INCLUDE =
endif

libivykis_la_SOURCES	= iv_avl.c			\
			  iv_event.c			\
			  iv_event_raw.c		\
			  iv_fd.c			\
			  iv_fd_pump.c			\
			  $(INOTIFY_SRC)		\
			  iv_main.c			\
			  $(DEV_POLL_SRC)		\
			  $(EPOLL_SRC)			\
			  $(KQUEUE_SRC)			\
			  $(PORT_SRC)			\
			  $(POLL_SRC)			\
			  $(SELECT_SRC)			\
			  iv_popen.c			\
			  iv_signal.c			\
			  iv_task.c			\
			  iv_thread.c			\
			  iv_timer.c			\
			  iv_tls.c			\
			  iv_wait.c			\
			  iv_work.c

include_HEADERS		= include/iv.h			\
			  include/iv_avl.h		\
			  include/iv_event.h		\
			  include/iv_event_raw.h	\
			  include/iv_fd_pump.h		\
			  $(INOTIFY_INCLUDE)		\
			  include/iv_list.h		\
			  include/iv_popen.h		\
			  include/iv_signal.h		\
			  include/iv_thread.h		\
			  include/iv_tls.h		\
			  include/iv_wait.h		\
			  include/iv_work.h

noinst_HEADERS		= iv_private.h			\
			  spinlock.h
