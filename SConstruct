#! /usr/bin/env python

import os
import sys
sys.path.append( "./admin" )
from pkgconfig import *

if not os.path.isdir( "cache" ):
	os.mkdir( "cache" )

opts = Options( "cache/options.cache" )

opts.Add( BoolOption( "DEBUG", "Toggle debug-build.", True ) )
opts.AddOptions(
	BoolOption( "ENABLE_BEBOB", "Enable/Disable the bebob part.", True ),
	BoolOption( "ENABLE_MOTU", "Enable/Disable the Motu part.", False ),
	BoolOption( "ENABLE_DICE", "Enable/Disable the DICE part.", False ),
	BoolOption( "ENABLE_METRIC_HALO", "Enable/Disable the Metric Halo part.", False ),
	BoolOption( "ENABLE_RME", "Enable/Disable the RME part.", False ),
	BoolOption( "ENABLE_BOUNCE", "Enable/Disable the BOUNCE part.", False ),
	PathOption( "PREFIX", "The prefix where ffado will be installed to.", "/usr/local" ),
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

Currently it seems as if only the BEBOB and the MOTU drivers are
kind-of-working, thats why only BEBOB is enabled by default.

Note that this is a development version! Don't complain if its not working!
See www.ffado.org for stable releases.
""" )
Help( opts.GenerateHelpText( env ) )

opts.Save( "cache/options.cache", env )

env.Append( CCFLAGS = "-Wall -g -fpic" )

if not env.GetOption('clean'):
	conf = Configure( env, custom_tests={ 'CheckForPKGConfig' : CheckForPKGConfig, 'CheckForPKG' : CheckForPKG }, conf_dir='cache', log_file='cache/config.log' )

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

	if not allpresent:
		print """
(At least) One of the dependencies is missing. I can't go on without it, please
install the needed packages (remember to also install the *-devel packages)
"""
		Exit( 1 )

	env = conf.Finish()


if env['DEBUG']:
	env.AppendUnique( CCFLAGS=["-DDEBUG"] )

if env['ENABLE_BEBOB']:
	env.AppendUnique( CCFLAGS=["-DENABLE_BEBOB"] )
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

#env.AppendUnique( CCFLAGS = "-Wall -Werror -g -fpic" )
env.AppendUnique( CCFLAGS = env.Split("-Wall -g -fpic") )
#env.AppendUnique( LINKFLAGS = env.Split("-Wl,-rpath $libdir -Wl,soname -Wl,libffado.so.1") )


#
# Some includes in src/*/ are full path (src/*), that should be fixed?
env.AppendUnique( CPPPATH=["#/"] )

env['bindir'] = os.path.join( env['PREFIX'], "bin" )
env['libdir'] = os.path.join( env['PREFIX'], "lib" )
env['includedir'] = os.path.join( env['PREFIX'], "include" )
env['cachedir'] = os.path.join( env['PREFIX'] + "var/cache/libffado" )

env.Alias( "install", env['libdir'] )
env.Alias( "install", env['includedir'] )

env['PACKAGE'] = "libffado"
env['VERSION'] = "1.999.5"
env['LIBVERSION'] = "1.0.0"

#
# Start building
#

env.ScanReplace( "config.h.in" )
pkgconfig = env.ScanReplace( "libffado.pc.in" )
env.Alias( "install", env.Install( env['libdir'] + '/pkgconfig', pkgconfig ) )

if not os.path.exists( env['cachedir'] ):
	env.Alias( "install", os.makedirs( env["cachedir"] ) )

env.SConscript( dirs=['src','libffado','tests','support'], exports="env" )

# By default only src is built but all is cleaned
if not env.GetOption('clean'):
	Default( 'src' )

