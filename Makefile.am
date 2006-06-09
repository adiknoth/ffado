# Makefile.am - Makefile template
# Copyright (C) 2006 by Pieter Palmers.
#
# This file is part of FreeBob.
#
# FreeBob is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# FreeBob is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with FreeBob; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

noinst_LTLIBRARIES = libutil.la
noinst_HEADERS = DelayLockedLoop.h Atomic.h PosixThread.h  Thread.h

libutil_la_SOURCES = DelayLockedLoop.cpp  PosixThread.cpp

INCLUDES = -I$(top_srcdir)/src -lpthread
libutil_la_LIBADD  = 

MAINTAINERCLEANFILES = Makefile.in
noinst_PROGRAMS = test-dll
test_dll_SOURCES = test-dll.cpp
test_dll_LDADD = libutil.la
