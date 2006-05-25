# Makefile.am - Makefile template
# Copyright (C) 2006 by Pieter Palmers.
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

noinst_LTLIBRARIES = libfreebobstreaming.la
noinst_HEADERS = 				\
	cip.h freebob_connections.h messagebuffer.h \
	ringbuffer.h freebob_streaming_private.h freebob_debug.h thread.h \
	watchdog.h handlers.h

libfreebobstreaming_la_SOURCES =  			\
	cip.c freebob_streaming.c messagebuffer.c \
	freebob_connections.c ringbuffer.c thread.c \
	watchdog.c handlers.c 

INCLUDES = $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) -I$(top_srcdir)/src \
	-I$(top_srcdir)
libfreebobstreaming_la_LIBADD  = $(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) $(LIBAVC_LIBS)

MAINTAINERCLEANFILES = Makefile.in
