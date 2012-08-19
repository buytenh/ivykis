AM_CPPFLAGS		= -D_GNU_SOURCE -I$(top_srcdir)/src/include	\
			  -I$(top_builddir)/src/include

LDADD			= $(top_builddir)/src/libivykis.la

PROGS			= client			\
			  iv_event_test			\
			  iv_thread_test		\
			  iv_work_test

TESTS			= avl				\
			  iv_event_raw_test		\
			  struct_sizes			\
			  timer				\
			  timer_order

if HAVE_POSIX
PROGS			+= connectfail			\
			   connectreset			\
			   iv_fd_pump_discard		\
			   iv_fd_pump_echo		\
			   iv_popen_test		\
			   iv_signal_child_test		\
			   iv_wait_test			\
			   null				\
			   server			\
			   server_thread

if HAVE_INOTIFY
PROGS			+= iv_inotify_test
endif

TESTS			+= iv_signal_test

endif

if HAVE_WIN32
PROGS			+= handle			\
			   iv_wsa_socket_test
endif

noinst_PROGRAMS		= $(PROGS) $(TESTS)

avl_SOURCES			= avl.c
client_SOURCES			= client.c
connectfail_SOURCES		= connectfail.c
connectreset_SOURCES		= connectreset.c
handle_SOURCES			= handle.c
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
timer_order_SOURCES		= timer_order.c

server_thread_CPPFLAGS	= -D_GNU_SOURCE -I$(top_srcdir)/src/include -I$(top_builddir)/src/include -DTHREAD
server_thread_SOURCES	= server.c
