# Makefile.am - toplevel Makefile for FFADO
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

SUBDIRS = libffado src tests doc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libffado.pc

$(srcdir)/configure: configure.ac aclocal.m4
	cd $(srcdir) && autoconf

# autoheader might not change config.h.in, so touch a stamp file.
$(srcdir)/config.h.in: stamp-h.in
$(srcdir)/stamp-h.in: configure.ac aclocal.m4
	cd $(srcdir) && autoheader
	echo timestamp > $(srcdir)/stamp-h.in

config.h: stamp-h
stamp-h: config.h.in config.status
	./config.status

Makefile: Makefile.am config.status
	./config.status

config.status: configure
	./config.status --recheck
