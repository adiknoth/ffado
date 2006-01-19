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

SUBDIRS = debugmodule libfreebobavc

INCLUDES = $(LIBSIGC_CFLAGS) $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) \
	$(LIBAVC1394_CFLAGS) $(LIBLO_CFLAGS) $(LIBXML_CFLAGS) \
	-I$(top_srcdir) -I$(top_srcdir)/src

lib_LTLIBRARIES = libfreebob.la

libfreebob_la_SOURCES = 	\
	freebob.cpp 		\
	xmlparser.c		\
	devicemanager.h         \
	devicemanager.cpp       \
	avdevice.h              \
	avdevice.cpp		\
	avdevicesubunit.h	\
	avdevicesubunit.cpp	\
	avplug.h                \
	avplug.cpp		\
	configrom.h		\
	configrom.cpp		\
	csr1212.h		\
	csr1212.c		\
	fbtypes.h

libfreebob_la_LDFLAGS = -version-info $(LT_VERSION) -lpthread $(LIBSIGC_LIBS) \
	$(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) $(LIBAVC1394_LIBS) \
	$(LIBLO_LIBS) $(LIBXML_LIBS) debugmodule/libdebugmodule.la \
	libfreebobavc/libfreebobavc.la -safe-temps

