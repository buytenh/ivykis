AM_CPPFLAGS		= -D_GNU_SOURCE -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include -I$(top_srcdir)/modules/include

AM_CFLAGS		= -Wall

LDADD			= $(top_builddir)/libivykis.la

noinst_PROGRAMS		= bench_sched			\
			  bench_signal_tls		\
			  bench_signal_atomic		\
			  bench_signal_atomic_fd	\
			  bench_signal_atomic_futex	\
			  bench_signal_fd		\
			  bench_sigqueue_atomic		\
			  bench_sigqueue_atomic_fd	\
			  bench_sigqueue_atomic_futex	\
			  bench_sigqueue_fd		\
			  test

bench_sched_SOURCES			= bench.c iv_mb_bcast_sched.c iv_mb_bcast.h
bench_signal_tls_SOURCES		= bench.c iv_mb_bcast_signal_tls.c iv_mb_bcast.h
bench_signal_atomic_SOURCES		= bench.c iv_mb_bcast_signal_atomic.c iv_mb_bcast.h
bench_signal_atomic_fd_SOURCES		= bench.c iv_mb_bcast_signal_atomic_fd.c iv_mb_bcast.h
bench_signal_atomic_futex_SOURCES	= bench.c iv_mb_bcast_signal_atomic_futex.c iv_mb_bcast.h
bench_signal_fd_SOURCES			= bench.c iv_mb_bcast_signal_fd.c iv_mb_bcast.h
bench_sigqueue_atomic_SOURCES		= bench.c iv_mb_bcast_sigqueue_atomic.c iv_mb_bcast.h
bench_sigqueue_atomic_fd_SOURCES	= bench.c iv_mb_bcast_sigqueue_atomic_fd.c iv_mb_bcast.h
bench_sigqueue_atomic_futex_SOURCES	= bench.c iv_mb_bcast_sigqueue_atomic_futex.c iv_mb_bcast.h
bench_sigqueue_fd_SOURCES		= bench.c iv_mb_bcast_sigqueue_fd.c iv_mb_bcast.h

test_SOURCES		= test.c iv_mb_bcast_sched.c iv_mb_bcast.h
