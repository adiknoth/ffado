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
LATEST_FFADO_RELEASE_URL = 'http://www.ffado.org/files/libffado-2.0-beta7.tar.gz'
LATEST_FFADO_RELEASE_UNPACK_DIR = 'libffado-2.0-beta7'
LATEST_JACK1_RELEASE_URL = 'http://jackaudio.org/downloads/jack-audio-connection-kit-0.109.2.tar.gz'
LATEST_JACK1_RELEASE_UNPACK_DIR = 'jack-audio-connection-kit-0.109.2'

def ask_for_dir(descr, suggestion):
    ret_dir = None
    while True:
        ret_dir = raw_input("Please specify a %s directory [%s]: " % (descr, suggestion))
        if ret_dir == "":
            ret_dir = suggestion

        if not os.path.exists(ret_dir):
            try:
                os.makedirs(ret_dir)
            except:
                yesno = raw_input("Could not create the %s directory. Try again? [yes/no] " % descr)
                if yesno == "" or yesno[0] != 'y':
                    return None
                else:
                    continue
            break
        else:
            yesno = raw_input("WARNING: the %s directory at %s already exists. Do you want to overwrite it? [yes/no] " % (descr, ret_dir))
            if yesno == "" or yesno[0] != 'y':
                yesno = raw_input("Specify new %s directory? [yes/no] " % descr)
                if yesno == "" or yesno[0] != 'y':
                    return None
                else:
                    continue
            else:
                yesno = raw_input("WARNING: about to remove the old %s directory at %s. Proceed? [yes/no] " % (descr, ret_dir))
                if yesno == "" or yesno[0] != 'y':
                    yesno = raw_input("Specify new %s directory? [yes/no] " % descr)
                    if yesno == "" or yesno[0] != 'y':
                        return None
                    else:
                        continue
                else:
                    os.system('rm -Rf "%s"' % ret_dir)
                    os.makedirs(ret_dir)
                    break
    return ret_dir

def fetch_source(build_dir, source_descriptor, target):
    logfile = "%s/%s.log" % (build_dir, target)
    os.system('echo "" > %s' % logfile)

    if source_descriptor[1] == 'svn':
        print " Checking out SVN repository: %s" % source_descriptor[2]
        cwd = os.getcwd()
        os.chdir(build_dir)
        retval = os.system('svn co "%s" "%s" >> %s' % (source_descriptor[2], target, logfile))
        os.chdir(cwd)
        if retval:
            print "  Failed to checkout the SVN repository. Inspect %s for details. (is subversion installed?)" % logfile
            return False
        return True
    elif source_descriptor[1] == 'tar.gz':
        print " Downloading tarball: %s" % source_descriptor[2]
        import urllib
        tmp_file = '%s/tmp.tar.gz' % build_dir
        try:
            urllib.urlretrieve(source_descriptor[2], tmp_file)
        except:
            print " Could not retrieve source tarball."
            return False
        cwd = os.getcwd()
        os.chdir(build_dir)
        print " extracting tarball..."
        retval = os.system('tar -zxf "%s" > %s' % (tmp_file, logfile))
        if retval:
            print "  Failed to extract the source tarball. Inspect %s for details." % logfile
            os.chdir(cwd)
            return False
        if source_descriptor[3]:
            retval = os.system('mv "%s" "%s"' % (source_descriptor[3], target))
            if retval:
                print "  Failed to move the extracted tarball"
                os.chdir(cwd)
                return False
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
    suggestion_sandbox = "%s/ffadosandbox" % os.environ['HOME']
else:
    suggestion_sandbox = "/home/myuser/ffadosandbox"

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
""" % suggestion_sandbox

if 'HOME' in os.environ.keys():
    suggestion_build = "%s/ffadobuild" % os.environ['HOME']
else:
    suggestion_build = "/home/myuser/ffadobuild"

build_dir_msg = """
BUILD DIRECTORY
===============

The build directory is where all temporary files are stored while
building FFADO and jack. It should be writable by your user. If
it doesn't exist, it will be created.

The build directory can be removed as soon as this tool has finished.
It is not automatically removed.

Suggestion: %s
""" % suggestion_build

print sandbox_dir_msg
sandbox_dir = ask_for_dir('sandbox', suggestion_sandbox)
if sandbox_dir == None:
    print "Cannot proceed without valid sandbox directory."
    exit(-1)
print(" using %s as sandbox directory" % sandbox_dir)


print build_dir_msg
build_dir = ask_for_dir('build', suggestion_build)
if build_dir == None:
    print "Cannot proceed without valid build directory."
    exit(-1)
print(" using %s as build directory" % build_dir)

# figure out what version of FFADO to build
# Note: format:
# ['title', type=svn or tar.gz, url, (optional) dir that contains target]

ffado_versions = {}
ffado_versions[0] = ['SVN trunk', 'svn', 'http://subversion.ffado.org/ffado/trunk/libffado', None]
ffado_versions[1] = ['SVN libffado-2.0 (recommended)', 'svn', 'http://subversion.ffado.org/ffado/branches/libffado-2.0', None]
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
        if yesno == "" or yesno[0] != 'y':
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
        if yesno == "" or yesno[0] != 'y':
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
print " Successfully fetched FFADO source"

print "Fetching jack source..."

if not fetch_source(build_dir, use_jack_version, 'jack'):
    print "Could not fetch jack source"
    exit(-1)
print " Successfully fetched jack source"

cwd = os.getcwd()

ffado_log = "%s/ffadobuild.log" % build_dir
ffado_scons_options = "-j2" # TODO: interactive config of the build
os.system('echo "" > %s' % ffado_log) 

# configure FFADO
os.chdir("%s/libffado/" % build_dir)
print "Building FFADO..."
print " Compiling..."
retval = os.system('scons PREFIX="%s" %s >> %s' % (sandbox_dir, ffado_scons_options, ffado_log))
if retval:
    print """
Failed to configure/build FFADO. Most likely this is due to uninstalled dependencies.
Check %s for details.
""" % ffado_log
    exit(-1)

# install FFADO
print " Installing into %s..." % sandbox_dir
retval = os.system('scons install >> %s' % (ffado_log))
if retval:
    print "Failed to install FFADO. Check %s for details." % ffado_log
    exit(-1)

# configure JACK
os.chdir("%s/jack/" % build_dir)
jack_log = "%s/jackbuild.log" % build_dir
os.system('echo "" > %s' % jack_log) 

print "Building Jack..."
if use_jack_version[1] == 'svn':
    print " Initializing build system..."
    retval = os.system('./autogen.sh >> %s' % jack_log)
    if retval:
        print """
Failed to initialize the jack build system. Most likely this is due to uninstalled dependencies.
Check %s for details.
""" % jack_log
        exit(-1)

print " Configuring build..."
retval = os.system('./configure --prefix="%s" >> %s' % (sandbox_dir, jack_log))
if retval:
    print """
Failed to configure the jack build. Most likely this is due to uninstalled dependencies.
Check %s for details.
""" % jack_log
    exit(-1)

# build and install jack
print " Compiling..."
retval = os.system('make >> %s' % (jack_log))
if retval:
    print "Failed to build jack. Check %s for details." % jack_log
    exit(-1)

print " Installing into %s..." % sandbox_dir
retval = os.system('make install >> %s' % (jack_log))
if retval:
    print "Failed to install jack. Check %s for details." % jack_log
    exit(-1)

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

print "Writing shell configuration file..."
sandbox_rc_file = "%s/ffado.rc" % sandbox_dir
try:
    fid = open(sandbox_rc_file, "w")
    fid.write(sandbox_bashrc)
    fid.close()
except:
    print "Could not write the sandbox rc file."
    exit(-1)

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
