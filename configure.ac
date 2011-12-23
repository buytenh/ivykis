AC_PREREQ(2.59)
AC_INIT([ivykis], [0.26], [libivykis-discuss@lists.sourceforge.net])
AC_CONFIG_SRCDIR([lib/iv_main.c])
AC_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
LT_INIT
AC_PROG_CC

# Checks for libraries.
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_SEARCH_LIBS([inet_ntop], [nsl])
AC_SEARCH_LIBS([pthread_create], [pthread], [],
	AC_MSG_ERROR(pthreads support is required to build ivykis.))
AC_SEARCH_LIBS([socket], [socket])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netinet/in.h stddef.h	\
		  stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h	\
		  sys/time.h sys/wait.h syslog.h unistd.h], [],
	AC_MSG_ERROR(some required header files not installed.))
AC_CHECK_HEADERS([sys/devpoll.h sys/select.h])

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

# Checks for library functions.
AC_CHECK_FUNCS([dup2 fork malloc memcmp memmove memset		\
		select socket strdup strerror], [],
	AC_MSG_ERROR(some required library functions not available.))
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_FUNCS([epoll_create])
AC_CHECK_FUNCS([epoll_create1])
AC_CHECK_FUNCS([inotify_init])
AC_CHECK_FUNCS([kqueue])
AC_CHECK_FUNCS([port_create])
AC_CHECK_FUNCS([splice])
AC_CHECK_FUNCS([wait4])

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

# Check for -pthreads.
ac_c_werror_flag=yes
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
AM_CONDITIONAL([HAVE_INOTIFY], [test x$ac_cv_func_inotify_init = xyes])
AM_CONDITIONAL([HAVE_KQUEUE], [test x$ac_cv_func_kqueue = xyes])
AM_CONDITIONAL([HAVE_PORT], [test x$ac_cv_func_port_create = xyes])

AC_CONFIG_FILES([Makefile			\
		 contrib/Makefile		\
		 contrib/iv_getaddrinfo/Makefile	\
		 contrib/iv_openssl/Makefile	\
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
