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
        $(LIBSIGC_CFLAGS) $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) 

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
	avdevicepool.h \
	avdevicepool.cpp \
	avdescriptor.h \
	avdescriptor.cpp \
	debugmodule.cpp \
	main.cpp 

freebob_LDFLAGS = \
	$(LIBSIGC_LIBS) $(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) $(LIBAVC_LIBS)
