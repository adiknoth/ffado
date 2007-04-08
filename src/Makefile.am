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
# you can redistribute and/or modify the library code
# under the terms of the GNU Lesser General Public License 
# version 2.1, as published by the Free Software Foundation
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

INCLUDES = 						\
	$(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) 	\
	$(LIBAVC1394_CFLAGS)	\
	$(LIBXMLCPP_CFLAGS)	$(LIBLO_CFLAGS)			\
	-I$(top_srcdir) -I$(top_srcdir)/src

lib_LTLIBRARIES = libffado.la

libffado_la_LDFLAGS = 				\
	-version-info $(LT_VERSION) $(LIBRAW1394_LIBS) 	\
	$(LIBIEC61883_LIBS) $(LIBAVC1394_LIBS) 		\
	$(LIBXMLCPP_LIBS) $(LIBLO_LIBS) \
	-lpthread -lrt

libffado_la_LIBADD = -lrt

noinst_HEADERS = \
	libieee1394/configrom.h libieee1394/csr1212.h \
	libieee1394/ieee1394service.h libieee1394/ARMHandler.h \
	libieee1394/IEC61883.h \
	debugmodule/debugmodule.h \
	devicemanager.h fbtypes.h iavdevice.h threads.h bebob/bebob_avdevice.h \
	bebob/bebob_avdevice_subunit.h bebob/bebob_avplug.h bebob/bebob_dl_bcd.h bebob/bebob_dl_codes.h \
	bebob/bebob_dl_mgr.h bebob/bebob_functionblock.h bounce/bounce_avdevice.h bounce/bounce_slave_avdevice.h \
	bebob/GenericMixer.h \
	maudio/maudio_avdevice.h motu/motu_avdevice.h rme/rme_avdevice.h \
	metrichalo/mh_avdevice.h dice/dice_avdevice.h \
	libavc/avc_connect.h \
	libavc/avc_definitions.h libavc/avc_extended_cmd_generic.h \
	libavc/avc_extended_plug_info.h libavc/avc_extended_stream_format.h \
	libavc/avc_extended_subunit_info.h libavc/avc_function_block.h libavc/avc_generic.h \
	libavc/avc_plug_info.h libavc/avc_serialize.h libavc/avc_signal_source.h \
	libavc/avc_subunit_info.h libavc/avc_unit_info.h \
	libosc/OscArgument.h libosc/OscNode.h libosc/OscResponse.h libosc/OscServer.h libosc/OscMessage.h \
	libstreaming/AmdtpPort.h libstreaming/AmdtpPortInfo.h libstreaming/AmdtpStreamProcessor.h \
	libstreaming/AmdtpSlaveStreamProcessor.h \
	libstreaming/cip.h libstreaming/cycletimer.h libstreaming/IsoHandler.h \
	libstreaming/IsoHandlerManager.h libstreaming/IsoStream.h libstreaming/MotuPort.h \
	libstreaming/MotuPortInfo.h libstreaming/MotuStreamProcessor.h \
	libstreaming/Port.h libstreaming/PortManager.h \
	libstreaming/StreamProcessor.h libstreaming/StreamProcessorManager.h \
	libutil/Atomic.h libutil/cycles.h libutil/DelayLockedLoop.h libutil/PosixThread.h \
	libutil/ringbuffer.h libutil/OptionContainer.h \
	libutil/PacketBuffer.h libutil/StreamStatistics.h \
	libutil/serialize.h libutil/SystemTimeSource.h libutil/Thread.h libutil/Time.h \
	libutil/TimeSource.h libutil/TimestampedBuffer.h

# common sources
libffado_la_SOURCES = \
	    devicemanager.cpp \
	    ffado.cpp \
	    ffado_streaming.cpp \
	    iavdevice.cpp \
	    debugmodule/debugmodule.cpp \
	    libavc/avc_connect.cpp \
	    libavc/avc_definitions.cpp \
	    libavc/avc_extended_cmd_generic.cpp \
	    libavc/avc_extended_plug_info.cpp \
	    libavc/avc_extended_stream_format.cpp \
	    libavc/avc_extended_subunit_info.cpp \
	    libavc/avc_function_block.cpp \
	    libavc/avc_generic.cpp \
	    libavc/avc_plug_info.cpp \
	    libavc/avc_serialize.cpp \
	    libavc/avc_signal_source.cpp \
	    libavc/avc_subunit_info.cpp \
	    libavc/avc_unit_info.cpp \
	    libieee1394/ARMHandler.cpp \
	    libieee1394/configrom.cpp \
	    libieee1394/csr1212.c \
	    libieee1394/ieee1394service.cpp \
	    libieee1394/IEC61883.cpp \
	    libosc/OscArgument.cpp \
	    libosc/OscMessage.cpp \
	    libosc/OscNode.cpp \
	    libosc/OscResponse.cpp \
	    libosc/OscServer.cpp \
	    libstreaming/cip.c \
	    libstreaming/IsoHandler.cpp \
	    libstreaming/IsoHandlerManager.cpp \
	    libstreaming/IsoStream.cpp \
	    libstreaming/Port.cpp \
	    libstreaming/PortManager.cpp \
	    libstreaming/StreamProcessor.cpp \
	    libstreaming/StreamProcessorManager.cpp \
	    libutil/DelayLockedLoop.cpp \
	    libutil/PacketBuffer.cpp \
	    libutil/OptionContainer.cpp \
	    libutil/PosixThread.cpp \
	    libutil/ringbuffer.c \
	    libutil/serialize.cpp \
	    libutil/StreamStatistics.cpp \
	    libutil/SystemTimeSource.cpp \
	    libutil/Time.c \
	    libutil/TimeSource.cpp \
	    libutil/TimestampedBuffer.cpp

# class specific sources
bebob_src = \
	    bebob/bebob_avdevice.cpp \
	    bebob/bebob_avdevice_subunit.cpp \
	    bebob/bebob_avplug.cpp \
	    bebob/bebob_dl_bcd.cpp \
	    bebob/bebob_dl_codes.cpp \
	    bebob/bebob_dl_mgr.cpp \
	    bebob/bebob_functionblock.cpp \
	    bebob/GenericMixer.cpp \
	    maudio/maudio_avdevice.cpp

motu_src = \
	    motu/motu_avdevice.cpp \
	    libstreaming/MotuPort.cpp \
	    libstreaming/MotuPortInfo.cpp \
	    libstreaming/MotuStreamProcessor.cpp

dice_src = \
	    dice/dice_avdevice.cpp

bounce_src = \
	    bounce/bounce_avdevice.cpp \
	    bounce/bounce_slave_avdevice.cpp \
	    libstreaming/AmdtpSlaveStreamProcessor.cpp

metric_halo_src = \
	    metrichalo/mh_avdevice.cpp

rme_src = \
	    rme/rme_avdevice.cpp

amdtp_src = \
	    libstreaming/AmdtpPort.cpp \
	    libstreaming/AmdtpPortInfo.cpp \
	    libstreaming/AmdtpStreamProcessor.cpp

# data
#  generic
nobase_dist_pkgdata_DATA =
#  bebob specific
bebob_pkgdata = 				\
	maudio/refdesign.xml				\
	maudio/fw410.xml 				\
	maudio/fwap.xml

# programs intended for installation in the 'bin' directory
#  generic
bin_PROGRAMS = 

#  class specific
bebob_bin = bridgeco-downloader bebob-sync

bridgeco_downloader_SOURCES = bridgeco-downloader.cpp
bridgeco_downloader_LDADD = $(top_builddir)/src/libffado.la

bebob_sync_SOURCES = bebob-sync.cpp
bebob_sync_LDADD = $(top_builddir)/src/libffado.la

# programs not intended for installation
# generic
noinst_PROGRAMS = test-dll test-debugmodule \
	              test-unittests-util test-unittests-osc \
	              test-ser

test_dll_SOURCES = libutil/test-dll.cpp
test_dll_LDADD = $(top_builddir)/src/libffado.la

test_debugmodule_SOURCES = debugmodule/test_debugmodule.cpp
test_debugmodule_LDADD = $(top_builddir)/src/libffado.la

test_unittests_util_SOURCES = libutil/unittests.cpp
test_unittests_util_LDADD = $(top_builddir)/src/libffado.la

test_unittests_osc_SOURCES = libosc/unittests.cpp
test_unittests_osc_LDADD = $(top_builddir)/src/libffado.la

test_ser_SOURCES = ser.cpp
test_ser_LDADD = $(top_builddir)/src/libffado.la

# build customisation

if BUILD_BEBOB
libffado_la_SOURCES += $(bebob_src)
bin_PROGRAMS += $(bebob_bin)
noinst_PROGRAMS += $(bebob_noinst)
nobase_dist_pkgdata_DATA += $(bebob_pkgdata)
endif

if BUILD_BOUNCE
libffado_la_SOURCES += $(bounce_src)
bin_PROGRAMS += $(bounce_bin)
noinst_PROGRAMS += $(bounce_noinst)
nobase_dist_pkgdata_DATA += $(bounce_pkgdata)
endif

if BUILD_DICE
libffado_la_SOURCES += $(dice_src)
bin_PROGRAMS += $(dice_bin)
noinst_PROGRAMS += $(dice_noinst)
nobase_dist_pkgdata_DATA += $(dice_pkgdata)
endif

if BUILD_MOTU
libffado_la_SOURCES += $(motu_src)
bin_PROGRAMS += $(motu_bin)
noinst_PROGRAMS += $(motu_noinst)
nobase_dist_pkgdata_DATA += $(motu_pkgdata)
endif

if BUILD_METRIC_HALO
libffado_la_SOURCES += $(metric_halo_src)
bin_PROGRAMS += $(metric_halo_bin)
noinst_PROGRAMS += $(metric_halo_noinst)
nobase_dist_pkgdata_DATA += $(metric_halo_pkgdata)
endif

if BUILD_RME
libffado_la_SOURCES += $(rme_src)
bin_PROGRAMS += $(rme_bin)
noinst_PROGRAMS += $(rme_noinst)
nobase_dist_pkgdata_DATA += $(rme_pkgdata)
endif

if BUILD_AMDTP
libffado_la_SOURCES += $(amdtp_src)
bin_PROGRAMS += $(amdtp_bin)
noinst_PROGRAMS += $(amdtp_noinst)
nobase_dist_pkgdata_DATA += $(amdtp_pkgdata)
endif