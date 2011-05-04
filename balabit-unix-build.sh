#!/bin/sh

cmd=$1
shift
case "$cmd" in
  get-version)
    echo 0.20-sng33ref9
    ;;
  build-exclude-list|dist-exclude-list|prepare-dist)
    echo "out autom4te.cache"
    ;;
  bootstrap)
    sh ./autogen.sh
    ;;
  configure)
    CFLAGS="-g -O2" LDFLAGS="-g" ./configure $@
    ;;
  make)
    make $@
    ;;
  *)
    echo "Unknown command: $cmd"
    exit 1
    ;;
esac

# vim: ts=2 sw=2 expandtab
