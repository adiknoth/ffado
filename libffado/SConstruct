#
# Copyright (C) 2007 Arnold Krille
# Copyright (C) 2007 Pieter Palmers
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# FFADO is based upon FreeBoB.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import os
from string import Template

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
	BoolOption( "DEBUG", """\
Toggle debug-build. DEBUG means \"-g -Wall\" and more, otherwise we will use
  \"-O2\" to optimise.""", True ),
	PathOption( "PREFIX", "The prefix where ffado will be installed to.", "/usr/local", PathOption.PathAccept ),
	PathOption( "BINDIR", "Overwrite the directory where apps are installed to.", "$PREFIX/bin", PathOption.PathAccept ),
	PathOption( "LIBDIR", "Overwrite the directory where libs are installed to.", "$PREFIX/lib", PathOption.PathAccept ),
	PathOption( "INCLUDEDIR", "Overwrite the directory where headers are installed to.", "$PREFIX/include", PathOption.PathAccept ),
	PathOption( "SHAREDIR", "Overwrite the directory where misc shared files are installed to.", "$PREFIX/share/libffado", PathOption.PathAccept ),
	BoolOption( "ENABLE_BEBOB", "Enable/Disable the bebob part.", True ),
	BoolOption( "ENABLE_FIREWORKS", "Enable/Disable the ECHO Audio FireWorks avc part.", True ),
	BoolOption( "ENABLE_MOTU", "Enable/Disable the Motu part.", False ),
	BoolOption( "ENABLE_DICE", "Enable/Disable the DICE part.", False ),
	BoolOption( "ENABLE_METRIC_HALO", "Enable/Disable the Metric Halo part.", False ),
	BoolOption( "ENABLE_RME", "Enable/Disable the RME part.", False ),
	BoolOption( "ENABLE_BOUNCE", "Enable/Disable the BOUNCE part.", False ),
	BoolOption( "ENABLE_GENERICAVC", """\
Enable/Disable the the generic avc part (mainly used by apple).
  Note that disabling this option might be overwritten by other devices needing
  this code.""", False ),
	BoolOption( "ENABLE_ALL", "Enable/Disable support for all devices.", False ),
	BoolOption( "BUILD_TESTS", """\
Build the tests in their directory. As some contain quite some functionality,
  this is on by default.
  If you just want to use ffado with jack without the tools, you can disable this.\
""", True ),
    BoolOption( "BUILD_STATIC_TOOLS", "Build a statically linked version of the FFADO tools.", False ),
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

if os.environ.has_key('LD_LIBRARY_PATH'):
	buildenv['LD_LIBRARY_PATH']=os.environ['LD_LIBRARY_PATH']
else:
	buildenv['LD_LIBRARY_PATH']=''


env = Environment( tools=['default','scanreplace','pyuic','dbus','doxygen','pkgconfig'], toolpath=['admin'], ENV = buildenv, options=opts )


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


tests = {}
tests.update( env['PKGCONFIG_TESTS'] )

if not env.GetOption('clean'):
	conf = Configure( env,
		custom_tests = tests,
		conf_dir = "cache/" + build_base,
		log_file = "cache/" + build_base + 'config.log' )

	#
	# Check if the environment can actually compile c-files by checking for a
	# header shipped with gcc.
	#
	if not conf.CheckHeader( "stdio.h" ):
		print "It seems as if stdio.h is missing. This probably means that your build environment is broken, please make sure you have a working c-compiler and libstdc installed and usable."
		Exit( 1 )

	#
	# The following checks are for headers and libs and packages we need.
	#
	allpresent = 1;
	allpresent &= conf.CheckHeader( "expat.h" )
	allpresent &= conf.CheckLib( 'expat', 'XML_ExpatVersion', '#include <expat.h>' )
	
	allpresent &= conf.CheckForPKGConfig();

	pkgs = {
		'libraw1394' : '1.3.0',
		'libavc1394' : '0.5.3',
		'libiec61883' : '1.1.0',
		'alsa' : '1.0.0',
		'libxml++-2.6' : '2.13.0',
		'dbus-1' : '1.0',
		}
	for pkg in pkgs:
		name2 = pkg.replace("+","").replace(".","").replace("-","").upper()
		env['%s_FLAGS' % name2] = conf.GetPKGFlags( pkg, pkgs[pkg] )
		if env['%s_FLAGS'%name2] == 0:
			allpresent &= 0

	if not allpresent:
		print """
(At least) One of the dependencies is missing. I can't go on without it, please
install the needed packages (remember to also install the *-devel packages)
"""
		Exit( 1 )

	#
	# Optional checks follow:
	#
	env['ALSA_SEQ_OUTPUT'] = conf.CheckLib( 'asound', symbol='snd_seq_event_output_direct', autoadd=0 )

	env = conf.Finish()

if env['DEBUG']:
	print "Doing a DEBUG build"
	# -Werror could be added to, which would force the devs to really remove all the warnings :-)
	env.AppendUnique( CCFLAGS=["-DDEBUG","-Wall","-g"] ) # HACK!!
else:
	env.AppendUnique( CCFLAGS=["-O2","-DNDEBUG"] )

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

if env['ENABLE_BEBOB'] or env['ENABLE_DICE'] or env['ENABLE_BOUNCE'] or env['ENABLE_FIREWORKS']:
	env['ENABLE_GENERICAVC'] = True

env['BUILD_STATIC_LIB'] = False
if env['BUILD_STATIC_TOOLS']:
    print "Building static versions of the tools..."
    env['BUILD_STATIC_LIB'] = True

if build_base:
	env['build_base']="#/"+build_base
else:
	env['build_base']="#/"

#
# XXX: Maybe we can even drop these lower-case variables and only use the uppercase ones?
#
env['bindir'] = Template( os.path.join( env['BINDIR'] ) ).safe_substitute( env )
env['libdir'] = Template( os.path.join( env['LIBDIR'] ) ).safe_substitute( env )
env['includedir'] = Template( os.path.join( env['INCLUDEDIR'] ) ).safe_substitute( env )
env['sharedir'] = Template( os.path.join( env['SHAREDIR'] ) ).safe_substitute( env )

env.Command( target=env['sharedir'], source="", action=Mkdir( env['sharedir'] ) )

env.Alias( "install", env['libdir'] )
env.Alias( "install", env['includedir'] )
env.Alias( "install", env['sharedir'] )
env.Alias( "install", env['bindir'] )


env['REVISION'] = os.popen('svnversion .').read()[:-1]
# This may be as simple as '89' or as complex as '4123:4184M'.
# We'll just use the last bit.
env['REVISION'] = env['REVISION'].split(':')[-1]

if env['REVISION'] == 'exported':
	env['REVISION'] = ''

env['FFADO_API_VERSION']="5"

env['PACKAGE'] = "libffado"
env['VERSION'] = "1.999.9"
env['LIBVERSION'] = "1.0.0"

#
# To have the top_srcdir as the doxygen-script is used from auto*
#
env['top_srcdir'] = env.Dir( "." ).abspath

#
# Start building
#
env.ScanReplace( "config.h.in" )
# ensure that the config.h is updated with the version
NoCache("config.h")
AlwaysBuild("config.h")

pkgconfig = env.ScanReplace( "libffado.pc.in" )
env.Install( env['libdir'] + '/pkgconfig', pkgconfig )

subdirs=['external','src','libffado','tests','support','doc']
if build_base:
	env.SConscript( dirs=subdirs, exports="env", build_dir=build_base+subdir )
else:
	env.SConscript( dirs=subdirs, exports="env" )

if 'debian' in COMMAND_LINE_TARGETS:
	env.SConscript("deb/SConscript", exports="env")

# By default only src is built but all is cleaned
if not env.GetOption('clean'):
    Default( 'external' )
    Default( 'src' )
    Default( 'support' )
    if env['BUILD_TESTS']:
        Default( 'tests' )

