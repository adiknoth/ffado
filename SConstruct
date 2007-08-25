#! /usr/bin/env python

import os
import sys
sys.path.append( "./admin" )
from pkgconfig import *

build_dir = ARGUMENTS.get('BUILDDIR', "")
if build_dir:
	build_base=build_dir+'/'
	if not os.path.isdir( build_base ):
		os.makedirs( build_base )
	print "Building into: " + build_base
else:
	build_base=''

if not os.path.isdir( "cache" ):
	os.makedirs( "cache" )

opts = Options( "cache/"+build_base+"options.cache" )

opts.Add( "BUILDDIR", "Path to place the built files in", "")

opts.AddOptions(
	BoolOption( "DEBUG", """
Toggle debug-build. DEBUG means \"-g -Wall\" and more, otherwise we will use
\"-O2\" to optimise.""", True ),
	PathOption( "PREFIX", "The prefix where ffado will be installed to.", "/usr/local" ),
	BoolOption( "ENABLE_BEBOB", "Enable/Disable the bebob part.", True ),
	BoolOption( "ENABLE_GENERIC_AVC", "Enable/Disable the generic avc part (apple).", True ),
	BoolOption( "ENABLE_MOTU", "Enable/Disable the Motu part.", False ),
	BoolOption( "ENABLE_DICE", "Enable/Disable the DICE part.", False ),
	BoolOption( "ENABLE_METRIC_HALO", "Enable/Disable the Metric Halo part.", False ),
	BoolOption( "ENABLE_RME", "Enable/Disable the RME part.", False ),
	BoolOption( "ENABLE_BOUNCE", "Enable/Disable the BOUNCE part.", False ),
	BoolOption( "ENABLE_ALL", "Enable/Disable support for all devices.", False ),
	BoolOption( "BUILD_TESTS", """
Build the tests in their directory. As some contain quite some functionality,
this is on by default.
If you just want to use ffado with jack without the tools, you can disable this.
""", True ),
	)

## Load the builders in config
env = Environment( tools=['default','scanreplace','pyuic'], toolpath=['admin'], ENV = { 'PATH' : os.environ['PATH'], 'PKG_CONFIG_PATH' : os.environ['PKG_CONFIG_PATH'] }, options=opts )

Help( """
For building ffado you can set different options as listed below. You have to
specify them only once, scons will save the last value you used and re-use
that.
To really undo your settings and return to the factory defaults, remove the
"cache"-folder and the file ".sconsign.dblite" from this directory.
For example with: "rm -Rf .sconsign.dblite cache"

Currently it seems as if only the BEBOB is kind-of-working, thats why only BEBOB
is enabled by default.

Note that this is a development version! Don't complain if its not working!
See www.ffado.org for stable releases.
""" )
Help( opts.GenerateHelpText( env ) )

# make sure the necessary dirs exist
if not os.path.isdir( "cache/" + build_base ):
	os.makedirs( "cache/" + build_base )
if not os.path.isdir( "cache/" + build_base + 'objects' ):
	os.makedirs( "cache/" + build_base + 'objects' )

CacheDir( 'cache/' + build_base + 'objects' )

opts.Save( 'cache/' + build_base + "options.cache", env )

if not env.GetOption('clean'):
	conf = Configure( env, custom_tests={ 'CheckForPKGConfig' : CheckForPKGConfig, 'CheckForPKG' : CheckForPKG }, conf_dir="cache/" + build_base, log_file="cache/" + build_base + 'config.log' )

	if not conf.CheckHeader( "stdio.h" ):
		print "It seems as if stdio.h is missing. This probably means that your build environment is broken, please make sure you have a working c-compiler and libstdc installed and usable."
		Exit( 1 )

	allpresent = 1;
	allpresent &= conf.CheckForPKGConfig();
	allpresent &= conf.CheckForPKG( 'libraw1394', "1.3.0" )
	allpresent &= conf.CheckForPKG( 'libavc1394', "0.5.3" )
	allpresent &= conf.CheckForPKG( 'libiec61883', "1.1.0" )
	allpresent &= conf.CheckForPKG( 'alsa', "1.0.0" )
	allpresent &= conf.CheckForPKG( 'libxml++-2.6', "2.13.0" )
	allpresent &= conf.CheckForPKG( 'liblo', "0.22" )
	allpresent &= conf.CheckForPKG( 'dbus-1', "1.0" )
	allpresent &= conf.CheckHeader( "expat.h" )
	allpresent &= conf.CheckLib( 'expat', 'XML_ExpatVersion', '#include <expat.h>' )
	
	if not allpresent:
		print """
(At least) One of the dependencies is missing. I can't go on without it, please
install the needed packages (remember to also install the *-devel packages)
"""
		Exit( 1 )

	env = conf.Finish()


if env['DEBUG']:
	print "Doing a debug build"
	# -Werror could be added to, which would force the devs to really remove all the warnings :-)
	env.AppendUnique( CCFLAGS=["-DDEBUG","-Wall","-g"] )
else:
	env.AppendUnique( CCFLAGS=["-O2"] )

if env['ENABLE_ALL']:
	env['ENABLE_BEBOB'] = True
	env['ENABLE_GENERIC_AVC'] = True
	env['ENABLE_MOTU'] = True
	env['ENABLE_DICE'] = True
	env['ENABLE_METRIC_HALO'] = True
	env['ENABLE_RME'] = True
	env['ENABLE_BOUNCE'] = True

if env['ENABLE_BEBOB']:
	env.AppendUnique( CCFLAGS=["-DENABLE_BEBOB"] )
if env['ENABLE_GENERIC_AVC']:
	env.AppendUnique( CCFLAGS=["-DENABLE_GENERIC_AVC"] )
if env['ENABLE_MOTU']:
	env.AppendUnique( CCFLAGS=["-DENABLE_MOTU"] )
if env['ENABLE_DICE']:
	env.AppendUnique( CCFLAGS=["-DENABLE_DICE"] )
if env['ENABLE_METRIC_HALO']:
	env.AppendUnique( CCFLAGS=["-DENABLE_METRIC_HALO"] )
if env['ENABLE_RME']:
	env.AppendUnique( CCFLAGS=["-DENABLE_RME"] )
if env['ENABLE_BOUNCE']:
	env.AppendUnique( CCFLAGS=["-DENABLE_BOUNCE"] )

env.MergeFlags( ["!pkg-config --cflags --libs libraw1394"] )
env.MergeFlags( ["!pkg-config --cflags --libs libavc1394"] )
env.MergeFlags( ["!pkg-config --cflags --libs libiec61883"] )
env.MergeFlags( ["!pkg-config --cflags --libs alsa"] )
env.MergeFlags( ["!pkg-config --cflags --libs libxml++-2.6"] )
env.MergeFlags( ["!pkg-config --cflags --libs liblo"] )


#
# Some includes in src/*/ are full path (src/*), that should be fixed?
env.AppendUnique( CPPPATH=["#/"] )

env['bindir'] = os.path.join( env['PREFIX'], "bin" )
env['libdir'] = os.path.join( env['PREFIX'], "lib" )
env['includedir'] = os.path.join( env['PREFIX'], "include" )
env['sharedir'] = os.path.join( env['PREFIX'], "share/libffado" )
env['cachedir'] = os.path.join( env['PREFIX'], "var/cache/libffado" )

env.Alias( "install", env['libdir'] )
env.Alias( "install", env['includedir'] )

env['PACKAGE'] = "libffado"
env['VERSION'] = "1.999.5"
env['LIBVERSION'] = "1.0.0"

#
# Start building
#

env.Command( "config.h.in", "config.h.in.scons", "cp $SOURCE $TARGET" )

env.ScanReplace( "config.h.in" )
pkgconfig = env.ScanReplace( "libffado.pc.in" )
env.Alias( "install", env.Install( env['libdir'] + '/pkgconfig', pkgconfig ) )


subdirs=['src','libffado','tests','support','external']
if build_base:
	env['build_base']="#/"+build_base
	for subdir in subdirs:
		env.SConscript( dirs=subdir, exports="env", build_dir=build_base+subdir )
else:
	env['build_base']="#/"
	env.SConscript( dirs=subdirs, exports="env" )


# By default only src is built but all is cleaned
if not env.GetOption('clean'):
	Default( 'external' )
	Default( 'src' )
	if env['BUILD_TESTS']:
		Default( 'tests' )
	#env.Alias( "install", env["cachedir"], os.makedirs( env["cachedir"] ) )
	env.Alias( "install", env.Install( env["cachedir"], "" ) ) #os.makedirs( env["cachedir"] ) )
