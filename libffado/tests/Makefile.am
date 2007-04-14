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

#SUBDIRS=streaming

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src $(LIBXML_CFLAGS) \
	$(LIBAVC1394_CFLAGS) $(LIBXMLCPP_CFLAGS)

noinst_PROGRAMS =

if BUILD_TESTS
noinst_PROGRAMS += test-ffado test-extplugcmd test-fw410 \
	     test-volume test-mixer test-cycletimer test-sytmonitor \
	     test-timestampedbuffer test-ieee1394service test-streamdump
endif

noinst_HEADERS =

test_ffado_SOURCES = test-ffado.cpp
test_ffado_LDADD   = $(top_builddir)/src/libffado.la \
	$(LIBXML_LIBS) $(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)

test_extplugcmd_SOURCES = test-extplugcmd.cpp
test_extplugcmd_LDADD = $(top_builddir)/src/libffado.la \
	$(LIBAVC1394_LIBS)

test_volume_SOURCES = test-volume.cpp
test_volume_LDADD = $(top_builddir)/src/libffado.la \
	$(LIBAVC1394_LIBS)

test_mixer_SOURCES = test-mixer.cpp
test_mixer_LDADD = $(top_builddir)/src/libffado.la \
	$(LIBAVC1394_LIBS)

test_fw410_SOURCES = test-fw410.cpp
test_fw410_LDADD   = $(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS) -lrom1394

test_streamdump_SOURCES = test-streamdump.cpp
test_streamdump_LDADD   = $(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS) -lrom1394

test_ieee1394service_SOURCES = test-ieee1394service.cpp
test_ieee1394service_LDADD   = $(top_builddir)/src/libffado.la \
	                           $(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS) -lrom1394

#TESTS_ENVIRONMENT
TEST = test-ffado

test_cycletimer_LDADD = $(top_builddir)/src/libffado.la $(LIBIEC61883_LIBS) \
	$(LIBRAW1394_LIBS) $(LIBAVC1394_LIBS)
test_cycletimer_SOURCES = test-cycletimer.cpp

test_sytmonitor_LDADD = $(top_builddir)/src/libffado.la $(LIBIEC61883_LIBS) \
	$(LIBRAW1394_LIBS) $(LIBAVC1394_LIBS)
test_sytmonitor_SOURCES = test-sytmonitor.cpp SytMonitor.cpp \
	                      SytMonitor.h

test_timestampedbuffer_LDADD = $(top_builddir)/src/libffado.la $(LIBIEC61883_LIBS) \
	$(LIBRAW1394_LIBS) $(LIBAVC1394_LIBS)
test_timestampedbuffer_SOURCES = test-timestampedbuffer.cpp
