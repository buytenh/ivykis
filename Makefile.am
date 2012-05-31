SUBDIRS = lib modules . contrib lib/test modules/test misc

EXTRA_DIST = DEDICATION

lib_LTLIBRARIES = libivykis.la

libivykis_la_SOURCES =
libivykis_la_LIBADD = lib/libivykis.la modules/libivykis-modules.la
libivykis_la_LDFLAGS = -version-info 0:0:0

if HAVE_VERSIONING
libivykis_la_LDFLAGS += -Wl,--version-script,${srcdir}/libivykis.ver
endif
