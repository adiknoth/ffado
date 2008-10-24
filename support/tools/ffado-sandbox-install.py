#!/usr/bin/python
#

#
# Copyright (C) 2008 Pieter Palmers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#
# A script to install FFADO in a 'sandbox', i.e. without changing the 
# system installation.
#
import os

FFADOSBI_VERSION = '0.1'
LATEST_FFADO_RELEASE_URL = 'http://www.ffado.org/files/libffado-2.0-beta6.tar.gz'
LATEST_FFADO_RELEASE_UNPACK_DIR = 'libffado-2.0-beta6'
LATEST_JACK1_RELEASE_URL = 'http://jackaudio.org/downloads/jack-audio-connection-kit-0.109.2.tar.gz'
LATEST_JACK1_RELEASE_UNPACK_DIR = 'jack-audio-connection-kit-0.109.2'

def ask_for_dir(descr):
    ret_dir = None
    while True:
        ret_dir = raw_input("Please specify a %s directory: " % descr)
        
        if not os.path.exists(ret_dir):
            try:
                os.makedirs(ret_dir)
            except:
                yesno = raw_input("Could not create the %s directory. Try again? [yes/no] " % descr)
                if yesno[0] != 'y':
                    return None
                else:
                    continue
            break
        else:
            yesno = raw_input("WARNING: the %s directory already exists. Do you want to overwrite it? [yes/no] " % descr)
            if yesno[0] != 'y':
                yesno = raw_input("Try again? [yes/no] ")
                if yesno[0] != 'y':
                    return None
                else:
                    continue
            else:
                os.system('rm -Rf "%s"' % ret_dir)
                os.makedirs(ret_dir)
                break
    return ret_dir

def fetch_source(build_dir, source_descriptor, target):
    if source_descriptor[1] == 'svn':
        print " checking out SVN repository: %s" % source_descriptor[2]
        cwd = os.getcwd()
        os.chdir(build_dir)
        os.system('svn co "%s" "%s"' % (source_descriptor[2], target))
        os.chdir(cwd)
        return True
    elif source_descriptor[1] == 'tar.gz':
        print " downloading tarball: %s" % source_descriptor[2]
        import urllib
        tmp_file = '%s/tmp.tar.gz' % build_dir
        urllib.urlretrieve(source_descriptor[2], tmp_file)
        cwd = os.getcwd()
        os.chdir(build_dir)
        print " extracting tarball..."
        os.system('tar -zxf "%s"' % tmp_file)
        if source_descriptor[3]:
            os.system('mv "%s" "%s"' % (source_descriptor[3], target))
        os.chdir(cwd)
        return True
    else:
        print "bad source type"
        return False

welcome_msg = """
FFADO sandbox install utility """ + FFADOSBI_VERSION + """
=================================
(C) 2008 Pieter Palmers

This utility will automatically build and install a sandbox
environment that can be used to evaluate FFADO. The required
FFADO and/or jack packages are downloaded.

NOTE: The build dependencies are not automatically installed.
Please consult http://subversion.ffado.org/wiki/Dependencies
for more information. The subversion tool has to be installed.

NOTE: This tool assumes that an internet connection is available
while it runs. Once everything is built, no connection is
required.

You can exit the tool at any time using CTRL-C.

"""

print welcome_msg

# get the paths to be used
if 'HOME' in os.environ.keys():
    suggestion = "%s/ffadosandbox" % os.environ['HOME']
else:
    suggestion = "/home/myuser/ffadosandbox"

sandbox_dir_msg = """
SANDBOX DIRECTORY
=================

The sandbox directory is the directory where all built files are 
installed into. It should be writable by your user, and will
contain the evaluation binaries if this tool is successful. If
it doesn't exist, it will be created.

Once you are finished with evaluating FFADO you can simply remove
this directory and things are as if nothing happened.

Suggestion: %s

NOTE: if you specify a system directory here, the tool will install
system-wide (if run as root). This is not recommended, but can be
useful for automated installs. Uninstall will be a lot harder though.
""" % suggestion

if 'HOME' in os.environ.keys():
    suggestion = "%s/ffadobuild" % os.environ['HOME']
else:
    suggestion = "/home/myuser/ffadobuild"

build_dir_msg = """
BUILD DIRECTORY
===============

The build directory is where all temporary files are stored while
building FFADO and jack. It should be writable by your user. If
it doesn't exist, it will be created.

The build directory can be removed as soon as this tool has finished.
It is not automatically removed.

Suggestion: %s
""" % suggestion

print sandbox_dir_msg
sandbox_dir = ask_for_dir('sandbox')
if sandbox_dir == None:
    print "Cannot proceed without valid sandbox directory."
    exit(-1)
print(" using %s as sandbox directory" % sandbox_dir)


print build_dir_msg
build_dir = ask_for_dir('build')
if build_dir == None:
    print "Cannot proceed without valid build directory."
    exit(-1)
print(" using %s as build directory" % build_dir)

# figure out what version of FFADO to build
# Note: format:
# ['title', type=svn or tar.gz, url, (optional) dir that contains target]

ffado_versions = {}
ffado_versions[0] = ['SVN trunk', 'svn', 'http://subversion.ffado.org/ffado/trunk/libffado', None]
ffado_versions[1] = ['libffado-2.0 (recommended)', 'svn', 'http://subversion.ffado.org/ffado/branches/libffado-2.0', None]
ffado_versions[2] = ['latest release', 'tar.gz', LATEST_FFADO_RELEASE_URL, LATEST_FFADO_RELEASE_UNPACK_DIR]

ffado_versions_msg = """
Available FFADO versions:
"""
for key in sorted(ffado_versions.keys()):
    ffado_versions_msg += " %2s: %s\n" % (key, ffado_versions[key][0])

print ffado_versions_msg
while True:
    ffado_version = raw_input("Please select a FFADO version: ")
    try:
        ffado_version_int = eval(ffado_version)
        if ffado_version_int not in [0, 1, 2]:
            raise
    except:
        yesno = raw_input("Invalid FFADO version specified. Try again? [yes/no] ")
        if yesno[0] != 'y':
            print "Cannot proceed without valid FFADO version."
            exit(-1)
        else:
            continue
    break

use_ffado_version = ffado_versions[ffado_version_int]

# figure out what version of jack to build
jack_versions = {}
jack_versions[0] = ['jack1 SVN trunk', 'svn', 'http://subversion.jackaudio.org/jack/trunk/jack', None]
# note: latest release is no good.
#jack_versions[1] = ['jack1 latest release', 'tar.gz', LATEST_JACK1_RELEASE_URL, LATEST_JACK_RELEASE_UNPACK_DIR]

jack_versions_msg = """
Available jack versions:
"""
for key in sorted(jack_versions.keys()):
    jack_versions_msg += " %2s: %s\n" % (key, jack_versions[key][0])

print jack_versions_msg
while True:
    jack_version = raw_input("Please select a jack version: ")
    try:
        jack_version_int = eval(jack_version)
        if jack_version_int not in [0, 1, 2]:
            raise
    except:
        yesno = raw_input("Invalid jack version specified. Try again? [yes/no] ")
        if yesno[0] != 'y':
            print "Cannot proceed without valid jack version."
            exit(-1)
        else:
            continue
    break

use_jack_version = jack_versions[jack_version_int]

# print a summary
print """
SUMMARY
=======
Sandbox directory : %s
Build directory   : %s
FFADO version     : %s
Jack version      : %s

""" % (sandbox_dir, build_dir, use_ffado_version[0], use_jack_version[0])

# get the ffado source
print "Fetching FFADO source..."

if not fetch_source(build_dir, use_ffado_version, 'libffado'):
    print "Could not fetch FFADO source"
    exit(-1)
print " got FFADO source"

print "Fetching jack source..."

if not fetch_source(build_dir, use_jack_version, 'jack'):
    print "Could not fetch jack source"
    exit(-1)
print " got jack source"

cwd = os.getcwd()
# configure FFADO
os.chdir("%s/libffado/" % build_dir)
os.system('scons PREFIX="%s"' % sandbox_dir)

# install FFADO
os.system('scons install')

# configure JACK
os.chdir("%s/jack/" % build_dir)
if use_jack_version[1] == 'svn':
    os.system('./autogen.sh') 
os.system('./configure --prefix="%s"' % sandbox_dir)

# build and install jack
os.system('make')
os.system('make install')

# write the bashrc file
sandbox_bashrc = """
#!/bin/bash
#

LD_LIBRARY_PATH="%s/lib:$LD_LIBRARY_PATH"
PKG_CONFIG_PATH="%s/lib/pkgconfig:$PKG_CONFIG_PATH"
PATH="%s/bin:$PATH"

export LD_LIBRARY_PATH
export PKG_CONFIG_PATH
export PATH
""" % (sandbox_dir, sandbox_dir, sandbox_dir)

sandbox_rc_file = "%s/ffado.rc" % sandbox_dir

fid = open(sandbox_rc_file, "w")
fid.write(sandbox_bashrc)
fid.close()

os.chdir(cwd)

print """
FFADO and jack are installed into %s. If you want to use the
versions in the sandbox, you have to alter your environment
such that it uses them instead of the system versions.

If you use the bash shell (or compatible) you can use the following
rc script: %s. The procedure to use the sandboxed ffado+jack would be:
   
   $ source %s
   $ jackd -R -d firewire

If you don't use the bash shell, you have to alter your environment
manually. Look at the %s script for inspiration.

Note that you have to source the settings script for every terminal
you open. If you want a client application to use the sandboxed jack
you have to source the settings.

E.g.:
 terminal 1:
   $ source %s
   $ jackd -R -d firewire
 terminal 2:
   $ source %s
   $ ardour2
 terminal 3:
   $ source %s
   $ qjackctl &
   $ hydrogen

If you want to use the sandboxed version as default, you have to ensure
that the default environment is changed. This can be done e.g. by adding
the settings script to ~/.bashrc.

The build directory %s can now be deleted. To uninstall this sandbox, delete
the %s directory.
""" % (sandbox_dir, sandbox_rc_file, sandbox_rc_file, \
       sandbox_rc_file, sandbox_rc_file, sandbox_rc_file, \
       sandbox_rc_file, build_dir, sandbox_dir)
