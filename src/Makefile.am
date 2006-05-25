# Makefile.am - Makefile template
# Copyright (C) 2005,06 by Daniel Wagner.
# Copyright (C) 2006 by Pieter Palmers.
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

SUBDIRS = debugmodule libfreebobavc libstreaming

INCLUDES = $(LIBSIGC_CFLAGS) $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) \
	$(LIBAVC1394_CFLAGS) $(LIBLO_CFLAGS) $(LIBXML_CFLAGS) \
	-I$(top_srcdir) -I$(top_srcdir)/src

lib_LTLIBRARIES = libfreebob.la
noinst_HEADERS = 				\
	configrom.h					\
	csr1212.h					\
	devicemanager.h         			\
	fbtypes.h					\
	bebob_light/bebob_light_avdevice.h		\
	bebob_light/bebob_light_avdevicesubunit.h	
	bebob_light/bebob_light_avplug.h		\
# 	bebob/bebob_avdevice.h          		\
# 	bebob/bebob_avdevice_subunit.h			\
# 	bebob/bebob_avplug.h            		\
# 	bebob/bebob_functionblock.h			\
# 	bounce/bounce_avdevice.h			\
# 	motu/motu_avdevice.h				

libfreebob_la_SOURCES = 				\
	configrom.cpp					\
	csr1212.c					\
	devicemanager.cpp       			\
	freebob.cpp 					\
	xmlparser.c					\
	bebob_light/bebob_light_avdevice.cpp		\
	bebob_light/bebob_light_avdevicesubunit.cpp	\
	bebob_light/bebob_light_avplug.cpp		
# 	bebob/bebob_avdevice.cpp			\
# 	bebob/bebob_avdevice_xml.cpp			\
# 	bebob/bebob_avdevice_subunit.cpp		\
# 	bebob/bebob_avplug.cpp				\
# 	bebob/bebob_avplug_xml.cpp			\
# 	bebob/bebob_functionblock.cpp 			\
# 	bounce/bounce_avdevice.cpp			\
# 	motu/motu_avdevice.cpp

libfreebob_la_LDFLAGS = -version-info $(LT_VERSION) -lpthread $(LIBSIGC_LIBS) \
	$(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) $(LIBAVC1394_LIBS) \
	$(LIBLO_LIBS) $(LIBXML_LIBS) -safe-temps

libfreebob_la_LIBADD = debugmodule/libdebugmodule.la libfreebobavc/libfreebobavc.la \
	libstreaming/libstreaming.la 

