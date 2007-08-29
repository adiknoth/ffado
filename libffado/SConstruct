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

#
# If this is just to display a help-text for the variable used via ARGUMENTS, then its wrong...
opts.Add( "BUILDDIR", "Path to place the built files in", "")

opts.AddOptions(
	BoolOption( "DEBUG", """
Toggle debug-build. DEBUG means \"-g -Wall\" and more, otherwise we will use
\"-O2\" to optimise.""", True ),
	PathOption( "PREFIX", "The prefix where ffado will be installed to.", "/usr/local" ),
	BoolOption( "ENABLE_BEBOB", "Enable/Disable the bebob part.", True ),
	BoolOption( "ENABLE_FIREWORKS", "Enable/Disable the ECHO Audio FireWorks avc part.", True ),
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
buildenv={}
if os.environ.has_key('PATH'):
	buildenv['PATH']=os.environ['PATH']
else:
	buildenv['PATH']=''

if os.environ.has_key('PKG_CONFIG_PATH'):
	buildenv['PKG_CONFIG_PATH']=os.environ['PKG_CONFIG_PATH']
else:
	buildenv['PKG_CONFIG_PATH']=''

env = Environment( tools=['default','scanreplace','pyuic','dbus'], toolpath=['admin'], ENV = buildenv, options=opts )

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
if not os.path.isdir( 'cache/objects' ):
	os.makedirs( 'cache/objects' )

CacheDir( 'cache/objects' )

opts.Save( 'cache/' + build_base + "options.cache", env )

#
# Check for apps...
#
def CheckForApp( context, app ):
	context.Message( "Checking if '%s' executes... " % app )
	ret = context.env.WhereIs( app )
	if ret != "":
		context.Result( True )
	else:
		context.Result( False )
	return ret


if not env.GetOption('clean'):
	conf = Configure( env,
		custom_tests={ 'CheckForPKGConfig' : CheckForPKGConfig, 'CheckForPKG' : CheckForPKG, 'CheckForApp' : CheckForApp },
		conf_dir="cache/" + build_base,
		log_file="cache/" + build_base + 'config.log' )

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

	#
	env['PYUIC'] = conf.CheckForApp( 'pyuic' )

	env = conf.Finish()

if env['DEBUG']:
	print "Doing a DEBUG build"
	# -Werror could be added to, which would force the devs to really remove all the warnings :-)
	env.AppendUnique( CCFLAGS=["-DDEBUG","-Wall","-g"] )
else:
	env.AppendUnique( CCFLAGS=["-O2"] )

# this is required to indicate that the DBUS version we use has support
# for platform dependent threading init functions
# this is true for DBUS >= 0.96 or so. Since we require >= 1.0 it is
# always true
env.AppendUnique( CCFLAGS=["-DDBUS_HAS_THREADS_INIT_DEFAULT"] )

if env['ENABLE_ALL']:
	env['ENABLE_BEBOB'] = True
	env['ENABLE_FIREWORKS'] = True
	env['ENABLE_MOTU'] = True
	env['ENABLE_DICE'] = True
	env['ENABLE_METRIC_HALO'] = True
	env['ENABLE_RME'] = True
	env['ENABLE_BOUNCE'] = True

if build_base:
	env['build_base']="#/"+build_base
else:
	env['build_base']="#/"

#
# Create an environment for the externals-directory without all the fancy
# ffado-defines. Probably the ffado-defines should be gathered in a distinct
# ffadoenv...
externalenv = env.Copy()
Export( 'externalenv' )

#
# TODO: Probably this should be in the src/SConscript...
#
if env['ENABLE_BEBOB']:
	env.AppendUnique( CCFLAGS=["-DENABLE_BEBOB"] )
if env['ENABLE_FIREWORKS']:
	env.AppendUnique( CCFLAGS=["-DENABLE_FIREWORKS"] )
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

# the GenericAVC code is used by these devices
if env['ENABLE_BEBOB'] or env['ENABLE_DICE'] or env['ENABLE_BOUNCE'] or env['ENABLE_FIREWORKS']:
	env.AppendUnique( CCFLAGS=["-DENABLE_GENERICAVC"] )

#
# TODO: Most of these flags aren't needed for all the apps/libs compiled here.
# The relevant MergeFlags-calls should be moved to the SConscript-files where
# its needed...
if env.has_key('LIBRAW1394_FLAGS'):
    env.MergeFlags( env['LIBRAW1394_FLAGS'] )
if env.has_key('LIBAVC1394_FLAGS'):
    env.MergeFlags( env['LIBAVC1394_FLAGS'] )
if env.has_key('LIBIEC61883_FLAGS'):
    env.MergeFlags( env['LIBIEC61883_FLAGS'] )
if env.has_key('ALSA_FLAGS'):
    env.MergeFlags( env['ALSA_FLAGS'] )
if env.has_key('LIBXML26_FLAGS'):
    env.MergeFlags( env['LIBXML26_FLAGS'] )
if env.has_key('LIBLO_FLAGS'):
    env.MergeFlags( env['LIBLO_FLAGS'] )

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

subdirs=['external','src','libffado','tests','support']
if build_base:
	env.SConscript( dirs=subdirs, exports="env", build_dir=build_base+subdir )
else:
	env.SConscript( dirs=subdirs, exports="env" )


# By default only src is built but all is cleaned
if not env.GetOption('clean'):
    Default( 'external' )
    Default( 'src' )
    if env['BUILD_TESTS']:
        Default( 'tests' )
    # Cachedir has to be fixed...
    #env.Alias( "install", env["cachedir"], os.makedirs( env["cachedir"] ) )
    #env.Alias( "install", env.Install( env["cachedir"], "" ) ) #os.makedirs( env["cachedir"] ) )
