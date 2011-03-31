AC_PREREQ(2.59)
AC_INIT([ivykis], [0.20], [libivykis-discuss@lists.sourceforge.net])
AC_CONFIG_SRCDIR([lib/iv_main.c])
AC_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.
AC_SEARCH_LIBS([clock_gettime], [rt], [],
	AC_MSG_ERROR(clock_gettime is required to build ivykis.))
AC_SEARCH_LIBS([pthread_create], [pthread], [],
	AC_MSG_ERROR(pthreads support is required to build ivykis modules.))

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netinet/in.h stddef.h	\
		  stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h	\
		  sys/time.h sys/wait.h syslog.h unistd.h], [],
	AC_MSG_ERROR(some required header files not installed.))
AC_CHECK_HEADERS([sys/devpoll.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([clock_gettime dup2 fork malloc memcmp memmove memset	\
		select socket strdup strerror], [],
	AC_MSG_ERROR(some required library functions not available.))
AC_CHECK_FUNCS([epoll_create])
AC_CHECK_FUNCS([kqueue])

# Conditionals for poll methods.
AM_CONDITIONAL([HAVE_DEV_POLL], [test x$ac_cv_header_sys_devpoll_h = xyes])
AM_CONDITIONAL([HAVE_EPOLL], [test x$ac_cv_func_epoll_create = xyes])
AM_CONDITIONAL([HAVE_KQUEUE], [test x$ac_cv_func_kqueue = xyes])

AC_CONFIG_FILES([Makefile			\
		 contrib/Makefile		\
		 contrib/iv_inotify/Makefile	\
		 contrib/kojines/Makefile	\
		 contrib/splice/Makefile	\
		 ivykis.spec			\
		 lib/Makefile			\
		 lib/man3/Makefile		\
		 lib/test/Makefile		\
		 misc/Makefile			\
		 misc/ivykis.pc			\
		 misc/ivykis-modules.pc		\
		 modules/Makefile		\
		 modules/man3/Makefile		\
		 modules/test/Makefile])
AC_OUTPUT
