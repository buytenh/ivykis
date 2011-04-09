dnl ***************************************************************************
dnl Initial setup for autoconf/automake/libtool
dnl ***************************************************************************

AC_PREREQ(2.59)
AC_INIT([ivykis], [0.21], [libivykis-discuss@lists.sourceforge.net])
AC_CONFIG_SRCDIR([lib/iv_main.c])
AC_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])

dnl ***************************************************************************
dnl Arguments
dnl ***************************************************************************

AC_ARG_ENABLE(epoll,
              [ --enable-epoll       Enable Linux specific epoll support (default: auto)],, enable_epoll="auto")
AC_ARG_ENABLE(devpoll,
              [ --enable-dev-poll    Enable Solaris specific /dev/poll support (default: auto)],, enable_dev_poll="auto")
AC_ARG_ENABLE(kqueue,
              [ --enable-kqueue      Enable FreeBSD specific kqueue support (default: auto)],, enable_kqueue="auto")

dnl ***************************************************************************
dnl Locate tools used by the build process
dnl ***************************************************************************

LT_INIT()
AC_PROG_CC

dnl ***************************************************************************
dnl Initial setup (set up defaults for CFLAGS, LDFLAGS, etc)
dnl ***************************************************************************
echo "Configuring for host: $host"

# FIXME: this would probably be solved by  using acx_pthreads autoconf macro, which'd detect this magic.
case $host in
    *solaris*)
	CFLAGS="$CFLAGS -D_REENTRANT -D_POSIX_C_SOURCE=199506L -D_POSIX_PTHREAD_SEMANTICS -D_XPG4_2"
        if test $host = "i386-pc-solaris2.9"; then
            LIBS="$LIBS -lthread"
        fi
	;;
    *-osf*)
	# force Tru64 to use pthreads.
	CFLAGS="$CFLAGS -pthread -D_OSF_SOURCE -D_XOPEN_SOURCE=500 -D_POSIX_C_SOURCE"
	;;
    *)
        ;;
esac

dnl ***************************************************************************
dnl Checks for external dependencies, system pecularities
dnl ***************************************************************************

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# system headers (required)
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netinet/in.h stddef.h \
        inttypes.h stdlib.h string.h sys/ioctl.h sys/socket.h \
        sys/time.h sys/wait.h syslog.h unistd.h],,
    AC_MSG_ERROR(some required header files not installed.))

# system headers (optional)
AC_CHECK_HEADERS([sys/select.h sys/devpoll.h sys/timers.h])

# system library functions (required)
AC_CHECK_FUNCS([dup2 fork malloc memcmp memmove memset select strerror],,
    AC_MSG_ERROR(some required library functions not available.))

# detect POSIX thread implementation
#
# FIXME: we should try to use acx_pthreads.m4 from the autoconf macro archive
#   http://ac-archive.sourceforge.net/ac-archive/acx_pthread.html
#
# AC_SEARCH_LIBS doesn't work properly on HP-UX 11.11, where pthread.h must be
# included, to link to the proper libs.
LIBS="$LIBS -lpthread"
AC_TRY_COMPILE([#include <pthread.h>],
               [pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;],,
               AC_MSG_ERROR(pthreads support is required to build ivykis modules.))


# whether we need -lrt
AC_SEARCH_LIBS([clock_gettime], [rt])

# whether we need -lsocket
AC_CHECK_FUNC(socket, [],
	AC_CHECK_LIB(socket, socket, LIBS="$LIBS -lsocket", AC_MSG_ERROR(socket(2) function missing))
	)


# functions that we could use if present
AC_CHECK_FUNCS([clock_gettime gettimeofday pthread_spin_lock kqueue epoll_create wait4])

for hdr in 'sys/timers.h' 'sys/time.h' 'time.h' 'pthread.h'; do
    AC_CHECK_MEMBER(struct timespec.tv_nsec,
                    [ac_cv_timespec_hdr=$hdr; break],
                    [unset ac_cv_member_struct_timespec_tv_nsec],
                    [[#include <$hdr>] AC_INCLUDES_DEFAULT])
done

# check if Thread Local Storage is present
# we should perhaps try
#
# http://ac-archive.sourceforge.net/guidod/ax_check_tls_support.html
#
# to do the same.
AC_LINK_IFELSE([AC_LANG_PROGRAM(
            [[#include <pthread.h>
                    __thread int a;
                    ]],
            [a=0;])],
    [ac_have_tls=yes],
    [ac_have_tls=no])


dnl ***************************************************************************
dnl whether to enable various features, misc checks.
dnl ***************************************************************************

if test -z "$ac_cv_func_clock_gettime"; then
	AC_MSG_NOTICE(clock_gettime missing, using less precise gettimeofday instead)
fi

if test "x$enable_epoll" = "xauto"; then
    enable_epoll="$ac_cv_func_epoll_create"
fi

dnl ***************************************************************************
dnl generate an installed config header (named iv_config.h) that can contain
dnl platform specific matters, that affect the API.
dnl ***************************************************************************

AC_CONFIG_COMMANDS([lib/include/iv_config.h],
    [
	outfile=iv_config.h.tmp
	cat >$outfile <<__EOF
/* This is an automatically generated file. Please modify 'configure.ac'.
 * It contains missing per-system specific definitions, needed for ivykis and
 * software using it.
 */

#ifndef IV_CONFIG_H
#define IV_CONFIG_H

__EOF

	if test "x$ac_cv_member_struct_timespec_tv_nsec" != "xyes"; then
		cat >> $outfile << __EOF
#include <sys/time.h>
/* Timespec declaration for systems lacking one. */
struct timespec {
	time_t   tv_sec;        /* seconds */
	long int    tv_nsec;       /* nanoseconds */
};
__EOF
        else
                cat >> $outfile << __EOF
#include <$ac_cv_timespec_hdr>
__EOF
	fi
cat >> $outfile << __EOF
#endif /* IV_CONFIG_H */
__EOF
	mv $outfile lib/include/iv_config.h
], [
        ac_cv_member_struct_timespec_tv_nsec=${ac_cv_member_struct_timespec_tv_nsec}
        ac_cv_timespec_hdr=${ac_cv_timespec_hdr}
])

# defines for config.h
AC_DEFINE_UNQUOTED([HAVE_TLS], [`test x$ac_have_tls = xyes && echo 1 || echo 0`], [Whether the __thread keyword should be used])
AC_DEFINE_UNQUOTED([ENABLE_DEV_POLL], [`test x$enable_devpoll = xyes && echo 1 || echo 0`], [Whether Solaris /dev/poll support should be compiled in])
AC_DEFINE_UNQUOTED([ENABLE_EPOLL], [`test x$enable_epoll = xyes && echo 1 || echo 0`], [Whether Linux epoll support should be compiled in])
AC_DEFINE_UNQUOTED([ENABLE_KQUEUE], [`test x$enable_kqueue = xyes && echo 1 || echo 0`], [Whether FreeBSD kqueue support should be compiled in])

# Conditionals for poll methods.
AM_CONDITIONAL([ENABLE_DEV_POLL], [test x$enable_devpoll = xyes])
AM_CONDITIONAL([ENABLE_EPOLL], [test x$enable_epoll = xyes])
AM_CONDITIONAL([ENABLE_KQUEUE], [test x$enable_kqueue = xyes])

AC_CONFIG_FILES([Makefile			\
		 contrib/Makefile		\
		 contrib/iv_inotify/Makefile	\
		 contrib/iv_openssl/Makefile	\
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
