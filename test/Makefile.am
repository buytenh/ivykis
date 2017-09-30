AM_CPPFLAGS		= -D_GNU_SOURCE			\
			  -I$(top_srcdir)/src/include	\
			  -I$(top_builddir)/src/include

LDADD			= $(top_builddir)/src/libivykis.la

PROGS			= iv_event_raw_bench_timer

TESTS			= avl				\
			  event_unregister_bug		\
			  iv_event_raw_test		\
			  struct_sizes			\
			  timer				\
			  timer_fairness		\
			  timer_fairness_bug		\
			  timer_order			\
			  timer_past			\
			  timer_zero

if HAVE_POSIX
PROGS			+= client			\
			   connectfail			\
			   connectreset			\
			   iv_event_raw_bench_signal	\
			   iv_fd_pump_discard		\
			   iv_fd_pump_echo		\
			   iv_popen_test		\
			   iv_signal_bench_signal	\
			   iv_signal_bench_timer	\
			   iv_signal_child_test		\
			   null

if HAVE_INOTIFY
PROGS			+= iv_inotify_test
endif

TESTS			+= iv_signal_test

endif

noinst_PROGRAMS		= $(PROGS) $(TESTS)

avl_SOURCES			= avl.c
client_SOURCES			= client.c
connectfail_SOURCES		= connectfail.c
connectreset_SOURCES		= connectreset.c
iv_event_raw_test_SOURCES	= iv_event_raw_test.c
iv_fd_pump_discard_SOURCES	= iv_fd_pump_discard.c
iv_fd_pump_echo_SOURCES		= iv_fd_pump_echo.c
iv_popen_test_SOURCES		= iv_popen_test.c
iv_signal_child_test_SOURCES	= iv_signal_child_test.c
iv_signal_test_SOURCES		= iv_signal_test.c
null_SOURCES			= null.c
struct_sizes_SOURCES		= struct_sizes.c
timer_SOURCES			= timer.c
timer_fairness_SOURCES		= timer_fairness.c
timer_order_SOURCES		= timer_order.c
timer_past_SOURCES		= timer_past.c

iv_event_raw_bench_signal_CPPFLAGS	= $(AM_CPPFLAGS) -DUSE_SIGNAL
iv_event_raw_bench_signal_SOURCES	= iv_event_raw_bench.c

iv_event_raw_bench_timer_SOURCES	= iv_event_raw_bench.c

iv_signal_bench_signal_CPPFLAGS		= $(AM_CPPFLAGS) -DUSE_SIGNAL
iv_signal_bench_signal_SOURCES		= iv_signal_bench.c

iv_signal_bench_timer_SOURCES		= iv_signal_bench.c
