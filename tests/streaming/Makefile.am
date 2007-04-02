# Makefile.am
#
# Copyright (C) 2005-2007 by Daniel Wagner.
# Copyright (C) 2005-2007 by Pieter Palmers.
#
# This file is part of FFADO.
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# FFADO is based upon FreeBoB.
#
# FFADO is free software.
#
# you can redistribute and/or modify the applications
# under the terms of the GNU General Public License
# version 2, as published by the Free Software Foundation
#
# FFADO is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU (Lesser) General Public License for more details.
#
# You should have received a copy of the GNU (Lesser) General Public License
# along with FFADO; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src $(LIBIEC61883_CFLAGS) 

noinst_PROGRAMS = teststreaming teststreaming2 teststreaming3 testmidistreaming1
noinst_HEADERS = debugtools.h

teststreaming_SOURCES = teststreaming.c debugtools.c
teststreaming_LDADD   = $(top_builddir)/src/libffado.la \
	$(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)

teststreaming2_SOURCES = teststreaming2.c debugtools.c
teststreaming2_LDADD   = $(top_builddir)/src/libffado.la \
	$(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)

teststreaming3_SOURCES = teststreaming3.c debugtools.c
teststreaming3_LDADD   = $(top_builddir)/src/libffado.la \
	$(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)

testmidistreaming1_SOURCES = testmidistreaming1.c debugtools.c
testmidistreaming1_LDADD   = $(top_builddir)/src/libffado.la -lasound \
	$(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)
