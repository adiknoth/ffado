# configure.ac - Configure script for FFADO.
# Copyright (C) 2005,06,07 by Daniel Wagner.
# Copyright (C) 2006,07 by Pieter Palmers.
#
# This file is part of FFADO.
# FFADO is based upon FreeBoB.
#
# FFADO is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# FFADO is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with FFADO; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)


m4_define(ffado_major_version, 1)
m4_define(ffado_minor_version, 999)
m4_define(ffado_micro_version, 6)

m4_define(ffado_version, ffado_major_version.ffado_minor_version.ffado_micro_version)

AC_INIT(libffado, ffado_version, <ffado-devel@lists.sf.net>)

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A


LT_VERSION=1:0:0
AC_SUBST(LT_VERSION)

AC_DEFINE(FFADO_MAJOR_VERSION, ffado_major_version,[FFADO major version])
AC_DEFINE(FFADO_MINOR_VERSION, ffado_minor_version,[FFADO minor version])
AC_DEFINE(FFADO_MICRO_VERSION, ffado_micro_version,[FFADO micro version])
AC_SUBST(FFADO_MINOR_VERSION, ffado_minor_version)
AC_SUBST(FFADO_MAJOR_VERSION, ffado_major_version)
AC_SUBST(FFADO_MICRO_VERSION, ffado_micro_version)

AM_INIT_AUTOMAKE([subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/ffado.cpp])
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
        AC_ERROR(You need the pthread library to compile ffado)
])

# Check for dependencies
PKG_CHECK_MODULES(LIBRAW1394, libraw1394 >= 1.3.0)
PKG_CHECK_MODULES(LIBIEC61883, libiec61883 >= 1.1.0)
PKG_CHECK_MODULES(LIBAVC1394, libavc1394 >= 0.5.3)
PKG_CHECK_MODULES(ALSA, alsa >= 1.0.0)
PKG_CHECK_MODULES(LIBXMLCPP, libxml++-2.6 >= 2.14.0)
PKG_CHECK_MODULES(LIBLO, liblo >= 0.22)

# Setup CFLAGS and CXXFLAGS
AC_ARG_ENABLE(debug,
    AC_HELP_STRING([--enable-debug],
		   [Turn on debugging]),
[case "${enableval}" in
  yes) debug=true;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac],[debug=false])

dnl Check for MMX assembly

AC_ARG_ENABLE(mmx,
    AC_HELP_STRING([--enable-mmx],
		   [enable MMX support (default=auto)]),,
    enable_mmx=yes)

AC_ARG_ENABLE(sse,
    AC_HELP_STRING([--enable-sse],
		   [enable SSE support (default=auto)]),,
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
      FFADO_OPT_CFLAGS="-march=i586 "
  elif test "$target_cpu" = "i686"; then
      FFADO_OPT_CFLAGS="-march=i686"
  else
      :
  fi

  FFADO_OPT_CFLAGS="$COMMON_X86_OPT_FLAGS $FFADO_OPT_CFLAGS $MMX_FLAGS $SSE_FLAGS"


AC_ARG_ENABLE(optimize,
    AC_HELP_STRING([--enable-optimize],
		   [ask the compiler for its best optimizations]),
    [ if test x$enable_optimize != xno ; then
	CFLAGS="$CFLAGS $FFADO_OPT_CFLAGS" ;
	CXXFLAGS="$CXXFLAGS $FFADO_OPT_CFLAGS"
      fi 
    ]
)

dnl Conditional compilation of targets

dnl --- Build BeBoB code?

AC_ARG_ENABLE(bebob,
    AC_HELP_STRING([--enable-bebob],
		   [build BeBoB support (default=yes)]),
[case "${enableval}" in
  yes) build_bebob=true;;
  no)  build_bebob=false;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-bebob) ;;
esac],
[build_bebob=true])

dnl --- Build Generic AV/C code (Apple class driver compatible)?

AC_ARG_ENABLE(generic-avc,
    AC_HELP_STRING([--enable-generic-avc],
		   [build Generic AV/C support (default=yes)]),
[case "${enableval}" in
  yes) build_genericavc=true;;
  no)  build_genericavc=false;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-generic-avc) ;;
esac],
[build_genericavc=true])

dnl --- Build Motu code?

AC_ARG_ENABLE(motu,
    AC_HELP_STRING([--enable-motu],
		   [build Motu support (default=no)]),
[case "${enableval}" in
  yes) build_motu=true;;
  no)  build_motu=false;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-motu) ;;
esac],
[build_motu=false])

dnl --- Build DICE code?

AC_ARG_ENABLE(dice,
    AC_HELP_STRING([--enable-dice],
		   [build DICE support (default=no)]),
[case "${enableval}" in
  yes) build_dice=true;;
  no)  build_dice=false;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-dice) ;;
esac],
[build_dice=false])

dnl --- Build Metric Halo code?

AC_ARG_ENABLE(metric-halo,
    AC_HELP_STRING([--enable-metric-halo],
		   [build Metric Halo support (note: completely useless) (default=no)]),
[case "${enableval}" in
  yes) build_metric_halo=true;;
  no)  build_metric_halo=false;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-metric-halo) ;;
esac],
[build_metric_halo=false])

dnl --- Build Metric Halo code?

AC_ARG_ENABLE(rme,
    AC_HELP_STRING([--enable-rme],
		   [build RME support (note: completely useless) (default=no)]),
[case "${enableval}" in
  yes) build_rme=true;;
  no)  build_rme=false;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-rme) ;;
esac],
[build_rme=false])

dnl --- Build Bounce device code?

AC_ARG_ENABLE(bounce,
    AC_HELP_STRING([--enable-bounce],
		   [build Bounce device support (default=no)]),
[case "${enableval}" in
  yes) build_bounce=true;;
  no)  build_bounce=false;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-bounce) ;;
esac],
[build_bounce=false])

dnl --- Build support for all supported devices?

AC_ARG_ENABLE(all-devices,
    AC_HELP_STRING([--enable-all-devices],
		   [build support for all supported devices (default=no)]),
[case "${enableval}" in
  yes) build_all=true;;
  no)  build_all=false;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-all-devices) ;;
esac],
[build_all=false])

AC_ARG_ENABLE(tests,
    AC_HELP_STRING([--enable-tests],
		   [build programs in the tests directory (default=yes)]),
[case "${enableval}" in
  yes) build_tests=true;;
  no)  build_tests=false;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-tests) ;;
esac],
[build_tests=true])

CFLAGS="$CFLAGS -Wall -std=gnu99"
CXXFLAGS="$CXXFLAGS -Wall -DDATADIR=\\\"\$(datadir)\\\"  -DCACHEDIR=\\\"\$(localstatedir)\\\""
if test "${debug}" = true; then
   CFLAGS="$CFLAGS -g -DDEBUG"
   CXXFLAGS="$CXXFLAGS -g -DDEBUG" 
else 
   CFLAGS="$CFLAGS -O2"
   CXXFLAGS="$CXXFLAGS -O2"
fi;

dnl Device classes
if test "${build_all}" = true; then
    build_bebob=true
    build_genericavc=true
    build_motu=true
    build_dice=true
    build_metric_halo=true
    build_bounce=true
    build_rme=true
fi;

build_amdtp=false

if test "${build_bebob}" = true; then
   CFLAGS="$CFLAGS -DENABLE_BEBOB"
   CXXFLAGS="$CXXFLAGS -DENABLE_BEBOB" 
   supported_devices="${supported_devices}BeBoB  "
   build_amdtp=true
fi;

if test "${build_genericavc}" = true; then
   CFLAGS="$CFLAGS -DENABLE_GENERICAVC"
   CXXFLAGS="$CXXFLAGS -DENABLE_GENERICAVC" 
   supported_devices="${supported_devices}GenericAVC  "
   build_amdtp=true
fi;

if test "${build_motu}" = true; then
   CFLAGS="$CFLAGS -DENABLE_MOTU"
   CXXFLAGS="$CXXFLAGS -DENABLE_MOTU" 
   supported_devices="${supported_devices}Motu  "
fi;

if test "${build_dice}" = true; then
   CFLAGS="$CFLAGS -DENABLE_DICE"
   CXXFLAGS="$CXXFLAGS -DENABLE_DICE" 
   supported_devices="${supported_devices}DICE  "
   build_amdtp=true
fi;

if test "${build_metric_halo}" = true; then
   CFLAGS="$CFLAGS -DENABLE_METRIC_HALO"
   CXXFLAGS="$CXXFLAGS -DENABLE_METRIC_HALO"
   supported_devices="${supported_devices}Metric Halo (useless)  "
fi;

if test "${build_bounce}" = true; then
   CFLAGS="$CFLAGS -DENABLE_BOUNCE"
   CXXFLAGS="$CXXFLAGS -DENABLE_BOUNCE"
   supported_devices="${supported_devices}Bounce  "
   build_amdtp=true
fi;

if test "${build_rme}" = true; then
   CFLAGS="$CFLAGS -DENABLE_RME"
   CXXFLAGS="$CXXFLAGS -DENABLE_RME" 
   supported_devices="${supported_devices}RME (useless)  "
fi;

if test "${supported_devices}" = ""; then
    AC_ERROR(You need to enable at least one device class to support!)
fi;

AM_CONDITIONAL(BUILD_BEBOB,test "${build_bebob}" = true)
AM_CONDITIONAL(BUILD_GENERICAVC,test "${build_genericavc}" = true)
AM_CONDITIONAL(BUILD_MOTU,test "${build_motu}" = true)
AM_CONDITIONAL(BUILD_DICE,test "${build_dice}" = true)
AM_CONDITIONAL(BUILD_METRIC_HALO,test "${build_metric_halo}" = true)
AM_CONDITIONAL(BUILD_BOUNCE,test "${build_bounce}" = true)
AM_CONDITIONAL(BUILD_RME,test "${build_rme}" = true)

AM_CONDITIONAL(BUILD_AMDTP,test "${build_amdtp}" = true)

AM_CONDITIONAL(BUILD_TESTS,test "${build_tests}" = true)

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
AC_SUBST([LIBLO_LIBS])
AC_SUBST([LIBLO_CFLAGS])

AC_SUBST([BIN_PROGRAMS])

AC_CONFIG_FILES([Makefile
		 doc/Makefile
		 doc/reference.doxygen
		 libffado/Makefile
		 src/Makefile
		 tests/Makefile
		 tests/streaming/Makefile
		 libffado.pc])

AC_OUTPUT

# Print a summary so that important information isn't missed.
AC_MSG_RESULT([

FFADO $VERSION is now configured for $canonical_host_type

  Building support for: $supported_devices
   build AMDTP support: $build_amdtp
   
  Building tests:       $build_tests

  Source directory:     $srcdir
  Installation prefix:  $prefix
  C compiler:           $CC $CFLAGS
  C++ compiler:         $CXX $CXXFLAGS

  libraw1394 CFLAGS:    $LIBRAW1394_CFLAGS
  libraw1394 LIBS:      $LIBRAW1394_LIBS
  libiec61883 CFLAGS:   $LIBIEC61883_CFLAGS
  libiec61883 LIBS:     $LIBIEC61883_LIBS
  libavc1394 CFLAGS:    $LIBAVC1394_CFLAGS
  libavc1394 LIBS:      $LIBAVC1394_LIBS
])
