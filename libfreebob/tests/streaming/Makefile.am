# Makefile.am - Makefile template
# Copyright (C) 2005 by Daniel Wagner.
# Copyright (C) 2006 by Pieter Palmers.
#
# This file is part of FreeBoB.
#
# FreeBoB is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# FreeBoB is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with FreeBoB; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src $(LIBIEC61883_CFLAGS) 

noinst_PROGRAMS = teststreaming teststreaming2 teststreaming3 testmidistreaming1
noinst_HEADERS = debugtools.h

teststreaming_SOURCES = teststreaming.c debugtools.c
teststreaming_LDADD   = $(top_builddir)/src/libfreebob.la \
	$(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)

teststreaming2_SOURCES = teststreaming2.c debugtools.c
teststreaming2_LDADD   = $(top_builddir)/src/libfreebob.la \
	$(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)

teststreaming3_SOURCES = teststreaming3.c debugtools.c
teststreaming3_LDADD   = $(top_builddir)/src/libfreebob.la \
	$(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)

testmidistreaming1_SOURCES = testmidistreaming1.c debugtools.c
testmidistreaming1_LDADD   = $(top_builddir)/src/libfreebob.la -lasound \
	$(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)
