# Makefile.am - Makefile template
# Copyright (C) 2005 by Daniel Wagner.
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

AM_CFLAGS = -I@top_srcdir@libfreebob -I@top_builddir@/libfreebob $(LIBXML_CFLAGS) 

noinst_PROGRAMS = test-freebob test-extplugcmd
noinst_HEADERS = 

test_freebob_SOURCES = test-freebob.c
test_freebob_LDADD   = @top_builddir@src/libfreebob.la $(LIBXML_LIBS) 

test_extplugcmd_SOURCES = test-extplugcmd.cpp
test_extplugcmd_LDADD = @top_builddir@src/libfreebobavc/libfreebobavc.la

#TESTS_ENVIRONMENT
TEST = test-freebob
