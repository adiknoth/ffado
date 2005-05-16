# Makefile.am - Makefile template
# Copyright (C) 2004 by Daniel Wagner.
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


INCLUDES = \
        $(LIBSIGC_CFLAGS) $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) $(LIBAVC_CFLAGS) $(LIBLO_CFLAGS) $(LIBXML_CFLAGS)

bin_PROGRAMS = freebob

freebob_SOURCES = \
	cmhandler.h  \
	cmhandler.cpp \
	freebob.h \
	streamprocess.h  \
	streamprocess.cpp \
	ieee1394service.h \
	ieee1394service.cpp \
	avdevice.h \
	avdevice.cpp \
	avdevicesubunit.h \
	avdevicesubunit.cpp \
	avdeviceaudiosubunit.h \
	avdeviceaudiosubunit.cpp \
	avdevicemusicsubunit.h \
	avdevicemusicsubunit.cpp \
	avdevicepool.h \
	avdevicepool.cpp \
	threads.h \
	workerthread.h \
	workerthread.cpp \
	avdescriptor.h \
	avdescriptor.cpp \
	avmusicidentifierdescriptor.h \
	avmusicidentifierdescriptor.cpp \
	avmusicstatusdescriptor.h \
	avmusicstatusdescriptor.cpp \
	avinfoblock.h \
	avinfoblock.cpp \
	avgeneralmusicstatusinfoblock.h \
	avgeneralmusicstatusinfoblock.cpp \
	avnameinfoblock.h \
	avnameinfoblock.cpp \
	avaudioinfoblock.h \
	avaudioinfoblock.cpp \
	avmidiinfoblock.h \
	avmidiinfoblock.cpp \
	avaudiosyncinfoblock.h \
	avaudiosyncinfoblock.cpp \
	avsourcepluginfoblock.h \
	avsourcepluginfoblock.cpp \
	avoutputplugstatusinfoblock.h \
	avoutputplugstatusinfoblock.cpp \
	avroutingstatusinfoblock.h \
	avroutingstatusinfoblock.cpp \
	avpluginfoblock.h \
	avpluginfoblock.cpp \
	avmusicpluginfoblock.h \
	avmusicpluginfoblock.cpp \
	avclusterinfoblock.h \
	avclusterinfoblock.cpp \
	ipchandler.cpp \
	ipchandler.h \
	debugmodule.cpp \
	main.cpp \
	csr1212.h \
	csr1212.c \
	configrom.h \
	configrom.cpp \
	avaudiosubunitidentifierdescriptor.h \
	avaudiosubunitidentifierdescriptor.cpp

freebob_LDFLAGS = \
	$(LIBSIGC_LIBS) $(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) $(LIBAVC_LIBS) $(LIBLO_LIBS) $(LIBXML_LIBS)
