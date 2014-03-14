# -*- coding: utf-8 -*-
#
# Copyright (C) 2007, 2008, 2010 Arnold Krille
# Copyright (C) 2007, 2008 Pieter Palmers
# Copyright (C) 2008, 2012 Jonathan Woithe
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

FFADO_API_VERSION = "9"
FFADO_VERSION="2.1.9999"

from subprocess import Popen, PIPE
import os
import re
from string import Template
import imp
import distutils.sysconfig

if not os.path.isdir( "cache" ):
	os.makedirs( "cache" )

opts = Variables( "cache/options.cache" )

opts.AddVariables(
    BoolVariable( "DEBUG", """\
Toggle debug-build. DEBUG means \"-g -Wall\" and more, otherwise we will use
  \"-O2\" to optimize.""", True ),
    BoolVariable( "PROFILE", "Build with symbols and other profiling info", False ),
    PathVariable( "PREFIX", "The prefix where ffado will be installed to.", "/usr/local", PathVariable.PathAccept ),
    PathVariable( "BINDIR", "Overwrite the directory where apps are installed to.", "$PREFIX/bin", PathVariable.PathAccept ),
    PathVariable( "LIBDIR", "Overwrite the directory where libs are installed to.", "$PREFIX/lib", PathVariable.PathAccept ),
    PathVariable( "INCLUDEDIR", "Overwrite the directory where headers are installed to.", "$PREFIX/include", PathVariable.PathAccept ),
    PathVariable( "SHAREDIR", "Overwrite the directory where misc shared files are installed to.", "$PREFIX/share/libffado", PathVariable.PathAccept ),
    PathVariable( "MANDIR", "Overwrite the directory where manpages are installed", "$PREFIX/man", PathVariable.PathAccept ),
    PathVariable( "PYPKGDIR", "The directory where the python modules get installed.",
        distutils.sysconfig.get_python_lib( prefix="$PREFIX" ), PathVariable.PathAccept ),
    PathVariable( "UDEVDIR", "Overwrite the directory where udev rules are installed to.", "/lib/udev/rules.d/", PathVariable.PathAccept ),
    BoolVariable( "ENABLE_BEBOB", "Enable/Disable support for the BeBoB platform.", True ),
    BoolVariable( "ENABLE_FIREWORKS", "Enable/Disable support for the ECHO Audio FireWorks platform.", True ),
    BoolVariable( "ENABLE_OXFORD", "Enable/Disable support for the Oxford Semiconductor FW platform.", True ),
    BoolVariable( "ENABLE_MOTU", "Enable/Disable support for the MOTU platform.", True ),
    BoolVariable( "ENABLE_DICE", "Enable/Disable support for the TCAT DICE platform.", True ),
    BoolVariable( "ENABLE_METRIC_HALO", "Enable/Disable support for the Metric Halo platform.", False ),
    BoolVariable( "ENABLE_RME", "Enable/Disable support for the RME platform.", True ),
    BoolVariable( "ENABLE_DIGIDESIGN", "Enable/Disable support for Digidesign interfaces.", False ),
    BoolVariable( "ENABLE_BOUNCE", "Enable/Disable the BOUNCE device.", False ),
    BoolVariable( "ENABLE_GENERICAVC", """\
Enable/Disable the the generic avc part (mainly used by apple).
  Note that disabling this option might be overwritten by other devices needing
  this code.""", False ),
    BoolVariable( "ENABLE_ALL", "Enable/Disable support for all devices.", False ),
    BoolVariable( "SERIALIZE_USE_EXPAT", "Use libexpat for XML serialization.", False ),
    BoolVariable( "BUILD_TESTS", """\
Build the tests in their directory. As some contain quite some functionality,
  this is on by default.
  If you just want to use ffado with jack without the tools, you can disable this.\
""", True ),
    BoolVariable( "BUILD_STATIC_TOOLS", "Build a statically linked version of the FFADO tools.", False ),
    EnumVariable('DIST_TARGET', 'Build target for cross compiling packagers', 'auto', allowed_values=('auto', 'i386', 'i686', 'x86_64', 'powerpc', 'powerpc64', 'none' ), ignorecase=2),
    BoolVariable( "ENABLE_OPTIMIZATIONS", "Enable optimizations and the use of processor specific extentions (MMX/SSE/...).", False ),
    BoolVariable( "PEDANTIC", "Enable -Werror and more pedantic options during compile.", False ),
    ( "COMPILE_FLAGS", "Add additional flags to the environment.\nOnly meant for distributors and gentoo-users who want to over-optimize their built.\n Using this is not supported by the ffado-devs!" ),
    EnumVariable( "ENABLE_SETBUFFERSIZE_API_VER", "Report API version at runtime which includes support for dynamic buffer resizing (requires recent jack).", 'auto', allowed_values=('auto', 'true', 'false', 'force'), ignorecase=2),

    )

## Load the builders in config
buildenv=os.environ

env = Environment( tools=['default','scanreplace','pyuic','pyuic4','dbus','doxygen','pkgconfig'], toolpath=['admin'], ENV = buildenv, options=opts )

if env.has_key('COMPILE_FLAGS') and len(env['COMPILE_FLAGS']) > 0:
    print '''
 * Usage of additional flags is not supported by the ffado-devs.
 * Use at own risk!
 *
 * Currentl value is '%s'
 ''' % env['COMPILE_FLAGS']
    env.MergeFlags(env['COMPILE_FLAGS'])

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
if not os.path.isdir( "cache" ):
	os.makedirs( "cache" )
if not os.path.isdir( 'cache/objects' ):
    os.makedirs( 'cache/objects' )

CacheDir( 'cache/objects' )

opts.Save( 'cache/options.cache', env )

def ConfigGuess( context ):
    context.Message( "Trying to find the system triple: " )
    ret = os.popen( "/bin/sh admin/config.guess" ).read()[:-1]
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
#include <stdio.h>

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

def CheckPKG(context, name):
    context.Message( 'Checking for %s... ' % name )
    ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
    context.Result( ret )
    return ret

tests = {
    "ConfigGuess" : ConfigGuess,
    "CheckForApp" : CheckForApp,
    "CheckForPyModule": CheckForPyModule,
    "CompilerCheck" : CompilerCheck,
    "CheckPKG" : CheckPKG,
}
tests.update( env['PKGCONFIG_TESTS'] )
tests.update( env['PYUIC_TESTS'] )
tests.update( env['PYUIC4_TESTS'] )

conf = Configure( env,
    custom_tests = tests,
    conf_dir = "cache/",
    log_file = 'cache/config.log' )

version_re = re.compile(r'^(\d+)\.(\d+)\.(\d+)')

def VersionInt(vers):
    match = version_re.match(vers)
    if not match:
        return -1
    (maj, min, patch) = match.group(1, 2, 3)
    # For now allow "min" to run up to 65535.  "maj" and "patch" are 
    # restricted to 0-255.
    return (int(maj) << 24) | (int(min) << 8) | int(patch)

def CheckJackdVer():
    print 'Checking jackd version...',
    ret = Popen("which jackd >/dev/null 2>&1 && jackd --version | tail -n 1 | cut -d ' ' -f 3", shell=True, stdout=PIPE).stdout.read()[:-1]
    if (ret == ""):
        print "not installed"
        return -1
    else:
        print ret
    return VersionInt(ret)

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
    # for cache-serialization.
    if env['SERIALIZE_USE_EXPAT']:
        allpresent &= conf.CheckHeader( "expat.h" )
        allpresent &= conf.CheckLib( 'expat', 'XML_ExpatVersion', '#include <expat.h>' )

    pkgs = {
        'libraw1394' : '2.0.5',
        'libiec61883' : '1.1.0',
        'libconfig++' : '0'
        }

    if env['REQUIRE_LIBAVC']:
        pkgs['libavc1394'] = '0.5.3'

    if not env['SERIALIZE_USE_EXPAT']:
        pkgs['libxml++-2.6'] = '2.13.0'

    # Provide a way for users to compile newer libffado which will work 
    # against older jack installations which will not accept the new API
    # version reported at runtime.
    jackd_ver = CheckJackdVer()
    if (jackd_ver != -1):
        # If jackd is available, use the version number reported by it.  This
        # means users don't have to have jack development files present on
        # their system for this to work.
        have_jack = (jackd_ver >= VersionInt('0.0.0'))
        good_jack1 = (jackd_ver < VersionInt('1.9.0')) and (jackd_ver >= VersionInt('0.121.4'))
        good_jack2 = (jackd_ver >= VersionInt('1.9.9'))
    else:
        # Jackd is not runnable.  Attempt to identify a version from
        # pkgconfig on the off-chance jack details are available from there.
        print "Will retry jack detection using pkg-config"
        have_jack = conf.CheckPKG('jack >= 0.0.0')
        good_jack1 = conf.CheckPKG('jack < 1.9.0') and conf.CheckPKG('jack >= 0.122.0')
        good_jack2 = conf.CheckPKG('jack >= 1.9.9')
    if env['ENABLE_SETBUFFERSIZE_API_VER'] == 'auto':
        if not(have_jack):
            print """
No Jack Audio Connection Kit (JACK) installed: assuming a FFADO 
setbuffersize-compatible version will be used.
"""
        elif not(good_jack1 or good_jack2):
            FFADO_API_VERSION="8"
            print """
Installed Jack Audio Connection Kit (JACK) jack does not support FFADO 
setbuffersize API: will report earlier API version at runtime.  Consider 
upgrading to jack1 >=0.122.0 or jack2 >=1.9.9 at some point, and then 
recompile ffado to gain access to this added feature.
"""
        else:
            print "Installed Jack Audio Connection Kit (JACK) supports FFADO setbuffersize API"
    elif env['ENABLE_SETBUFFERSIZE_API_VER'] == 'true':
        if (have_jack and not(good_jack1) and not(good_jack2)):
            print """
SetBufferSize API version is enabled but no suitable version of Jack Audio 
Connection Kit (JACK) has been found.  The resulting FFADO would cause your 
jackd to abort with "incompatible FFADO version".  Please upgrade to 
jack1 >=0.122.0 or jack2 >=1.9.9, or set ENABLE_SETBUFFERSIZE_API_VER to "auto"
or "false".
"""
            # Although it's not strictly an error, in almost every case that 
            # this occurs the user will want to know about it and fix the
            # problem, so we exit so they're guaranteed of seeing the above
            # message.
            Exit( 1 )
        else:
            print "Will report SetBufferSize API version at runtime"
    elif env['ENABLE_SETBUFFERSIZE_API_VER'] == 'force':
        print "Will report SetBufferSize API version at runtime"
    else:
        FFADO_API_VERSION="8"
        print "Will not report SetBufferSize API version at runtime"

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

#
# Optional pkg-config
#
pkgs = {
    'alsa': '0',
    'dbus-1': '1.0',
    'dbus-c++-1' : '0',
    }
for pkg in pkgs:
    name2 = pkg.replace("+","").replace(".","").replace("-","").upper()
    env['%s_FLAGS' % name2] = conf.GetPKGFlags( pkg, pkgs[pkg] )

if not env['DBUS1_FLAGS'] or not env['DBUSC1_FLAGS'] or not conf.CheckForApp('which dbusxx-xml2cpp'):
    env['DBUS1_FLAGS'] = ""
    env['DBUSC1_FLAGS'] = ""
    print """
One of the dbus-headers, the dbus-c++-headers and/or the application
'dbusxx-xml2cpp' where not found. The dbus-server for ffado will therefore not
be built.
"""
else:
    # Get the directory where dbus stores the service-files
    env['dbus_service_dir'] = conf.GetPKGVariable( 'dbus-1', 'session_bus_services_dir' ).strip()
    # this is required to indicate that the DBUS version we use has support
    # for platform dependent threading init functions
    # this is true for DBUS >= 0.96 or so. Since we require >= 1.0 it is
    # always true
    env['DBUS1_FLAGS'] += " -DDBUS_HAS_THREADS_INIT_DEFAULT"


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


if env['ENABLE_ALL']:
    env['ENABLE_BEBOB'] = True
    env['ENABLE_FIREWORKS'] = True
    env['ENABLE_OXFORD'] = True
    env['ENABLE_MOTU'] = True
    env['ENABLE_DICE'] = True
    env['ENABLE_METRIC_HALO'] = True
    env['ENABLE_RME'] = True
    env['ENABLE_DIGIDESIGN'] = True
    env['ENABLE_BOUNCE'] = True


env['BUILD_STATIC_LIB'] = False
if env['BUILD_STATIC_TOOLS']:
    print "Building static versions of the tools..."
    env['BUILD_STATIC_LIB'] = True

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
env['UDEVDIR'] = Template( env['UDEVDIR'] ).safe_substitute( env )
env['prefix'] = Template( env.destdir + env['PREFIX'] ).safe_substitute( env )
env['bindir'] = Template( env.destdir + env['BINDIR'] ).safe_substitute( env )
env['libdir'] = Template( env.destdir + env['LIBDIR'] ).safe_substitute( env )
env['includedir'] = Template( env.destdir + env['INCLUDEDIR'] ).safe_substitute( env )
env['sharedir'] = Template( env.destdir + env['SHAREDIR'] ).safe_substitute( env )
env['mandir'] = Template( env.destdir + env['MANDIR'] ).safe_substitute( env )
env['pypkgdir'] = Template( env.destdir + env['PYPKGDIR'] ).safe_substitute( env )
env['udevdir'] = Template( env.destdir + env['UDEVDIR'] ).safe_substitute( env )
env['PYPKGDIR'] = Template( env['PYPKGDIR'] ).safe_substitute( env )

env.Command( target=env['sharedir'], source="", action=Mkdir( env['sharedir'] ) )

env.Alias( "install", env['libdir'] )
env.Alias( "install", env['includedir'] )
env.Alias( "install", env['sharedir'] )
env.Alias( "install", env['bindir'] )
env.Alias( "install", env['mandir'] )
if build_mixer:
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

#=== Begin Revised CXXFLAGS ========================================= 
def outputof(*cmd):
    """Run a command without running a shell, return cmd's stdout
    """
    p = Popen(cmd, stdout=PIPE)
    return p.communicate()[0]

def cpuinfo_kv():
    """generator which reads lines from Linux /proc/cpuinfo and splits them
    into key:value tokens and yields (key, value) tuple.
    """
    f = open('/proc/cpuinfo', 'r')
    for line in f:
        line = line.strip()
        if line:
            k,v = line.split(':')
            yield (k.strip(), v.strip())
    f.close()


class CpuInfo (object):
    """Collects information about the CPU, mainly from /proc/cpuinfo
    """
    def __init__(self):
        self.sysname, self.hostname, self.release, self.version, self.machine = os.uname()
        # general CPU architecture
        self.is_x86 = self.machine in ('i686', 'x86_64') or \
                      re.match("i[3-5]86", self.machine) or False
        self.is_powerpc = self.machine in ('ppc64', 'ppc', 'powerpc', 'powerpc64')
        #!!! probably not comprehensive
        self.is_mips = self.machine == 'mips'
        #!!! not a comprehensive list. uname -m on one android phone reports 'armv71'
        # I have no other arm devices I can check
        self.is_arm = self.machine in ('armv71', )

        self.cpu_count = 0
        if self.is_x86:
            self.cpu_info_x86()
        elif self.is_powerpc:
            self.cpu_info_ppc()
        elif self.is_mips:
            self.cpu_info_mips()

        # 64-bit (x86_64/AMD64/Intel64)
        # Long Mode (x86-64: amd64, also known as Intel 64, i.e. 64-bit capable)
        self.is_64bit = (self.is_x86 and 'lm' in self.x86_flags) or \
                        (self.is_powerpc and '970' in self.ppc_type)

        # Hardware virtualization capable: vmx (Intel), svm (AMD)
        self.has_hwvirt = self.is_x86 and (
                            (self.is_amd and 'svm' in self.x86_flags) or
                            (self.is_intel and 'vmx' in self.x86_flags))

        # Physical Address Extensions (support for more than 4GB of RAM)
        self.has_pae = self.is_x86 and 'pae' in self.x86_flags


    def cpu_info_x86(self):
        "parse /proc/cpuinfo for x86 kernels"
        for k,v in cpuinfo_kv():
            if k == 'processor':
                self.cpu_count += 1
                if self.cpu_count > 1:
                    # assume all CPUs are identical features, no need to
                    # parse all of them
                    continue
            elif k == 'vendor_id': # AuthenticAMD, GenuineIntel
                self.vendor_id = v
                self.is_amd = v == 'AuthenticAMD'
                self.is_intel = v == 'GenuineIntel'
            elif k == 'flags':
                self.x86_flags = v.split()
            elif k == 'model name':
                self.model_name = v
            elif k == 'cpu family':
                self.cpu_family = v
            elif k == 'model':
                self.model = v

    def cpu_info_ppc(self):
        "parse /proc/cpuinfo for PowerPC kernels"
        # http://en.wikipedia.org/wiki/List_of_PowerPC_processors
        # PowerPC 7xx family
        # PowerPC 740 and 750, 233-366 MHz
        # 745/755, 300–466 MHz

        # PowerPC G4 series
        # 7400/7410 350 - 550 MHz, uses AltiVec, a SIMD extension of the original PPC specs
        # 7450 micro-architecture family up to 1.5 GHz and 256 kB on-chip L2 cache and improved Altivec
        # 7447/7457 micro-architecture family up to 1.8 GHz with 512 kB on-chip L2 cache
        # 7448 micro-architecture family (1.5 GHz) in 90 nm with 1MB L2 cache and slightly
        #  improved AltiVec (out of order instructions).
        # 8640/8641/8640D/8641D with one or two e600 (Formerly known as G4) cores, 1MB L2 cache

        # PowerPC G5 series
        # 970 (2003), 64-bit, derived from POWER4, enhanced with VMX, 512 kB L2 cache, 1.4 – 2 GHz
        # 970FX (2004), manufactured at 90 nm, 1.8 - 2.7 GHz
        # 970GX (2006), manufactured at 90 nm, 1MB L2 cache/core, 1.2 - 2.5 GHz
        # 970MP (2005), dual core, 1 MB L2 cache/core, 1.6 - 2.5 GHz
        for k,v in cpuinfo_kv():
            if k == 'processor':
                self.cpu_count += 1
            elif k == 'cpu':
                self.is_altivec_supported = 'altivec' in v
                ppc_type, x = v.split(',')
                self.ppc_type = ppc_type.strip()
        # older kernels might not have a 'processor' line
        if self.cpu_count == 0:
            self.cpu_count += 1


    def cpu_info_mips(self):
        "parse /proc/cpuinfo for MIPS kernels"
        for k,v in cpuinfo_kv():
            if k == 'processor':
                self.cpu_count += 1
            elif k == 'cpu model':
                self.mips_cpu_model = v


def is_userspace_32bit(cpuinfo):
    """Even if `uname -m` reports a 64-bit architecture, userspace could still
    be 32-bit, such as Debian on powerpc64. This function tries to figure out
    if userspace is 32-bit, i.e. we might need to pass '-m32' or '-m64' to gcc.
    """
    if not cpuinfo.is_64bit:
        return True
    # note that having a 64-bit CPU means nothing for these purposes. You could
    # run a completely 32-bit system on a 64-bit capable CPU.
    answer = None
    # Debian ppc64 returns machine 'ppc64', but userspace might be 32-bit
    # We'll make an educated guess by examining a known executable
    exe = '/bin/mount'
    if os.path.isfile(exe):
        #print 'Found %s' % exe
        if os.path.islink(exe):
            real_exe = os.path.join(os.path.dirname(exe), os.readlink(exe))
            #print '%s is a symlink to %s' % (exe, real_exe)
        else:
            real_exe = exe
        # presumably if a person is running this script, they should have
        # a gcc toolchain installed...
        x = outputof('objdump', '-Wi', real_exe)
        # should emit a line that looks like this:
        # /bin/mount:     file format elf32-i386
        # or like this:
        # /bin/mount:     file format elf64-x86-64
        # or like this:
        # /bin/mount:     file format elf32-powerpc
        for line in x.split('\n'):
            line = line.strip()
            if line.startswith(real_exe):
                x, fmt = line.rsplit(None, 1)
                answer = 'elf32' in fmt
                break
    else:
        print '!!! Not found %s' % exe
    return answer


def cc_flags_x86(cpuinfo, enable_optimizations):
    """add certain gcc -m flags based on CPU features
    """
    # See http://gcc.gnu.org/onlinedocs/gcc-4.4.4/gcc/i386-and-x86_002d64-Options.html
    cc_opts = []
    if cpuinfo.machine == 'i586':
        cc_opts.append('-march=i586')
    elif cpuinfo.machine == 'i686':
        cc_opts.append('-march=i686')

    if 'mmx' in cpuinfo.x86_flags:
        cc_opts.append('-mmmx')

    # map from proc/cpuinfo flags to gcc options
    opt_flags = [
            ('sse', ('-mfpmath=sse', '-msse')),
            ('sse2', '-msse2'),
            ('ssse3', '-mssse3'),
            ('sse4', '-msse4'),
            ('sse4_1', '-msse4.1'),
            ('sse4_2', '-msse4.2'),
            ('sse4a', '-msse4a'),
            ('3dnow', '-m3dnow'),
    ]
    if enable_optimizations:
        for flag, gccopt in opt_flags:
            if flag in cpuinfo.x86_flags:
                if isinstance(gccopt, (tuple, list)):
                    cc_opts.extend(gccopt)
                else:
                    cc_opts.append(gccopt)
    return cc_opts


def cc_flags_powerpc(cpuinfo, enable_optimizations):
    """add certain gcc -m flags based on CPU model
    """
    cc_opts = []
    if cpuinfo.is_altivec_supported:
        cc_opts.append ('-maltivec')
        cc_opts.append ('-mabi=altivec')

    if re.match('74[0145][0578]A?', cpuinfo.ppc_type) is not None:
        cc_opts.append ('-mcpu=7400')
        cc_opts.append ('-mtune=7400')
    elif re.match('750', cpuinfo.ppc_type) is not None:
        cc_opts.append ('-mcpu=750')
        cc_opts.append ('-mtune=750')
    elif re.match('PPC970', cpuinfo.ppc_type) is not None:
        cc_opts.append ('-mcpu=970')
        cc_opts.append ('-mtune=970')
    elif re.match('Cell Broadband Engine', cpuinfo.ppc_type) is not None:
        cc_opts.append('-mcpu=cell')
        cc_opts.append('-mtune=cell')
    return cc_opts
#=== End Revised CXXFLAGS =========================================

# Autodetect
if env['DIST_TARGET'] == 'auto':
    if re.search ("x86_64", config[config_cpu]) is not None:
        env['DIST_TARGET'] = 'x86_64'
    elif re.search("i[0-5]86", config[config_cpu]) is not None:
        env['DIST_TARGET'] = 'i386'
    elif re.search("i686", config[config_cpu]) is not None:
        env['DIST_TARGET'] = 'i686'
    elif re.search("powerpc64", config[config_cpu]) is not None:
        env['DIST_TARGET'] = 'powerpc64'
    elif re.search("powerpc", config[config_cpu]) is not None:
        env['DIST_TARGET'] = 'powerpc'
    else:
        env['DIST_TARGET'] = config[config_cpu]
    print "Detected DIST_TARGET = " + env['DIST_TARGET']

#=== Begin Revised CXXFLAGS =========================================
# comment on DIST_TARGET up top implies it can be used for cross-compiling
# but that's not true because even if it is not 'auto' the original
# script still reads /proc/cpuinfo to determine gcc arch flags.
# This script does the same as the original. Needs to be fixed someday.
cpuinfo = CpuInfo()
if cpuinfo.is_x86:
    opt_flags.extend(cc_flags_x86(cpuinfo, env['ENABLE_OPTIMIZATIONS']))
if cpuinfo.is_powerpc:
    opt_flags.extend(cc_flags_powerpc(cpuinfo, env['ENABLE_OPTIMIZATIONS']))
if '-msse' in opt_flags:
    env['USE_SSE'] = 1
if '-msse2' in opt_flags:
    env['USE_SSE2'] = 1

m32 = is_userspace_32bit(cpuinfo)
print 'User space is %s' % (m32 and '32-bit' or '64-bit')
if cpuinfo.is_powerpc:
    if m32:
        print "Doing a 32-bit PowerPC build for %s CPU" % cpuinfo.ppc_type
        machineflags = { 'CXXFLAGS' : ['-m32'] }
    else:
        print "Doing a 64-bit PowerPC build for %s CPU" % cpuinfo.ppc_type
        machineflags = { 'CXXFLAGS' : ['-m64'] }
    env.MergeFlags( machineflags )
elif cpuinfo.is_x86:
    if m32:
        print "Doing a 32-bit %s build for %s" % (cpuinfo.machine, cpuinfo.model_name)
        machineflags = { 'CXXFLAGS' : ['-m32'] }
    else:
        print "Doing a 64-bit %s build for %s" % (cpuinfo.machine, cpuinfo.model_name)
        machineflags = { 'CXXFLAGS' : ['-m64'] }
        needs_fPIC = True
    env.MergeFlags( machineflags )
#=== End Revised CXXFLAGS =========================================


if needs_fPIC or ( env.has_key('COMPILE_FLAGS') and '-fPIC' in env['COMPILE_FLAGS'] ):
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

# avoid the 1.999.41- type of version for exported versions
if env['REVISION'] != '':
	env['REVISIONSTRING'] = '-' + env['REVISION']
else:
	env['REVISIONSTRING'] = ''

env['FFADO_API_VERSION'] = FFADO_API_VERSION

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
env.Depends( "config.h", 'cache/options.cache' )

# update version.h whenever the version or SVN revision changes
env.Depends( "version.h", env.Value(env['REVISION']))
env.Depends( "version.h", env.Value(env['VERSION']))

env.Depends( "libffado.pc", "SConstruct" )
pkgconfig = env.ScanReplace( "libffado.pc.in" )
env.Install( env['libdir'] + '/pkgconfig', pkgconfig )

env.Install( env['sharedir'], 'configuration' )

subdirs=['src','libffado','support','doc']
if env['BUILD_TESTS']:
    subdirs.append('tests')

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
        mixerdesktopaction = env.Action( "-xdg-desktop-menu %s support/xdg/ffado.org-ffadomixer.desktop" % action )
        mixericonaction = env.Action( "-xdg-icon-resource %s --size 64 --novendor --context apps support/xdg/hi64-apps-ffado.png ffado" % action )
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
# vim: ts=4 sw=4 et
