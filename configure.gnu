#!/bin/sh

echo $0

configure="`dirname $0`/`basename $0 .gnu`"
echo "Running: " $configure $@ --disable-shared --enable-static --with-pic
$SHELL $configure "$@" --disable-shared --enable-static --with-pic
