# Makefile.am - Makefile template
# Copyright (C) 2005,06 by Daniel Wagner.
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

INCLUDES = 						\
	$(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) 	\
	$(LIBAVC1394_CFLAGS) $(LIBXML_CFLAGS)      	\
	-I$(top_srcdir) -I$(top_srcdir)/src

lib_LTLIBRARIES = libfreebob.la
noinst_HEADERS = 				\
	configrom.h					\
	csr1212.h					\
	devicemanager.h         			\
	fbtypes.h					\
 	bounce/bounce_avdevice.h			\
 	motu/motu_avdevice.h				\
 	rme/rme_avdevice.h

libfreebob_la_SOURCES = 				\
	iavdevice.h					\
	configrom.cpp					\
	csr1212.c					\
	devicemanager.cpp       			\
	freebob.cpp 					\
	xmlparser.c					\
	threads.h					\
	bebob/bebob_avdevice.h          		\
	bebob/bebob_avdevice.cpp			\
	bebob/bebob_avdevice_xml.cpp			\
	bebob/bebob_avdevice_subunit.h			\
	bebob/bebob_avdevice_subunit.cpp		\
	bebob/bebob_avplug.h            		\
	bebob/bebob_avplug.cpp				\
	bebob/bebob_avplug_xml.cpp			\
	bebob/bebob_functionblock.h			\
	bebob/bebob_functionblock.cpp 			\
	bebob/bebob_dl_mgr.h				\
	bebob/bebob_dl_mgr.cpp				\
	bebob/bebob_dl_codes.h				\
	bebob/bebob_dl_codes.cpp			\
	bebob/bebob_dl_bcd.h				\
	bebob/bebob_dl_bcd.cpp				\
 	motu/motu_avdevice.cpp				\
 	motu/motu_avdevice.h				\
 	rme/rme_avdevice.cpp				\
 	rme/rme_avdevice.h				\
	bounce/bounce_avdevice.h			\
	bounce/bounce_avdevice.cpp			\
	maudio/maudio_avdevice.h			\
	maudio/maudio_avdevice.cpp			\
	libfreebobavc/avc_connect.cpp			\
	libfreebobavc/avc_definitions.cpp		\
	libfreebobavc/avc_extended_cmd_generic.cpp	\
	libfreebobavc/avc_extended_plug_info.cpp	\
	libfreebobavc/avc_extended_stream_format.cpp	\
	libfreebobavc/avc_extended_subunit_info.cpp	\
	libfreebobavc/avc_function_block.cpp		\
	libfreebobavc/avc_function_block.h		\
	libfreebobavc/avc_generic.cpp			\
	libfreebobavc/avc_plug_info.cpp			\
	libfreebobavc/avc_signal_source.cpp		\
	libfreebobavc/avc_subunit_info.cpp		\
	libfreebobavc/avc_unit_info.cpp			\
	libfreebobavc/ieee1394service.cpp		\
	libfreebobavc/serialize.cpp			\
	libfreebobavc/avc_connect.h			\
	libfreebobavc/avc_definitions.h			\
	libfreebobavc/avc_extended_cmd_generic.h	\
	libfreebobavc/avc_extended_plug_info.h		\
	libfreebobavc/avc_extended_stream_format.h	\
	libfreebobavc/avc_extended_subunit_info.h	\
	libfreebobavc/avc_generic.h			\
	libfreebobavc/avc_plug_info.h			\
	libfreebobavc/avc_signal_source.h		\
	libfreebobavc/avc_subunit_info.h		\
	libfreebobavc/avc_unit_info.h			\
	libfreebobavc/ieee1394service.h			\
	libfreebobavc/serialize.h			\
	debugmodule/debugmodule.h			\
	debugmodule/debugmodule.cpp			\
	libstreaming/cip.c	\
	libstreaming/freebob_streaming.cpp	\
	libstreaming/IsoHandler.cpp	\
	libstreaming/IsoHandlerManager.cpp	\
	libstreaming/IsoStream.cpp	\
	libstreaming/PacketBuffer.cpp	\
	libstreaming/PortManager.cpp	\
	libstreaming/Port.cpp	\
	libstreaming/StreamProcessor.cpp	\
	libstreaming/StreamProcessorManager.cpp	\
	libstreaming/AmdtpPortInfo.cpp	\
	libstreaming/AmdtpPort.cpp	\
	libstreaming/AmdtpStreamProcessor.cpp	\
	libstreaming/ringbuffer.c	\
	libstreaming/streamstatistics.cpp	\
	libstreaming/MotuStreamProcessor.cpp	\
	libstreaming/MotuPort.cpp	\
	libstreaming/MotuPortInfo.cpp \
	libutil/DelayLockedLoop.h 	\
	libutil/Atomic.h 	\
	libutil/PosixThread.h  	\
	libutil/Thread.h	\
	libutil/DelayLockedLoop.cpp	\
	libutil/PosixThread.cpp	

libfreebob_la_LDFLAGS = 				\
	-version-info $(LT_VERSION) $(LIBRAW1394_LIBS) 	\
	$(LIBIEC61883_LIBS) $(LIBAVC1394_LIBS) 		\
	$(LIBXML_LIBS) -lpthread

nobase_dist_pkgdata_DATA = 				\
	maudio/refdesign.xml				\
	maudio/fw410.xml

bin_PROGRAMS = freebob_downloader freebob_sync

freebob_downloader_SOURCES =    \
        freebob-downloader.cpp
freebob_downloader_LDADD =      \
        libfreebob.la           \
        $(LIBRAW1394_LIBS)      \
        $(LIBIEC61883_LIBS)     \
        $(LIBAVC1394_LIBS)	\
	-lpthread

freebob_sync_SOURCES =    \
        freebob-sync.cpp
freebob_sync_LDADD =      \
        libfreebob.la           \
        $(LIBRAW1394_LIBS)      \
        $(LIBIEC61883_LIBS)     \
        $(LIBAVC1394_LIBS)	\
	-lpthread

noinst_PROGRAMS = test-dll test-debugmodule

test_dll_SOURCES = libutil/test-dll.cpp
test_dll_LDADD = libfreebob.la

test_debugmodule_SOURCES = debugmodule/test_debugmodule.cpp
test_debugmodule_LDADD = libfreebob.la
