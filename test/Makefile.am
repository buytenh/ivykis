AM_CPPFLAGS		= -D_GNU_SOURCE -I$(top_srcdir)/lib/include	\
			  -I$(top_builddir)/lib/include			\
			  -I$(top_srcdir)/modules/include

LDADD			= $(top_builddir)/libivykis.la

if HAVE_INOTIFY
INOTIFY_PROGS = iv_inotify_test
else
INOTIFY_PROGS =
endif

noinst_PROGRAMS		= avl				\
			  client			\
			  connectfail			\
			  connectreset 			\
			  iv_event_raw_test		\
			  iv_event_test			\
			  iv_fd_pump_discard		\
			  iv_fd_pump_echo		\
			  $(INOTIFY_PROGS)		\
			  iv_popen_test			\
			  iv_signal_child_test		\
			  iv_signal_test		\
			  iv_thread_test		\
			  iv_wait_test			\
			  iv_work_test			\
			  null				\
			  server			\
			  server_thread			\
			  struct_sizes			\
			  timer				\
			  timer_rat

avl_SOURCES			= avl.c
client_SOURCES			= client.c
connectfail_SOURCES		= connectfail.c
connectreset_SOURCES		= connectreset.c
iv_event_raw_test_SOURCES	= iv_event_raw_test.c
iv_event_test_SOURCES		= iv_event_test.c
iv_fd_pump_discard_SOURCES	= iv_fd_pump_discard.c
iv_fd_pump_echo_SOURCES		= iv_fd_pump_echo.c
iv_popen_test_SOURCES		= iv_popen_test.c
iv_signal_child_test_SOURCES	= iv_signal_child_test.c
iv_signal_test_SOURCES		= iv_signal_test.c
iv_thread_test_SOURCES		= iv_thread_test.c
iv_wait_test_SOURCES		= iv_wait_test.c
iv_work_test_SOURCES		= iv_work_test.c
null_SOURCES			= null.c
server_SOURCES			= server.c
struct_sizes_SOURCES		= struct_sizes.c
timer_SOURCES			= timer.c
timer_rat_SOURCES		= timer_rat.c

server_thread_CPPFLAGS	= -D_GNU_SOURCE -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include -DTHREAD
server_thread_SOURCES	= server.c
