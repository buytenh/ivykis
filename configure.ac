AC_PREREQ(2.59)
AC_INIT([ivykis], [0.42.3], [libivykis-discuss@lists.sourceforge.net])
AC_CONFIG_SRCDIR([src/iv_avl.c])
AC_CONFIG_SUBDIRS([test.mt])
AC_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

#
# On AIX 7.1, prevent AC_PROG_CC from adding -g to CFLAGS, as that
# leads to the following linker error: "The symbol refers to a csect
# with symbol number 0, which was not found. The new symbol cannot
# be associated with a csect and is being ignored."
#
AC_CANONICAL_HOST
case $host_os in
aix7.*)
	ac_cv_prog_cc_g=no
	;;
*)
	;;
esac

# Checks for programs.
LT_INIT
AC_PROG_CC
AC_PROG_LIBTOOL

# Build with -Wall.
CFLAGS="$CFLAGS -Wall"

# Check the host operating system type: posix or win32.
AC_MSG_CHECKING([host operating system type])
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
	#include <unistd.h>
	#ifndef _POSIX_VERSION
	#error not POSIX
	#endif
	)],
	[ac_cv_host_system=posix
	 AC_MSG_RESULT([posix])],
	[AC_COMPILE_IFELSE([AC_LANG_SOURCE(
		#ifndef _WIN32
		#error not WIN32
		#endif
		)],
		[ac_cv_host_system=win32
		 AC_MSG_RESULT([win32])],
		[AC_MSG_RESULT([unknown])
		 AC_MSG_ERROR(Can't determine host system type.)])])

if test $ac_cv_host_system = posix
then

# OS-specific preprocessor defines.
case $host_os in
hpux*)
	#
	# On HP-UX, we need -D_XOPEN_SOURCE=500 so that the
	# Berkeley socket API uses 'socklen_t' instead of int.
	#
	CFLAGS="$CFLAGS -D_XOPEN_SOURCE=500"
	;;
osf*)
	#
	# On Tru64, we need -D_BSD to get a definition for 'union
	# wait', and -D_POSIX_PII_SOCKET so that sys/socket.h defines
	# 'socklen_t'.
	#
	CFLAGS="$CFLAGS -D_BSD -D_POSIX_PII_SOCKET"
	;;
solaris*)
	#
	# On Solaris, we want -D_REENTRANT, so that we get the
	# threadsafe version of 'errno'.
	#
	CFLAGS="$CFLAGS -D_REENTRANT"
esac

#
# On Linux, linking against libpthread.so.0 doesn't give you
# pthread_atfork(), as the latter is defined in libpthread_nonshared.a,
# which doesn't get pulled in if libpthread.so.0 is pulled in
# indirectly, e.g. via -lrt, but does get pulled in if you link against
# -lpthread, as libpthread.so is a linker script that references both
# libpthread.so.0 and libpthread_nonshared.a.  As we can link in
# libpthread_nonshared.a without pulling in libpthread.so.0, explicitly
# link in libpthread_nonshared.a if it is available.
#
AC_CHECK_LIB([pthread_nonshared], [pthread_atfork])

# Checks for header files.
AC_CHECK_HEADERS([process.h])
AC_CHECK_HEADERS([sys/devpoll.h])
AC_CHECK_HEADERS([sys/eventfd.h])
AC_CHECK_HEADERS([sys/syscall.h])
# Older FreeBSDs (6.1) don't include sys/ucontext.h in sys/thr.h.
AC_CHECK_HEADERS([sys/thr.h], [], [], [#include <sys/ucontext.h>])
AC_CHECK_HEADERS([thread.h])

# Check for <linux/netfilter_ipv4.h> header file.
AC_CACHE_CHECK(for linux/netfilter_ipv4.h, ac_cv_header_linux_netfilter_ipv4_h,
	[ac_cv_header_linux_netfilter_ipv4_h=no
	 _AC_COMPILE_IFELSE([AC_LANG_SOURCE(
		#include <arpa/inet.h>
		#include <linux/netfilter_ipv4.h>
	 )], [ac_cv_header_linux_netfilter_ipv4_h=yes], [])
	])

# Check for pthread_spinlock_t.
AC_CHECK_TYPES([pthread_spinlock_t], [], [], [[#include <pthread.h>]])

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

#
# Check for working support for #pragma weak.  This doesn't work
# quite right on HP-UX.
#
case $host_os in
hpux*)
	;;
*)
	AC_DEFINE(HAVE_PRAGMA_WEAK, 1,
		  Define to 1 if system has a working pragma weak)
	;;
esac

#
# Check for clock_gettime() and available time sources.  (We cannot
# test for the CLOCK_* constants by using #ifdef, as they are enums
# and not defines on at least HP-UX 11.)
#
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_CHECK_FUNCS([clock_gettime])
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

	AC_CACHE_CHECK(for CLOCK_MONOTONIC_FAST,
		ac_cv_define_clock_monotonic_fast,
		[ac_cv_define_clock_monotonic_fast=no
		 _AC_COMPILE_IFELSE([AC_LANG_SOURCE(
			#include <sys/time.h>
			#include <time.h>
			int p = CLOCK_MONOTONIC_FAST;
		 )], [ac_cv_define_clock_monotonic_fast=yes], [])
		])

	if test $ac_cv_define_clock_monotonic_fast = yes
	then
		AC_DEFINE(HAVE_CLOCK_MONOTONIC_FAST, 1,
			  Define to 1 if system has CLOCK_MONOTONIC_FAST)
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

# Checks for libraries.
AC_SEARCH_LIBS([inet_ntop], [nsl])
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([thr_self], [thread])

# Checks for library functions.
AC_CHECK_FUNCS([epoll_create])
AC_CHECK_FUNCS([epoll_create1])
AC_CHECK_FUNCS([eventfd])
AC_CHECK_FUNCS([gettid])
AC_CHECK_FUNCS([inotify_init])
AC_CHECK_FUNCS([kqueue])
AC_CHECK_FUNCS([lwp_gettid])
AC_CHECK_FUNCS([pipe2])
AC_CHECK_FUNCS([port_create])
AC_CHECK_FUNCS([ppoll])
AC_CHECK_FUNCS([thr_self])
AC_CHECK_FUNCS([timerfd_create])

#
# Only test for splice(2) on Linux, to avoid confusing it with a
# system function on AIX 5.1 with the same name.
#
case $host_os in
linux*)
	AC_CHECK_FUNCS([splice])
	;;
*)
	;;
esac

# Check whether wait4(2) is usable.
case $host_os in
aix*)
	#
	# AIX 5.1 and 7.1 have a broken wait4(2) implementation
	# that sometimes drops events on the floor, so use waitpid(2)
	# instead.
	#
	;;
nto-qnx*)
	#
	# Calls to wait4(2) with pid == -1 don't return any events on
	# QNX at all for some reason (and there are no man pages on
	# the QNX machine I have access to, so I can't find out why).
	# waitpid(2) seems to work, though.
	#
	;;
solaris*)
	#
	# The Solaris version of wait4(2) interprets a pid argument
	# of -1 as an instruction to wait for processes in process
	# group ID 1 (instead of waiting for all processes), which
	# is not what we want, so use waitpid(2) instead.
	#
	;;
*)
	AC_CHECK_FUNCS([wait4])
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

else

# Build a libivykis DLL.
AC_LIBTOOL_WIN32_DLL

fi


# Conditionals for OS type.
AM_CONDITIONAL([HAVE_POSIX], [test $ac_cv_host_system = posix])
AM_CONDITIONAL([HAVE_WIN32], [test $ac_cv_host_system = win32])

# Conditionals for poll methods.
AM_CONDITIONAL([HAVE_DEV_POLL], [test x$ac_cv_header_sys_devpoll_h = xyes])
AM_CONDITIONAL([HAVE_EPOLL], [test x$ac_cv_func_epoll_create = xyes])
AM_CONDITIONAL([HAVE_KQUEUE], [test x$ac_cv_func_kqueue = xyes])
AM_CONDITIONAL([HAVE_PORT], [test x$ac_cv_func_port_create = xyes])

# Other conditionals.
AM_CONDITIONAL([CASE_INSENSITIVE_FS],
		[test $srcdir/COPYING -ef $srcdir/copying])
AM_CONDITIONAL([HAVE_INOTIFY], [test x$ac_cv_func_inotify_init = xyes])
AM_CONDITIONAL([HAVE_LINUX_NETFILTER_IPV4_H],
		[test x$ac_cv_header_linux_netfilter_ipv4_h = xyes])
AM_CONDITIONAL([HAVE_VERSIONING], [test x$ac_cv_prog_ld_version_script = xyes])

AC_CONFIG_FILES([Makefile			\
		 contrib/Makefile		\
		 contrib/iv_getaddrinfo/Makefile	\
		 contrib/kojines/Makefile	\
		 man3/Makefile			\
		 misc/Makefile			\
		 misc/ivykis.pc			\
		 src/Makefile			\
		 src/include/iv.h		\
		 test/Makefile])
AC_OUTPUT
