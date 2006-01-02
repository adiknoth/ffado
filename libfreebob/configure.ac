# configure.ac - Configure script for FreeBob.
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
#
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)

m4_define(freebob_major_version, 0)
m4_define(freebob_minor_version, 0)
m4_define(freebob_micro_version, 4)
m4_define(freebob_version, freebob_major_version.freebob_minor_version.freebob_micro_version)

AC_INIT(libfreebob, freebob_version, <freebob-devel@lists.sf.net>)

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LT_VERSION=1:0:1
AC_SUBST(LT_VERSION)

AC_DEFINE(FREEBOB_MAJOR_VERSION, freebob_major_version,[Freebob major version])
AC_DEFINE(FREEBOB_MINOR_VERSION, freebob_minor_version,[Freebob minor version])
AC_DEFINE(FREEBOB_MICRO_VERSION, freebob_micro_version,[Freebob micro version])
AC_SUBST(FREEBOB_MINOR_VERSION, freebob_minor_version)
AC_SUBST(FREEBOB_MAJOR_VERSION, freebob_major_version)
AC_SUBST(FREEBOB_MICRO_VERSION, freebob_micro_version)

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/freebob.cpp])
AC_CONFIG_HEADER([config.h])
AC_GNU_SOURCE
AC_LANG(C++)

# Prefix handling
AC_MSG_CHECKING(prefix)
defaultprefix=/usr/local
if test -z "$prefix" -o "$prefix" = "NONE"; then
	prefix=$defaultprefix
fi
AC_PREFIX_DEFAULT($prefix)
AC_MSG_RESULT($prefix)

maintainer_mode=${enable_maintainer_mode-no}

# Checks for programs.
# AC_PROG_CC and AC_PROG_CXX will add by default "-g -O2" if 
# CFLAGS and CXXFLAGS do not exists.  Disable this feature.
test ".$CFLAGS" = "." && CFLAGS=""
test ".$CXXFLAGS" = "." && CXXFLAGS=""
AC_PROG_CC
AC_PROG_CXX
AM_PROG_AS
AC_PROG_LIBTOOL
AC_CHECK_TOOL([AR], [ar], :)
AC_CHECK_TOOL([NM], [nm], :)
AC_PATH_PROG([SED], [sed], :)
AC_PATH_PROG([SORT], [sort], :)

# Required for building the documentation

# Checks for libraries.
AC_PROG_LIBTOOL

# XXX Debian specific hack:
# by Marcelo Magallon <mmagallo@efis.ucr.ac.cr>
# Turn around -rpath problem with libtool 1.0c
# This define should be improbable enough to not conflict with anything
case ${host} in
  *-pc-linux-gnu)
    AC_MSG_RESULT([Fixing libtool for -rpath problems.])
    sed < libtool > libtool-2 \
    's/^hardcode_libdir_flag_spec.*$/hardcode_libdir_flag_spec=" -D__LIBTOOL_IS_A_FOOL__ "/'
    mv libtool-2 libtool
    chmod 755 libtool
  ;;
esac

AC_CHECK_LIB(raw1394, raw1394_new_handle, ,
            [AC_MSG_ERROR([Unable to link with libraw1394. Check that you have libraw1394 installed])])

AC_CHECK_LIB(avc1394, avc1394_vcr_record, ,
             [AC_MSG_ERROR([Unable to link with libavc1394. Check that you have libavc1394 installed and that the version is 0.5.1 or newer])])

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
AC_CHECK_HEADER(execinfo.h, [AC_DEFINE(HAVE_EXECINFO_H, 1, Wheter or not execinfo.h is present)
	EXTRA_LIBS="$EXTRA_LIBS -rdynamic"])

# Check for libXML2
PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= 2.6.0)
AC_SUBST([LIBXML_LIBS])
AC_SUBST([LIBXML_CFLAGS])
	
# Checks for typedefs, structures, and compiler characteristics.
AC_CANONICAL_HOST
AC_C_CONST
AC_C_VOLATILE
AC_TYPE_SIZE_T

# Setup CFLAGS and CXXFLAGS
AC_ARG_ENABLE(debug,
[  --enable-debug    Turn on debugging],
[case "${enableval}" in
  yes) debug=true;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac],[debug=false])

CFLAGS="$CFLAGS -g -Wall -std=gnu99"
CXXFLAGS="$CXXFLAGS -g -Wall"

if test "${debug}" = true; then
   CFLAGS="$CFLAGS -DDEBUG"
   CXXFLAGS="$CXXFLAGS -DDEBUG" 
else
   CFLAGS="$CFLAGS -O2"
   CXXFLAGS="$CXXFLAGS -O2" 
fi;

AC_SUBST([CFLAGS])
AC_SUBST([CXXFLAGS])

# Configure components.
AC_ENABLE_STATIC(yes)
AC_ENABLE_SHARED(yes)

# Create links for header file.

# Checks for library functions.
PKG_CHECK_MODULES(LIBRAW1394, libraw1394 >= 1.2.0)
PKG_CHECK_MODULES(LIBIEC61883, libiec61883 >= 1.1.0)

PKG_CHECK_MODULES(ALSA, alsa >= 1.0.0)

AC_SUBST([LIBSIGC_CFLAGS])
AC_SUBST([LIBSIGC_LIBS])
AC_SUBST([LIBRAW1394_CFLAGS])
AC_SUBST([LIBRAW1394_LIBS])
AC_SUBST([LIBIEC61883_CFLAGS])
AC_SUBST([LIBIEC61883_LIBS])
AC_SUBST([ALSA_CFLAGS])
AC_SUBST([ALSA_LIBS])
LIBAVC_LIBS="-lrom1394 -lavc1394"
AC_SUBST([LIBAVC_LIBS])

AC_CONFIG_FILES([Makefile
		 libfreebob/Makefile
		 src/debugmodule/Makefile
		 src/libfreebobavc/Makefile
		 src/Makefile
		 tests/Makefile
		 libfreebob.pc])
AC_OUTPUT

# Print a summary so that important information isn't missed.
AC_MSG_RESULT([

FreeBob is now configured for $canonical_host_type

  Source directory:     $srcdir
  Installation prefix:  $prefix
  C++ compiler:         $CXX $CXXFLAGS
])
