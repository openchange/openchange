#!/bin/sh

test -d m4 || mkdir m4
aclocal
autoreconf --force --install
