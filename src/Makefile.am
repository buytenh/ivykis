AM_CPPFLAGS		= -D_GNU_SOURCE				\
			  -I$(top_srcdir)/src/include		\
			  -I$(top_builddir)/src/include

lib_LTLIBRARIES		= libivykis.la

SRC			= iv_avl.c			\
			  iv_event.c			\
			  iv_event_raw.c		\
			  iv_fatal.c			\
			  iv_fd.c			\
			  iv_fd_pump.c			\
			  iv_main.c			\
			  iv_popen.c			\
			  iv_signal.c			\
			  iv_task.c			\
			  iv_thread.c			\
			  iv_time.c			\
			  iv_timer.c			\
			  iv_tls.c			\
			  iv_wait.c			\
			  iv_work.c

INC			= include/iv.h			\
			  include/iv_avl.h		\
			  include/iv_event.h		\
			  include/iv_event_raw.h	\
			  include/iv_fd_pump.h		\
			  include/iv_list.h		\
			  include/iv_popen.h		\
			  include/iv_signal.h		\
			  include/iv_thread.h		\
			  include/iv_tls.h		\
			  include/iv_wait.h		\
			  include/iv_work.h

if HAVE_DEV_POLL
SRC			+= iv_fd_dev_poll.c
endif

if HAVE_EPOLL
SRC			+= iv_fd_epoll.c
endif

if HAVE_KQUEUE
SRC			+= iv_fd_kqueue.c
endif

if HAVE_POLL
SRC			+= iv_fd_poll.c
endif

if HAVE_PORT
SRC			+= iv_fd_port.c
endif

if NEED_SELECT
SRC			+= iv_fd_select.c
endif

if HAVE_INOTIFY
SRC			+= iv_inotify.c
INC			+= include/iv_inotify.h
endif

libivykis_la_SOURCES	= $(SRC)
libivykis_la_LDFLAGS	= -version-info 2:1:2
if HAVE_VERSIONING
libivykis_la_LDFLAGS	+= -Wl,--version-script,$(top_srcdir)/libivykis.ver
endif

include_HEADERS		= $(INC)

noinst_HEADERS		= iv_private.h			\
			  spinlock.h
