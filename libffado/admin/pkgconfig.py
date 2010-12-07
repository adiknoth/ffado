#!/usr/bin/python
#
# Copyright (C) 2007-2008 Arnold Krille
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

#
# Taken from http://www.scons.org/wiki/UsingPkgConfig
# and heavily modified
#

#
# Checks for pkg-config
#
def CheckForPKGConfig( context, version='0.0.0' ):
	context.Message( "Checking for pkg-config (at least version %s)... " % version )
	ret = context.TryAction( "pkg-config --atleast-pkgconfig-version=%s" %version )[0]
	context.Result( ret )
	return ret

#
# Checks for the given package with an optional version-requirement
#
# The flags (which can be imported into the environment by env.MergeFlags(...)
# are exported as env['NAME_FLAGS'] where name is built by removing all +,-,.
# and upper-casing.
#
def CheckForPKG( context, name, version="" ):
	name2 = name.replace("+","").replace(".","").replace("-","")

	if version == "":
		context.Message( "Checking for %s... \t" % name )
		ret = context.TryAction( "pkg-config --exists '%s'" % name )[0]
	else:
		context.Message( "Checking for %s (%s or higher)... \t" % (name,version) )
		ret = context.TryAction( "pkg-config --atleast-version=%s '%s'" % (version,name) )[0]

	if ret:
		context.env['%s_FLAGS' % name2.upper()] = context.env.ParseFlags("!pkg-config --cflags --libs %s" % name)

	context.Result( ret )
	return ret

#
# Checks for the existance of the package and returns the packages flags.
#
# This should allow caching of the flags so that pkg-config is called only once.
#
def GetPKGFlags( context, name, version="" ):
	import os

	if version == "":
		context.Message( "Checking for %s... \t" % name )
		ret = context.TryAction( "pkg-config --exists '%s'" % name )[0]
	else:
		context.Message( "Checking for %s (%s or higher)... \t" % (name,version) )
		ret = context.TryAction( "pkg-config --atleast-version=%s '%s'" % (version,name) )[0]

	if not ret:
		context.Result( ret )
		return ret

	out = os.popen2( "pkg-config --cflags --libs %s" % name )[1]
	ret = out.read()

	context.Result( True )
	return ret

#
# Checks for the existance of the package and returns the value of the specified variable.
#
def GetPKGVariable( context, name, variable ):
	import os

	context.Message( "Checking for variable %s in package %s... \t" % (variable,name) )

	ret = context.TryAction( "pkg-config --exists '%s'" % name )[0]
	if not ret:
		context.Result( ret )
		return ret

	out = os.popen2( "pkg-config --variable=%s %s" % (variable,name) )[1]
	ret = out.read()

	context.Result( True )
	return ret

def generate( env, **kw ):
	env['PKGCONFIG_TESTS' ] = { 'CheckForPKGConfig' : CheckForPKGConfig, 'CheckForPKG' : CheckForPKG, 'GetPKGFlags' : GetPKGFlags, 'GetPKGVariable' : GetPKGVariable }

def exists( env ):
	return 1


