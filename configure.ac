# configure.ac - Configure script for FreeBob.
# Copyright (C) 2005,06 by Daniel Wagner.
# Copyright (C) 2006    by Pieter Palmers.
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
m4_define(freebob_minor_version, 8)
m4_define(freebob_micro_version, 2)

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

AM_INIT_AUTOMAKE([subdir-objects])
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
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_CHECK_TOOL([AR], [ar], :)
AC_CHECK_TOOL([NM], [nm], :)
AC_PATH_PROG([SED], [sed], :)
AC_PATH_PROG([SORT], [sort], :)

# Required for building the documentation

# Checks for libraries.
AC_PROG_LIBTOOL

# Checks for typedefs, structures, and compiler characteristics.
AC_CANONICAL_HOST
AC_C_CONST
AC_C_VOLATILE
AC_TYPE_SIZE_T

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

AC_CHECK_LIB(pthread, pthread_create,,
[
        AC_ERROR(You need the pthread library to compile freebob)
])

# Check for dependencies
PKG_CHECK_MODULES(LIBRAW1394, libraw1394 >= 1.2.1)
PKG_CHECK_MODULES(LIBIEC61883, libiec61883 >= 1.1.0)
PKG_CHECK_MODULES(LIBAVC1394, libavc1394 >= 0.5.2)
PKG_CHECK_MODULES(ALSA, alsa >= 1.0.0)
PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= 2.6.0)

# Setup CFLAGS and CXXFLAGS
AC_ARG_ENABLE(debug,
[  --enable-debug    Turn on debugging],
[case "${enableval}" in
  yes) debug=true;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac],[debug=false])


dnl Check for MMX assembly

AC_ARG_ENABLE(mmx,
    [  --enable-mmx            enable MMX support (default=auto)],,
    enable_mmx=yes)

AC_ARG_ENABLE(sse,
    [  --enable-sse            enable SSE support (default=auto)],,
    enable_sse=$enable_mmx)

if test "x$enable_mmx" = xyes; then
    
    AC_MSG_CHECKING(whether we can compile MMX code)
    
    AC_COMPILE_IFELSE([asm ("movq 0, %mm0");],
	
	AC_DEFINE(USE_MMX, 1, [Define to 1 if MMX assembly is available.])
	AC_MSG_RESULT(yes)
	MMX_FLAGS="-mmmx -DENABLE_MMX"
	
	if test "x$enable_sse" = xyes; then
	      
	      AC_MSG_CHECKING(whether we can compile SSE code)
	      
	      AC_COMPILE_IFELSE([asm ("movntps %xmm0, 0");],
		  AC_DEFINE(USE_SSE, 1,
                            [Define to 1 if SSE assembly is available.])
		  SSE_FLAGS="-msse -mfpmath=sse -DENABLE_SSE"
		  AC_MSG_RESULT(yes)
		  ,
		  enable_sse=no
		  AC_MSG_RESULT(no)
		  AC_MSG_WARN(
                      [The assembler does not support the SSE command set.]
                  )
	      )
	      
	  fi
	  ,
	  enable_mmx=no
	  AC_MSG_RESULT(no)
	  AC_MSG_WARN([The assembler does not support the MMX command set.])
      )
  fi

  AC_DEFINE(x86, 1, "Nope it's intel")
  COMMON_X86_OPT_FLAGS="-DREENTRANT -O3 -fomit-frame-pointer -ffast-math -funroll-loops"

  if test "$target_cpu" = "i586"; then
      FREEBOB_OPT_CFLAGS="-march=i586 "
  elif test "$target_cpu" = "i686"; then
      FREEBOB_OPT_CFLAGS="-march=i686"
  else
      :
  fi

  FREEBOB_OPT_CFLAGS="$COMMON_X86_OPT_FLAGS $FREEBOB_OPT_CFLAGS $MMX_FLAGS $SSE_FLAGS"


AC_ARG_ENABLE(optimize,
    AC_HELP_STRING([--enable-optimize],
		   [ask the compiler for its best optimizations]),
    [ if test x$enable_optimize != xno ; then
	CFLAGS="$CFLAGS $FREEBOB_OPT_CFLAGS" ;
	CXXFLAGS="$CXXFLAGS $FREEBOB_OPT_CFLAGS"
      fi 
    ]
)


CFLAGS="$CFLAGS -Wall -std=gnu99"
CXXFLAGS="$CXXFLAGS -Wall"

if test "${debug}" = true; then
   CFLAGS="$CFLAGS -g -DDEBUG"
   CXXFLAGS="$CXXFLAGS -g -DDEBUG" 
fi;

AC_SUBST([CFLAGS])
AC_SUBST([CXXFLAGS])

# Configure components.
AC_ENABLE_STATIC(yes)
AC_ENABLE_SHARED(yes)

AC_SUBST([LIBSIGC_CFLAGS])
AC_SUBST([LIBSIGC_LIBS])
AC_SUBST([ALSA_CFLAGS])
AC_SUBST([ALSA_LIBS])
AC_SUBST([LIBRAW1394_CFLAGS])
AC_SUBST([LIBRAW1394_LIBS])
AC_SUBST([LIBIEC61883_CFLAGS])
AC_SUBST([LIBIEC61883_LIBS])
AC_SUBST([LIBAVC1394_CFLAGS])
AC_SUBST([LIBAVC1394_LIBS])
AC_SUBST([LIBXML_LIBS])
AC_SUBST([LIBXML_CFLAGS])

AC_CONFIG_FILES([Makefile
		 libfreebob/Makefile
		 src/debugmodule/Makefile
		 src/libfreebobavc/Makefile
		 src/libfreebobstreaming/Makefile
		 src/Makefile
		 tests/Makefile
		 tests/streaming/Makefile
		 libfreebob.pc])

AC_OUTPUT

# Print a summary so that important information isn't missed.
AC_MSG_RESULT([

FreeBob $VERSION is now configured for $canonical_host_type

  Source directory:     $srcdir
  Installation prefix:  $prefix
  C++ compiler:         $CXX $CXXFLAGS

  libraw1394 CFLAGS:    $LIBRAW1394_CFLAGS
  libraw1394 LIBS:      $LIBRAW1394_LIBS
  libiec61883 CFLAGS:   $LIBIEC61883_CFLAGS
  libiec61883 LIBS:     $LIBIEC61883_LIBS
  libavc1394 CFLAGS:    $LIBAVC1394_CFLAGS
  libavc1394 LIBS:      $LIBAVC1394_LIBS
])
