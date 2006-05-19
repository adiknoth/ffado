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

noinst_LTLIBRARIES = libstreaming.la

libstreaming_la_SOURCES = 	\
	messagebuffer.c messagebuffer.h	\
	debug.h \
	FreebobPosixThread.cpp  FreebobPosixThread.h \
	FreebobThread.h \
	FreebobAtomic.h   \
	freebob_streaming.cpp \
	IsoHandler.cpp \
	IsoHandler.h \
	IsoHandlerManager.cpp \
	IsoHandlerManager.h \
	IsoStream.cpp \
	IsoStream.h \
	IsoStreamManager.cpp \
	IsoStreamManager.h \
	ringbuffer.c ringbuffer.h \
	Receiver.cpp Receiver.h \
	PacketBuffer.cpp PacketBuffer.h

INCLUDES = $(LIBRAW1394_CFLAGS) $(LIBIEC61883_CFLAGS) -I$(top_srcdir)/src \
	-I$(top_srcdir)
libstreaming_la_LIBADD  = $(LIBRAW1394_LIBS) $(LIBIEC61883_LIBS) $(LIBAVC_LIBS)

MAINTAINERCLEANFILES = Makefile.in
