#!/usr/bin/python
#
# Copyright (C) 2007 Arnold Krille
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

def pyuic_action( target, source, env ):
	print "'"+str( target[0] )+"'"
	print "'"+str( source[0] )+"'"
	print env.Command( str( target[0] ), str( source[0] ), action = "pyuic $SOURCE > $TARGET" )
	return 0

def pyuic_string( target, source, env ):
	return "building '%s' from '%s'" % ( str(target[0]), str( source[0] ) )

def generate( env, **kw ):
	action = env.Action( pyuic_action, pyuic_string )
	env['BUILDERS']['PyUIC'] = env.Builder( action=action, src_suffix=".ui", single_source=True )

def exists( env ):
	return 1

