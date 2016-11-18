AM_CPPFLAGS		= -D_GNU_SOURCE				\
			  -I$(top_srcdir)/../src/include	\
			  -I$(top_builddir)/../src/include

LDADD			= $(top_builddir)/../src/libivykis.la

PROGS			= iv_event_bench_timer		\
			  iv_event_test			\
			  iv_thread_test		\
			  iv_work_test

if HAVE_POSIX
PROGS			+= iv_event_bench_signal	\
			   iv_signal_thread_test	\
			   iv_wait_test			\
			   server			\
			   server_thread
endif

if HAVE_WIN32
PROGS			+= handle
endif

noinst_PROGRAMS		= $(PROGS)

handle_SOURCES			= handle.c
iv_event_test_SOURCES		= iv_event_test.c
iv_signal_thread_test_SOURCES	= iv_signal_thread_test.c
iv_thread_test_SOURCES		= iv_thread_test.c
iv_wait_test_SOURCES		= iv_wait_test.c
iv_work_test_SOURCES		= iv_work_test.c

iv_event_bench_signal_CPPFLAGS	= $(AM_CPPFLAGS) -DUSE_SIGNAL
iv_event_bench_signal_SOURCES	= iv_event_bench.c

iv_event_bench_timer_SOURCES	= iv_event_bench.c

server_SOURCES		= server.c

server_thread_CPPFLAGS	= $(AM_CPPFLAGS) -DTHREAD
server_thread_SOURCES	= server.c
