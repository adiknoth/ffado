# configure.ac - Configure script for FreeBob.
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
#
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(freebob, 0.0, wagi@monom.org)
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/freebob.cpp])
AC_CONFIG_HEADER([config.h])
AC_GNU_SOURCE

maintainer_mode=${enable_maintainer_mode-no}

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AM_PROG_AS
AC_PROG_RANLIB
AC_CHECK_TOOL([AR], [ar], :)
AC_CHECK_TOOL([NM], [nm], :)
AC_PATH_PROG([SED], [sed], :)
AC_PATH_PROG([SORT], [sort], :)

# Required for building the documentation

# Checks for libraries.
AC_CHECK_LIB(raw1394, raw1394_new_handle, ,
            [AC_MSG_ERROR([Unable to link with libraw1394. Check that you have libraw1394 installed])])

AC_CHECK_LIB(avc1394, avc1394_vcr_record, ,
             [AC_MSG_ERROR([Unable to link with libavc1394. Check that you have libavc1394 installed and that the version is 0.4.1 or newer])])

AC_CHECK_LIB(rom1394, rom1394_get_node_type, ,
             [AC_MSG_ERROR([Unable to link with librom1394. Check that your have librom1394 installed (usually part of libavc1394) and that the version is 0.4.1 or newer])])

AC_CHECK_LIB(pthread, pthread_create,,
[
	AC_ERROR(You need the pthread library to compile freebob)
])

# Checks for header files.
AC_CHECK_HEADERS([libraw1394/raw1394.h], ,
    [AC_MSG_ERROR([raw1394.h not found install libraw1394-devel])
])

AC_CHECK_HEADER([libavc1394/avc1394.h], ,
	[AC_MSG_ERROR([avc1394.h not found, install libavc1394-devel])
])
AC_CHECK_HEADER([libavc1394/rom1394.h], ,
	[AC_MSG_ERROR([rom1394.h not found, install libavc1394-devel])
])
AC_CHECK_HEADER(execinfo.h, [AC_DEFINE(HAVE_EXECINFO_H, 1, Wheter or not execinfo.h is present)
	EXTRA_LIBS="$EXTRA_LIBS -rdynamic"])

# Checks for typedefs, structures, and compiler characteristics.
AC_CANONICAL_HOST
AC_C_CONST
AC_C_VOLATILE
CFLAGS="$CFLAGS -Wall"
CXXFLAGS="$CXXFLAGS -Wall"

AC_SUBST([CFLAGS])
AC_SUBST([CXXFLAGS])

# Configure components.

# Create links for header file.

# Checks for library functions.
PKG_CHECK_MODULES(LIBSIGC, sigc++-2.0 >= 2.0.3)
PKG_CHECK_MODULES(LIBRAW1394, libraw1394 >= 0.10)
PKG_CHECK_MODULES(LIBIEC61883, libiec61883 >= 0.1.0)

AC_SUBST([LIBSIGC_CFLAGS])
AC_SUBST([LIBSIGC_LIBS])
AC_SUBST([LIBRAW1394_CFLAGS])
AC_SUBST([LIBRAW1394_LIBS])
AC_SUBST([LIBIEC61883_CFLAGS])
AC_SUBST([LIBIEC61883_LIBS])
LIBAVC_LIBS="-lrom1394 -lavc1394"
AC_SUBST([LIBAVC_LIBS])

AC_CONFIG_FILES([Makefile
		 src/Makefile])
AC_OUTPUT

# Print a summary so that important information isn't missed.
AC_MSG_RESULT([

FreeBob is now configured for $canonical_host_type

  Source directory:     $srcdir
  Installation prefix:  $prefix
  C compiler:           $CC $XTRA_CFLAGS $WARN_CFLAGS $CFLAGS
  C++ compiler:         $CXX $XTRA_CXXFLAGS $WARN_CXXFLAGS $CXXFLAGS
])
