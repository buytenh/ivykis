#!/bin/sh

CONFIGURE_OPTS="--disable-shared --enable-static --with-pic --disable-kqueue --disable-dev-poll"

echo $0

configure="`dirname $0`/`basename $0 .gnu`"
echo "Running: " $configure $@ $CONFIGURE_OPTS
$SHELL $configure "$@" $CONFIGURE_OPTS
