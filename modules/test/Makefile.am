AM_CPPFLAGS		= -D_GNU_SOURCE -I$(top_srcdir)/lib/include	\
			  -I$(top_srcdir)/modules/include
AM_CFLAGS		= -Wall

LDADD			= $(top_srcdir)/lib/libivykis.a			\
			  $(top_srcdir)/modules/libivykis-modules.a

noinst_PROGRAMS		= iv_event_raw_test		\
			  iv_event_test			\
			  iv_popen_test			\
			  iv_signal_child_test		\
			  iv_signal_test		\
			  iv_thread_test		\
			  iv_wait_test			\
			  iv_work_test

iv_event_raw_test_SOURCES	= iv_event_raw_test.c
iv_event_test_SOURCES		= iv_event_test.c
iv_popen_test_SOURCES		= iv_popen_test.c
iv_signal_child_test_SOURCES	= iv_signal_child_test.c
iv_signal_test_SOURCES		= iv_signal_test.c
iv_thread_test_SOURCES		= iv_thread_test.c
iv_wait_test_SOURCES		= iv_wait_test.c
iv_work_test_SOURCES		= iv_work_test.c
