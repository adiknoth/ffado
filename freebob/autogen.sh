#!/bin/sh
# Run this to generate all the initial makefiles, etc.


autoreconf -f -i -s
./configure --enable-maintainer-mode
make
