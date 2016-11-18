AC_PREREQ(2.59)
AC_INIT([ivykis-test-mt], [0.1], [libivykis-discuss@lists.sourceforge.net])
AC_CONFIG_SRCDIR([iv_event_bench.c])
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
	# On Tru64, we need -D_POSIX_PII_SOCKET so that
	# sys/socket.h defines 'socklen_t'.
	#
	CFLAGS="$CFLAGS -D_POSIX_PII_SOCKET"
	;;
esac

#
# Check for pthreads support.
#
# At least Tru64 needs -pthread, to avoid the following error when
# including pthread.h:
#
#	pthread.h:339:4: error: #error "Please compile the module
#	including pthread.h with -pthread"
#
ac_save_CFLAGS=$CFLAGS
ac_save_c_werror_flag=$ac_c_werror_flag
ac_c_werror_flag=yes
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
ac_c_werror_flag=$ac_save_c_werror_flag

#
# At least Solaris 11 for x86 needs building with -pthreads to
# avoid run-time breakage in multithreaded applications.
#
ac_save_CFLAGS=$CFLAGS
ac_save_c_werror_flag=$ac_c_werror_flag
ac_c_werror_flag=yes
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
ac_c_werror_flag=$ac_save_c_werror_flag

#
# We cannot use pthread_atfork() for our AC_SEARCH_LIBS() test, as on
# Linux is it provided by a library that does not provide the rest of
# the pthread functions (libpthread_nonshared.a), and on FreeBSD it is
# provided by libc.so itself.  Search for pthread_create() instead.
#
# On SCO OpenServer Release 6 and SCO UnixWare 7.1.4, the pthread
# functions appear to live in libthread, so check for that in
# addition to checking for libpthread.
#
# Tru64 uses mangled symbol names in its libpthread, and has some
# preprocessor magic in its pthread.h to do the same mangling, and
# so, testing for pthreads symbols without including pthread.h
# doesn't work.  Since AC_SEARCH_LIBS() doesn't allow specifying a
# header file to be included for tests (and on Tru64, we can't
# include pthread.h without additionally specifying -pthread), we'll
# check for __pthread_create if pthread_create does not exist.
#
# On HP-UX, libpthread uses mangled symbol names like on Tru64, but
# what's worse, the HP-UX C library provides stub versions of almost
# all libpthread functions that return errors instead of doing what
# they are supposed to, and there's no way to detect this at compile
# time.  However, there are a number of symbols in libpthread that
# the C library doesn't define stubs for, and we can test for one of
# those (pthread_trace_notify_np is one of those symbols).
#
AC_SEARCH_LIBS([pthread_create], [pthread thread], [],
	AC_SEARCH_LIBS([__pthread_create], [pthread], [],
	AC_SEARCH_LIBS([pthread_trace_notify_np], [pthread], [],
		AC_MSG_ERROR(pthreads support is required to build ivykis.))))

# Checks for libraries.
AC_SEARCH_LIBS([socket], [socket])

# Checks for library functions.
AC_CHECK_FUNCS([inotify_init])

fi


# Conditionals for OS type.
AM_CONDITIONAL([HAVE_POSIX], [test $ac_cv_host_system = posix])
AM_CONDITIONAL([HAVE_WIN32], [test $ac_cv_host_system = win32])

# Other conditionals.
AM_CONDITIONAL([HAVE_INOTIFY], [test x$ac_cv_func_inotify_init = xyes])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
