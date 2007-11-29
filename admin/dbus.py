#!/usr/bin/python
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

#
# xml translator
#

def dbusxx_xml2cpp_emitter(target, source, env):
	env.Depends(target, "#/external/dbus/dbusxx-xml2cpp" )
	return (target, source)

def dbusxx_xml2cpp_adaptor_action( target, source, env ):
	env.Execute( "./external/dbus/dbusxx-xml2cpp %s --adaptor=%s" % ( source[0], target[0] ) )
	return 0

def dbusxx_xml2cpp_proxy_action( target, source, env ):
	env.Execute( "./external/dbus/dbusxx-xml2cpp %s --proxy=%s" % ( source[0], target[0] ) )
	return 0

def generate( env, **kw ):
	env['BUILDERS']['Xml2Cpp_Adaptor'] = env.Builder(action = dbusxx_xml2cpp_adaptor_action,
		emitter = dbusxx_xml2cpp_emitter,
		suffix = '.h', src_suffix = '.xml')
	env['BUILDERS']['Xml2Cpp_Proxy'] = env.Builder(action = dbusxx_xml2cpp_proxy_action,
		emitter = dbusxx_xml2cpp_emitter,
		suffix = '.h', src_suffix = '.xml', single_source=True )

def exists( env ):
	return 1

