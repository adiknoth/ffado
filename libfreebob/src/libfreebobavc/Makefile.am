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

noinst_LTLIBRARIES = libfreebobavc.la

libfreebobavc_la_SOURCES =  				\
		avc_connect.cpp 			\
		avc_connect.h 				\
		avc_definitions.cpp 			\
		avc_definitions.h 			\
		avc_extended_cmd_generic.cpp		\
		avc_extended_cmd_generic.h		\
		avc_extended_plug_info.cpp		\
		avc_extended_plug_info.h		\
		avc_extended_stream_format.cpp 		\
		avc_extended_stream_format.h 		\
		avc_extended_subunit_info.h		\
		avc_extended_subunit_info.cpp		\
		avc_generic.cpp 			\
		avc_generic.h 				\
		avc_plug_info.cpp 			\
		avc_plug_info.h 			\
		avc_signal_source.cpp 			\
		avc_signal_source.h 			\
		avc_subunit_info.cpp			\
		avc_subunit_info.h			\
		avc_unit_info.cpp			\
		avc_unit_info.h				\
		ieee1394service.cpp			\
		ieee1394service.h       		\
		serialize.cpp				\
		serialize.h

INCLUDES = $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) $(LIBAVC1394_CFLAGS) \
	-I$(top_srcdir)/src
libfreebobavc_la_LIBADD  = $(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) \
	$(LIBAVC1394_LIBS)

MAINTAINERCLEANFILES = Makefile.in
