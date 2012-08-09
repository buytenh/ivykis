SUBDIRS = src . contrib man3 test misc

EXTRA_DIST = DEDICATION

lib_LTLIBRARIES = libivykis.la

libivykis_la_SOURCES =
libivykis_la_LIBADD = src/libivykis.la
libivykis_la_LDFLAGS = -version-info 2:1:2

if HAVE_VERSIONING
libivykis_la_LDFLAGS += -Wl,--version-script,${srcdir}/libivykis.ver
endif
