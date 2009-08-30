#! /usr/bin/python
#
# Copyright (C) 2007-2008 Arnold Krille
# Copyright (C) 2007-2008 Pieter Palmers
# Copyright (C) 2008 Jonathan Woithe
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# FFADO is based upon FreeBoB.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

FFADO_API_VERSION="8"
FFADO_VERSION="2.999.0"

import os
import re
from string import Template
import imp
import distutils.sysconfig

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
  \"-O2\" to optimize.""", True ),
    BoolOption( "PROFILE", "Build with symbols and other profiling info", False ),
    PathOption( "PREFIX", "The prefix where ffado will be installed to.", "/usr/local", PathOption.PathAccept ),
    PathOption( "BINDIR", "Overwrite the directory where apps are installed to.", "$PREFIX/bin", PathOption.PathAccept ),
    PathOption( "LIBDIR", "Overwrite the directory where libs are installed to.", "$PREFIX/lib", PathOption.PathAccept ),
    PathOption( "INCLUDEDIR", "Overwrite the directory where headers are installed to.", "$PREFIX/include", PathOption.PathAccept ),
    PathOption( "SHAREDIR", "Overwrite the directory where misc shared files are installed to.", "$PREFIX/share/libffado", PathOption.PathAccept ),
    PathOption( "PYPKGDIR", "The directory where the python modules get installed.",
        distutils.sysconfig.get_python_lib( prefix="$PREFIX" ), PathOption.PathAccept ),
    BoolOption( "ENABLE_BEBOB", "Enable/Disable support for the BeBoB platform.", True ),
    BoolOption( "ENABLE_FIREWORKS", "Enable/Disable support for the ECHO Audio FireWorks platform.", True ),
    BoolOption( "ENABLE_OXFORD", "Enable/Disable support for the Oxford Semiconductor FW platform.", True ),
    BoolOption( "ENABLE_MOTU", "Enable/Disable support for the MOTU platform.", True ),
    BoolOption( "ENABLE_DICE", "Enable/Disable support for the TCAT DICE platform.", True ),
    BoolOption( "ENABLE_METRIC_HALO", "Enable/Disable support for the Metric Halo platform.", False ),
    BoolOption( "ENABLE_RME", "Enable/Disable support for the RME platform.", False ),
    BoolOption( "ENABLE_MAUDIO", "Enable/Disable support for the M-Audio custom BeBoB devices.", False ),
    BoolOption( "ENABLE_BOUNCE", "Enable/Disable the BOUNCE device.", False ),
    BoolOption( "ENABLE_GENERICAVC", """\
Enable/Disable the the generic avc part (mainly used by apple).
  Note that disabling this option might be overwritten by other devices needing
  this code.""", False ),
    BoolOption( "ENABLE_ALL", "Enable/Disable support for all devices.", False ),
    BoolOption( "SERIALIZE_USE_EXPAT", "Use libexpat for XML serialization.", False ),
    BoolOption( "BUILD_TESTS", """\
Build the tests in their directory. As some contain quite some functionality,
  this is on by default.
  If you just want to use ffado with jack without the tools, you can disable this.\
""", True ),
    BoolOption( "BUILD_STATIC_TOOLS", "Build a statically linked version of the FFADO tools.", False ),
    EnumOption('DIST_TARGET', 'Build target for cross compiling packagers', 'auto', allowed_values=('auto', 'i386', 'i686', 'x86_64', 'powerpc', 'powerpc64', 'none' ), ignorecase=2),
    BoolOption( "ENABLE_OPTIMIZATIONS", "Enable optimizations and the use of processor specific extentions (MMX/SSE/...).", False ),
    BoolOption( "PEDANTIC", "Enable -Werror and more pedantic options during compile.", False ),

    )

## Load the builders in config
buildenv=os.environ
vars_to_check = [
    'PATH',
    'PKG_CONFIG_PATH',
    'LD_LIBRARY_PATH',
    'XDG_CONFIG_DIRS',
    'XDG_DATA_DIRS',
    'HOME',
    'CC',
    'CFLAGS',
    'CXX',
    'CXXFLAGS',
    'CPPFLAGS',
]
for var in vars_to_check:
    if os.environ.has_key(var):
        buildenv[var]=os.environ[var]
    else:
        buildenv[var]=''

env = Environment( tools=['default','scanreplace','pyuic','pyuic4','dbus','doxygen','pkgconfig'], toolpath=['admin'], ENV = buildenv, options=opts )

if os.environ.has_key('LDFLAGS'):
    env['LINKFLAGS'] = os.environ['LDFLAGS']

# grab OS CFLAGS / CCFLAGS
env['OS_CFLAGS']=[]
if os.environ.has_key('CFLAGS'):
    env['OS_CFLAGS'] = os.environ['CFLAGS']
env['OS_CCFLAGS']=[]
if os.environ.has_key('CCFLAGS'):
    env['OS_CCFLAGS'] = os.environ['CCFLAGS']

Help( """
For building ffado you can set different options as listed below. You have to
specify them only once, scons will save the last value you used and re-use
that.
To really undo your settings and return to the factory defaults, remove the
"cache"-folder and the file ".sconsign.dblite" from this directory.
For example with: "rm -Rf .sconsign.dblite cache"

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

def ConfigGuess( context ):
    context.Message( "Trying to find the system triple: " )
    ret = os.popen( "admin/config.guess" ).read()[:-1]
    context.Result( ret )
    return ret

def CheckForApp( context, app ):
    context.Message( "Checking whether '" + app + "' executes " )
    ret = context.TryAction( app )
    context.Result( ret[0] )
    return ret[0]

def CheckForPyModule( context, module ):
    context.Message( "Checking for the python module '" + module + "' " )
    ret = context.TryAction( "python $SOURCE", "import %s" % module, ".py" )
    context.Result( ret[0] )
    return ret[0]

def CompilerCheck( context ):
    context.Message( "Checking for a working C-compiler " )
    ret = context.TryRun( """
#include <stdlib.h>

int main() {
    printf( "Hello World!" );
    return 0;
}""", '.c' )[0]
    context.Result( ret )
    if ret == 0:
        return False;
    context.Message( "Checking for a working C++-compiler " )
    ret = context.TryRun( """
#include <iostream>

int main() {
    std::cout << "Hello World!" << std::endl;
    return 0;
}""", ".cpp" )[0]
    context.Result( ret )
    return ret

tests = {
    "ConfigGuess" : ConfigGuess,
    "CheckForApp" : CheckForApp,
    "CheckForPyModule": CheckForPyModule,
    "CompilerCheck" : CompilerCheck,
}
tests.update( env['PKGCONFIG_TESTS'] )
tests.update( env['PYUIC_TESTS'] )
tests.update( env['PYUIC4_TESTS'] )

conf = Configure( env,
    custom_tests = tests,
    conf_dir = "cache/" + build_base,
    log_file = "cache/" + build_base + 'config.log' )

if env['SERIALIZE_USE_EXPAT']:
    env['SERIALIZE_USE_EXPAT']=1
else:
    env['SERIALIZE_USE_EXPAT']=0

if env['ENABLE_BOUNCE'] or env['ENABLE_ALL']:
    env['REQUIRE_LIBAVC']=1
else:
    env['REQUIRE_LIBAVC']=0

if not env.GetOption('clean'):
    #
    # Check for working gcc and g++ compilers and their environment.
    #
    if not conf.CompilerCheck():
        print "\nIt seems as if your system isn't even able to compile any C-/C++-programs. Probably you don't have gcc and g++ installed. Compiling a package from source without a working compiler is very hard to do, please install the needed packages.\nHint: on *ubuntu you need both gcc- and g++-packages installed, easiest solution is to install build-essential which depends on gcc and g++."
        Exit( 1 )

    # Check for pkg-config before using pkg-config to check for other dependencies.
    if not conf.CheckForPKGConfig():
        print "\nThe program 'pkg-config' could not be found.\nEither you have to install the corresponding package first or make sure that PATH points to the right directions."
        Exit( 1 )

    #
    # The following checks are for headers and libs and packages we need.
    #
    allpresent = 1;
    # for DBUS C++ bindings
    allpresent &= conf.CheckHeader( "expat.h" )
    allpresent &= conf.CheckLib( 'expat', 'XML_ExpatVersion', '#include <expat.h>' )
    
    pkgs = {
        'libraw1394' : '1.3.0',
        'libiec61883' : '1.1.0',
        'dbus-1' : '1.0',
        }

    if env['REQUIRE_LIBAVC']:
        pkgs['libavc1394'] = '0.5.3'

    if not env['SERIALIZE_USE_EXPAT']:
        pkgs['libxml++-2.6'] = '2.13.0'

    for pkg in pkgs:
        name2 = pkg.replace("+","").replace(".","").replace("-","").upper()
        env['%s_FLAGS' % name2] = conf.GetPKGFlags( pkg, pkgs[pkg] )
        #print '%s_FLAGS' % name2
        if env['%s_FLAGS'%name2] == 0:
            allpresent &= 0

    if not allpresent:
        print """
(At least) One of the dependencies is missing. I can't go on without it, please
install the needed packages for each of the lines saying "no".
(Remember to also install the *-devel packages!)

And remember to remove the cache with "rm -Rf .sconsign.dblite cache" so the
results above get rechecked.
"""
        Exit( 1 )

    # Check for C99 lrint() and lrintf() functions used to convert from
    # float to integer more efficiently via float_cast.h.  If not
    # present the standard slower methods will be used instead.  This
    # might not be the best way of testing for these but it's the only
    # way which seems to work properly.  CheckFunc() fails due to
    # argument count problems.
    if env.has_key( 'CFLAGS' ):
        oldcf = env['CFLAGS']
    else:
        oldcf = ""
    oldcf = env.Append(CFLAGS = '-std=c99')
    if conf.CheckLibWithHeader( "m", "math.h", "c", "lrint(3.2);" ):
        HAVE_LRINT = 1
    else:
        HAVE_LRINT = 0
    if conf.CheckLibWithHeader( "m", "math.h", "c", "lrintf(3.2);" ):
        HAVE_LRINTF = 1
    else:
        HAVE_LRINTF = 0
    env['HAVE_LRINT'] = HAVE_LRINT;
    env['HAVE_LRINTF'] = HAVE_LRINTF;
    env.Replace(CFLAGS=oldcf)

#
# Optional checks follow:
#

# PyQT checks
build_mixer = False
if conf.CheckForApp( 'which pyuic4' ) and conf.CheckForPyModule( 'dbus' ) and conf.CheckForPyModule( 'PyQt4' ) and conf.CheckForPyModule( 'dbus.mainloop.qt' ):
    env['PYUIC4'] = True
    build_mixer = True

if conf.CheckForApp( 'xdg-desktop-menu --help' ):
    env['XDG_TOOLS'] = True
else:
    print """
I couldn't find the program 'xdg-desktop-menu'. Together with xdg-icon-resource
this is needed to add the fancy entry to your menu. But if the mixer will be
installed, you can start it by executing "ffado-mixer".
"""

if not build_mixer and not env.GetOption('clean'):
    print """
I couldn't find all the prerequisites ('pyuic4' and the python-modules 'dbus'
and 'PyQt4', the packages could be named like dbus-python and PyQt) to build the
mixer.
Therefor the qt4 mixer will not get installed.
"""

# ALSA checks
pkg = 'alsa'
name2 = pkg.replace("+","").replace(".","").replace("-","").upper()
env['%s_FLAGS' % name2] = conf.GetPKGFlags( pkg, '1.0.0' )

#
# Get the directory where dbus stores the service-files
#
env['dbus_service_dir'] = conf.GetPKGVariable( 'dbus-1', 'session_bus_services_dir' ).strip()

config_guess = conf.ConfigGuess()

env = conf.Finish()

if env['DEBUG']:
    print "Doing a DEBUG build"
    env.MergeFlags( "-DDEBUG -Wall -g" )
else:
    env.MergeFlags( "-O2 -DNDEBUG" )

if env['PROFILE']:
    print "Doing a PROFILE build"
    env.MergeFlags( "-Wall -g" )

if env['PEDANTIC']:
    env.MergeFlags( "-Werror" )


# this is required to indicate that the DBUS version we use has support
# for platform dependent threading init functions
# this is true for DBUS >= 0.96 or so. Since we require >= 1.0 it is
# always true
env.MergeFlags( "-DDBUS_HAS_THREADS_INIT_DEFAULT" )

if env['ENABLE_ALL']:
    env['ENABLE_BEBOB'] = True
    env['ENABLE_FIREWORKS'] = True
    env['ENABLE_OXFORD'] = True
    env['ENABLE_MOTU'] = True
    env['ENABLE_DICE'] = True
    env['ENABLE_METRIC_HALO'] = True
    env['ENABLE_RME'] = True
    env['ENABLE_BOUNCE'] = True
    env['ENABLE_MAUDIO'] = True

if env['ENABLE_BEBOB'] or env['ENABLE_DICE'] \
   or env['ENABLE_BOUNCE'] or env['ENABLE_FIREWORKS'] \
   or env['ENABLE_OXFORD'] or env['ENABLE_MAUDIO']:
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
# Get the DESTDIR (if wanted) from the commandline
#
env.destdir = ARGUMENTS.get( 'DESTDIR', "" )

#
# Uppercase variables are for usage in code, lowercase versions for usage in
# scons-files for installing.
#
env['BINDIR'] = Template( env['BINDIR'] ).safe_substitute( env )
env['LIBDIR'] = Template( env['LIBDIR'] ).safe_substitute( env )
env['INCLUDEDIR'] = Template( env['INCLUDEDIR'] ).safe_substitute( env )
env['SHAREDIR'] = Template( env['SHAREDIR'] ).safe_substitute( env )
env['prefix'] = Template( env.destdir + env['PREFIX'] ).safe_substitute( env )
env['bindir'] = Template( env.destdir + env['BINDIR'] ).safe_substitute( env )
env['libdir'] = Template( env.destdir + env['LIBDIR'] ).safe_substitute( env )
env['includedir'] = Template( env.destdir + env['INCLUDEDIR'] ).safe_substitute( env )
env['sharedir'] = Template( env.destdir + env['SHAREDIR'] ).safe_substitute( env )
env['pypkgdir'] = Template( env.destdir + env['PYPKGDIR'] ).safe_substitute( env )
env['PYPKGDIR'] = Template( env['PYPKGDIR'] ).safe_substitute( env )

env.Command( target=env['sharedir'], source="", action=Mkdir( env['sharedir'] ) )

env.Alias( "install", env['libdir'] )
env.Alias( "install", env['includedir'] )
env.Alias( "install", env['sharedir'] )
env.Alias( "install", env['bindir'] )
env.Alias( "install", env['pypkgdir'] )

#
# shamelessly copied from the Ardour scons file
#

opt_flags = []
env['USE_SSE'] = 0

# guess at the platform, used to define compiler flags

config_cpu = 0
config_arch = 1
config_kernel = 2
config_os = 3
config = config_guess.split ("-")

needs_fPIC = False

# Autodetect
if env['DIST_TARGET'] == 'auto':
    if re.search ("x86_64", config[config_cpu]) != None:
        env['DIST_TARGET'] = 'x86_64'
    elif re.search("i[0-5]86", config[config_cpu]) != None:
        env['DIST_TARGET'] = 'i386'
    elif re.search("powerpc64", config[config_cpu]) != None:
        env['DIST_TARGET'] = 'powerpc64'
    elif re.search("powerpc", config[config_cpu]) != None:
        env['DIST_TARGET'] = 'powerpc'
    else:
        env['DIST_TARGET'] = 'i686'
    print "Detected DIST_TARGET = " + env['DIST_TARGET']

if ((re.search ("i[0-9]86", config[config_cpu]) != None) or (re.search ("x86_64", config[config_cpu]) != None) or (re.search ("powerpc", config[config_cpu]) != None)):
    
    build_host_supports_sse = 0
    build_host_supports_sse2 = 0
    build_host_supports_sse3 = 0

    if config[config_kernel] == 'linux' :
        
        if (env['DIST_TARGET'] == 'i686') or (env['DIST_TARGET'] == 'x86_64'):
            
            flag_line = os.popen ("cat /proc/cpuinfo | grep '^flags'").read()[:-1]
            x86_flags = flag_line.split (": ")[1:][0].split ()
            
            if "mmx" in x86_flags:
                opt_flags.append ("-mmmx")
            if "sse" in x86_flags:
                build_host_supports_sse = 1
            if "sse2" in x86_flags:
                build_host_supports_sse2 = 1
            #if "sse3" in x86_flags:
                #build_host_supports_sse3 = 1
            if "3dnow" in x86_flags:
                opt_flags.append ("-m3dnow")
            
            if config[config_cpu] == "i586":
                opt_flags.append ("-march=i586")
            elif config[config_cpu] == "i686":
                opt_flags.append ("-march=i686")

        elif (env['DIST_TARGET'] == 'powerpc') or (env['DIST_TARGET'] == 'powerpc64'):

            cpu_line = os.popen ("cat /proc/cpuinfo | grep '^cpu'").read()[:-1]

            ppc_type = cpu_line.split (": ")[1]
            if re.search ("altivec", ppc_type) != None:
                opt_flags.append ("-maltivec")
                opt_flags.append ("-mabi=altivec")

            ppc_type = ppc_type.split (", ")[0]
            if re.match ("74[0145][0578]A?", ppc_type) != None:
                opt_flags.append ("-mcpu=7400")
                opt_flags.append ("-mtune=7400")
            elif re.match ("750", ppc_type) != None:
                opt_flags.append ("-mcpu=750")
                opt_flags.append ("-mtune=750")
            elif re.match ("PPC970", ppc_type) != None:
                opt_flags.append ("-mcpu=970")
                opt_flags.append ("-mtune=970")
            elif re.match ("Cell Broadband Engine", ppc_type) != None:
                opt_flags.append ("-mcpu=cell")
                opt_flags.append ("-mtune=cell")

    if ((env['DIST_TARGET'] == 'i686') or (env['DIST_TARGET'] == 'x86_64')) \
       and build_host_supports_sse and env['ENABLE_OPTIMIZATIONS']:
        opt_flags.extend (["-msse", "-mfpmath=sse"])
        env['USE_SSE'] = 1

    if ((env['DIST_TARGET'] == 'i686') or (env['DIST_TARGET'] == 'x86_64')) \
       and build_host_supports_sse2 and env['ENABLE_OPTIMIZATIONS']:
        opt_flags.extend (["-msse2"])
        env['USE_SSE2'] = 1

    #if ((env['DIST_TARGET'] == 'i686') or (env['DIST_TARGET'] == 'x86_64')) \
       #and build_host_supports_sse2 and env['ENABLE_OPTIMIZATIONS']:
        #opt_flags.extend (["-msse3"])
        #env['USE_SSE3'] = 1

    # build for 64-bit userland?
    if env['DIST_TARGET'] == "powerpc64":
        print "Doing a 64-bit PowerPC build"
        env.MergeFlags( "-m64" )
    elif env['DIST_TARGET'] == "x86_64":
        print "Doing a 64-bit x86 build"
        env.MergeFlags( "-m64" )
        needs_fPIC = True
    else:
        print "Doing a 32-bit build"
    env.MergeFlags( "-m32" )

if needs_fPIC or '-fPIC' in env['OS_CFLAGS'] or "-fPIC" in env['OS_CCFLAGS']:
    env.MergeFlags( "-fPIC" )

# end of processor-specific section
if env['ENABLE_OPTIMIZATIONS']:
    opt_flags.extend (["-fomit-frame-pointer","-ffast-math","-funroll-loops"])
    env.MergeFlags( opt_flags )
    print "Doing an optimized build..."

env['REVISION'] = os.popen('svnversion .').read()[:-1]
# This may be as simple as '89' or as complex as '4123:4184M'.
# We'll just use the last bit.
env['REVISION'] = env['REVISION'].split(':')[-1]

# try to circumvent localized versions
if len(env['REVISION']) >= 5 and env['REVISION'][0:6] == 'export':
    env['REVISION'] = ''

env['FFADO_API_VERSION']=FFADO_API_VERSION

env['PACKAGE'] = "libffado"
env['VERSION'] = FFADO_VERSION
env['LIBVERSION'] = "1.0.0"

env['CONFIGDIR'] = "~/.ffado"
env['CACHEDIR'] = "~/.ffado"

env['USER_CONFIG_FILE'] = env['CONFIGDIR'] + "/configuration"
env['SYSTEM_CONFIG_FILE'] = env['SHAREDIR'] + "/configuration"

env['REGISTRATION_URL'] = "http://ffado.org/deviceregistration/register.php?action=register"

#
# To have the top_srcdir as the doxygen-script is used from auto*
#
env['top_srcdir'] = env.Dir( "." ).abspath

#
# Start building
#
env.ScanReplace( "config.h.in" )
env.ScanReplace( "config_debug.h.in" )
env.ScanReplace( "version.h.in" )

# ensure that the config.h is updated
env.Depends( "config.h", "SConstruct" )
env.Depends( "config.h", 'cache/' + build_base + "options.cache" )

# update version.h whenever the version or SVN revision changes
env.Depends( "version.h", env.Value(env['REVISION']))
env.Depends( "version.h", env.Value(env['VERSION']))

env.Depends( "libffado.pc", "SConstruct" )
pkgconfig = env.ScanReplace( "libffado.pc.in" )
env.Install( env['libdir'] + '/pkgconfig', pkgconfig )

env.Install( env['sharedir'], 'configuration' )

subdirs=['external','src','libffado','support','doc']
if env['BUILD_TESTS']:
    subdirs.append('tests')

if build_base:
    #env.SConscript( dirs=subdirs, exports="env", build_dir=build_base )
    builddirs = list()
    for dir in subdirs:
        builddirs.append( build_base + dir )
    env.SConscript( dirs=subdirs, exports="env", build_dir=builddirs )
else:
    env.SConscript( dirs=subdirs, exports="env" )

if 'debian' in COMMAND_LINE_TARGETS:
    env.SConscript("deb/SConscript", exports="env")

# By default only src is built but all is cleaned
if not env.GetOption('clean'):
    Default( 'src' )
    Default( 'support' )
    if env['BUILD_TESTS']:
        Default( 'tests' )

#
# Deal with the DESTDIR vs. xdg-tools conflict (which is basicely that the
# xdg-tools can't deal with DESTDIR, so the packagers have to deal with this
# their own :-/
#
if len(env.destdir) > 0:
    if not len( ARGUMENTS.get( "WILL_DEAL_WITH_XDG_MYSELF", "" ) ) > 0:
        print """
WARNING!
You are using the (packagers) option DESTDIR to install this package to a
different place than the real prefix. As the xdg-tools can't cope with
that, the .desktop-files are not installed by this build, you have to
deal with them your own.
(And you have to look into the SConstruct to learn how to disable this
message.)
"""
else:

    def CleanAction( action ):
        if env.GetOption( "clean" ):
            env.Execute( action )

    if env.has_key( 'XDG_TOOLS' ) and env.has_key( 'PYUIC4' ):
        if not env.GetOption("clean"):
            action = "install"
        else:
            action = "uninstall"
        mixerdesktopaction = env.Action( "xdg-desktop-menu %s support/xdg/ffado.org-ffadomixer.desktop" % action )
        mixericonaction = env.Action( "xdg-icon-resource %s --size 64 --novendor --context apps support/xdg/hi64-apps-ffado.png ffado" % action )
        env.Command( "__xdgstuff1", None, mixerdesktopaction )
        env.Command( "__xdgstuff2", None, mixericonaction )
        env.Alias( "install", ["__xdgstuff1", "__xdgstuff2" ] )
        CleanAction( mixerdesktopaction )
        CleanAction( mixericonaction )

#
# Create a tags-file for easier emacs/vim-source-browsing
#  I don't know if the dependency is right...
#
findcommand = "find . \( -path \"*.h\" -o -path \"*.cpp\" -o -path \"*.c\" \) \! -path \"*.svn*\" \! -path \"./doc*\" \! -path \"./cache*\""
env.Command( "tags", "", findcommand + " |xargs ctags" )
env.Command( "TAGS", "", findcommand + " |xargs etags" )
env.AlwaysBuild( "tags", "TAGS" )
if 'NoCache' in dir(env):
    env.NoCache( "tags", "TAGS" )

# Another convinience target
if env.GetOption( "clean" ):
    env.Execute( "rm cache/objects -Rf" )

#
# Temporary code to remove the installed qt3 mixer
#
import shutil
if os.path.exists( os.path.join( env['BINDIR'], "ffado-mixer-qt3" ) ):
    print "Removing the old qt3-mixer from the installation..."
    os.remove( os.path.join( env['BINDIR'], "ffado-mixer-qt3" ) )
if os.path.exists( os.path.join( env['SHAREDIR'], 'python-qt3' ) ):
    print "Removing the old qt3-mixer files from the installation..."
    shutil.rmtree( os.path.join( env['SHAREDIR'], 'python-qt3' ) )
if os.path.exists( os.path.join( env['SHAREDIR'], 'python' ) ):
    print "Removing the old qt3-mixer files from the installation..."
    shutil.rmtree( os.path.join( env['SHAREDIR'], 'python' ) )


#
# vim: ts=4 sw=4 et
