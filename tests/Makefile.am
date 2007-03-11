# Makefile.am - Makefile template
# Copyright (C) 2005,06 by Daniel Wagner.
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

# SUBDIRS = streaming

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src $(LIBXML_CFLAGS) \
	$(LIBAVC1394_CFLAGS) $(LIBXMLCPP_CFLAGS)
	
noinst_PROGRAMS =

if BUILD_TESTS
noinst_PROGRAMS += test-freebob test-extplugcmd test-fw410 \
 		test-volume test-mixer test-cycletimer test-sytmonitor \
 		test-timestampedbuffer test-ieee1394service
endif

noinst_HEADERS = 

test_freebob_SOURCES = test-freebob.cpp
test_freebob_LDADD   = $(top_builddir)/src/libfreebob.la \
	$(LIBXML_LIBS) $(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS)

test_extplugcmd_SOURCES = test-extplugcmd.cpp
test_extplugcmd_LDADD = $(top_builddir)/src/libfreebob.la \
	$(LIBAVC1394_LIBS)

test_volume_SOURCES = test-volume.cpp
test_volume_LDADD = $(top_builddir)/src/libfreebob.la \
	$(LIBAVC1394_LIBS)

test_mixer_SOURCES = test-mixer.cpp
test_mixer_LDADD = $(top_builddir)/src/libfreebob.la \
	$(LIBAVC1394_LIBS)

test_fw410_SOURCES = test-fw410.cpp
test_fw410_LDADD   = $(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS) -lrom1394

test_ieee1394service_SOURCES = test-ieee1394service.cpp
test_ieee1394service_LDADD   = $(top_builddir)/src/libfreebob.la \
                               $(LIBAVC1394_LIBS) $(LIBIEC61883_LIBS) -lrom1394

#TESTS_ENVIRONMENT
TEST = test-freebob

test_cycletimer_LDADD = $(top_builddir)/src/libfreebob.la $(LIBIEC61883_LIBS) \
	$(LIBRAW1394_LIBS) $(LIBAVC1394_LIBS)
test_cycletimer_SOURCES = test-cycletimer.cpp

test_sytmonitor_LDADD = $(top_builddir)/src/libfreebob.la $(LIBIEC61883_LIBS) \
	$(LIBRAW1394_LIBS) $(LIBAVC1394_LIBS)
test_sytmonitor_SOURCES = test-sytmonitor.cpp SytMonitor.cpp \
                          SytMonitor.h

test_timestampedbuffer_LDADD = $(top_builddir)/src/libfreebob.la $(LIBIEC61883_LIBS) \
	$(LIBRAW1394_LIBS) $(LIBAVC1394_LIBS)
test_timestampedbuffer_SOURCES = test-timestampedbuffer.cpp
