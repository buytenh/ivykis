AC_PREREQ(2.59)
AC_INIT([ivykis], [0.31], [libivykis-discuss@lists.sourceforge.net])
AC_CONFIG_SRCDIR([lib/iv_main.c])
AC_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
LT_INIT
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_SEARCH_LIBS([inet_ntop], [nsl])
AC_SEARCH_LIBS([pthread_atfork], [pthread], [],
	AC_MSG_ERROR(pthreads support is required to build ivykis.))
AC_SEARCH_LIBS([pthread_create], [pthread], [],
	AC_SEARCH_LIBS([__pthread_create], [pthread], [],
	AC_SEARCH_LIBS([pthread_trace_notify_np], [pthread], [],
		AC_MSG_ERROR(pthreads support is required to build ivykis.))))
AC_SEARCH_LIBS([socket], [socket])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netinet/in.h	\
		  stddef.h stdlib.h string.h sys/ioctl.h sys/socket.h	\
		  sys/time.h sys/wait.h syslog.h unistd.h], [],
	AC_MSG_ERROR(some required header files not installed.))
AC_CHECK_HEADERS([sys/devpoll.h sys/eventfd.h sys/select.h])

# Check for <linux/netfilter_ipv4.h> header file.
AC_CACHE_CHECK(for linux/netfilter_ipv4.h, ac_cv_header_linux_netfilter_ipv4_h,
	[ac_cv_header_linux_netfilter_ipv4_h=no
	 _AC_COMPILE_IFELSE([AC_LANG_SOURCE(
		#include <arpa/inet.h>
		#include <limits.h>
		#include <linux/netfilter_ipv4.h>
	 )], [ac_cv_header_linux_netfilter_ipv4_h=yes], [])
	])

# OS-specific preprocessor defines.
case $host_os in
hpux*)
	# On HP-UX, we need -D_XOPEN_SOURCE=500 so that the
	# Berkeley socket API uses 'socklen_t' instead of int.
	CFLAGS="$CFLAGS -D_XOPEN_SOURCE=500"
	;;

osf*)
	# On Tru64, we need -D_BSD to get a definition for 'union
	# wait', and -D_POSIX_PII_SOCKET so that sys/socket.h defines
	# 'socklen_t'.
	CFLAGS="$CFLAGS -D_BSD -D_POSIX_PII_SOCKET"
	;;
esac

# Check which header file defines 'struct timespec'.
for hdr in sys/time.h sys/timers.h time.h pthread.h
do
	AC_CHECK_MEMBER(struct timespec.tv_sec,
			[ac_cv_timespec_hdr=$hdr; break],
			[unset ac_cv_member_struct_timespec_tv_sec],
			[#include <$hdr>])
done
if test x$ac_cv_timespec_hdr = x
then
	AC_MSG_ERROR(Can't find definition of struct timespec.)
fi
AC_SUBST(ac_cv_timespec_hdr)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Check for __sync_lock_test_and_set().  We can't use AC_CHECK_FUNCS
# for this, as autoconf will spit out a dummy definition for the
# function we are testing for, and call it without any arguments,
# which will prevent gcc from using the intrinsic.
AC_CACHE_CHECK(for __sync_lock_test_and_set(),
	ac_cv_func_sync_lock_test_and_set,
	[ac_cv_func_sync_lock_test_and_set=no
	 _AC_LINK_IFELSE([AC_LANG_SOURCE(
		int main()
		{
			unsigned long test;
			while (__sync_lock_test_and_set(&test, 1) == 1)
				;
			return 0;
		}
	 )], [ac_cv_func_sync_lock_test_and_set=yes], [])
	])
if test $ac_cv_func_sync_lock_test_and_set = yes
then
	AC_DEFINE(HAVE_SYNC_LOCK_TEST_AND_SET, 1,
		  Define to 1 if system has __sync_lock_test_and_set)
fi

# Checks for library functions.
AC_CHECK_FUNCS([dup2 fork malloc memcmp memmove memset		\
		select socket strdup strerror], [],
	AC_MSG_ERROR(some required library functions not available.))
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_FUNCS([epoll_create])
AC_CHECK_FUNCS([epoll_create1])
AC_CHECK_FUNCS([eventfd])
AC_CHECK_FUNCS([inotify_init])
AC_CHECK_FUNCS([kqueue])
AC_CHECK_FUNCS([poll])
AC_CHECK_FUNCS([port_create])
AC_CHECK_FUNCS([pthread_spin_lock])
AC_CHECK_FUNCS([splice])
AC_CHECK_FUNCS([wait4])

# Check for available clock_gettime() clock sources.
if test $ac_cv_func_clock_gettime = yes
then
	AC_CACHE_CHECK(for CLOCK_MONOTONIC, ac_cv_define_clock_monotonic,
		[ac_cv_define_clock_monotonic=no
		 _AC_COMPILE_IFELSE([AC_LANG_SOURCE(
			#include <sys/time.h>
			#include <time.h>
			int p = CLOCK_MONOTONIC;
		 )], [ac_cv_define_clock_monotonic=yes], [])
		])

	if test $ac_cv_define_clock_monotonic = yes
	then
		AC_DEFINE(HAVE_CLOCK_MONOTONIC, 1,
			  Define to 1 if system has CLOCK_MONOTONIC)
	fi

	AC_CACHE_CHECK(for CLOCK_REALTIME, ac_cv_define_clock_realtime,
		[ac_cv_define_clock_realtime=no
		 _AC_COMPILE_IFELSE([AC_LANG_SOURCE(
			#include <sys/time.h>
			#include <time.h>
			int p = CLOCK_REALTIME;
		 )], [ac_cv_define_clock_realtime=yes], [])
		])

	if test $ac_cv_define_clock_realtime = yes
	then
		AC_DEFINE(HAVE_CLOCK_REALTIME, 1,
			  Define to 1 if system has CLOCK_REALTIME)
	fi
fi

# Check for __thread.
AC_MSG_CHECKING([for working __thread])
case $host_os in
netbsd*)
	AC_MSG_RESULT([no])
	;;
*)
	AC_COMPILE_IFELSE([AC_LANG_SOURCE(static __thread int p = 0;)],
		[AC_DEFINE(HAVE_THREAD, 1, Define to 1 if compiler
					   supports __thread.)
		 AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])])
	;;
esac

# Check for linker version script support.
ac_save_LDFLAGS=$LDFLAGS
AC_CACHE_CHECK(whether $LD supports symbol version scripts,
	ac_cv_prog_ld_version_script,
	[ac_cv_prog_ld_version_script=no
	 echo "TEST { local: *; };" > conftest.ver
	 LDFLAGS="$LDFLAGS -Wl,--version-script,conftest.ver"
	 _AC_LINK_IFELSE([AC_LANG_PROGRAM()],
		[ac_cv_prog_ld_version_script=yes], [])
	])
LDFLAGS=$ac_save_LDFLAGS
AM_CONDITIONAL([HAVE_VERSIONING], [test $ac_cv_prog_ld_version_script = yes])

# Check for -pthread.
ac_c_werror_flag=yes
ac_save_CFLAGS=$CFLAGS
AC_CACHE_CHECK(whether $CC accepts -pthread, ac_cv_prog_cc_pthread,
	[ac_cv_prog_cc_pthread=no
	 CFLAGS="-pthread"
	 _AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
		[ac_cv_prog_cc_pthread=yes], [])
	])
if test $ac_cv_prog_cc_pthread = yes
then
	CFLAGS="$ac_save_CFLAGS -pthread"
else
	CFLAGS=$ac_save_CFLAGS
fi

# Check for -pthreads.
ac_save_CFLAGS=$CFLAGS
AC_CACHE_CHECK(whether $CC accepts -pthreads, ac_cv_prog_cc_pthreads,
	[ac_cv_prog_cc_pthreads=no
	 CFLAGS="-pthreads"
	 _AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
		[ac_cv_prog_cc_pthreads=yes], [])
	])
if test $ac_cv_prog_cc_pthreads = yes
then
	CFLAGS="$ac_save_CFLAGS -pthreads"
else
	CFLAGS=$ac_save_CFLAGS
fi

# Conditionals for poll methods.
AM_CONDITIONAL([HAVE_DEV_POLL], [test x$ac_cv_header_sys_devpoll_h = xyes])
AM_CONDITIONAL([HAVE_EPOLL], [test x$ac_cv_func_epoll_create = xyes])
AM_CONDITIONAL([HAVE_KQUEUE], [test x$ac_cv_func_kqueue = xyes])
AM_CONDITIONAL([HAVE_POLL], [test x$ac_cv_func_poll = xyes])
AM_CONDITIONAL([HAVE_PORT], [test x$ac_cv_func_port_create = xyes])
AM_CONDITIONAL([NEED_SELECT],
		[test x$ac_cv_header_sys_devpoll_h != xyes &&
		 test x$ac_cv_func_epoll_create != xyes &&
		 test x$ac_cv_func_kqueue != xyes &&
		 test x$ac_cv_func_poll != xyes &&
		 test x$ac_cv_func_port_create != xyes])

# Other conditionals.
AM_CONDITIONAL([HAVE_INOTIFY], [test x$ac_cv_func_inotify_init = xyes])
AM_CONDITIONAL([HAVE_LINUX_NETFILTER_IPV4_H],
		[test x$ac_cv_header_linux_netfilter_ipv4_h = xyes])

AC_CONFIG_FILES([Makefile			\
		 contrib/Makefile		\
		 contrib/iv_getaddrinfo/Makefile	\
		 contrib/kojines/Makefile	\
		 lib/Makefile			\
		 lib/include/iv.h		\
		 lib/man3/Makefile		\
		 lib/test/Makefile		\
		 misc/Makefile			\
		 misc/ivykis.pc			\
		 modules/Makefile		\
		 modules/man3/Makefile		\
		 modules/test/Makefile])
AC_OUTPUT
