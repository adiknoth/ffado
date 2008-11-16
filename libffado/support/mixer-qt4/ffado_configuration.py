#!/usr/bin/python
#
# Copyright (C) 2008 by Arnold Krille
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

import re, os

class DeviceList:
	def __init__( self, filename="" ):
		self.devices = list()
		if not filename == "":
			self.updateFromFile( filename )

	def updateFromFile( self, filename ):
		if not os.path.exists( filename ):
			return
		f = open( filename, "r" )
		stream = f.read()
		stream = re.sub( "#[^#\n]*\n", "\n", stream )                               # remove the comments
		stream = re.sub( "\s", "", stream )                                         # remove all white-spaces and line-breaks
		stream = stream.replace( "device_definitions=({", "" ).replace( "});", "" ) # remove the trail and end
		stream = stream.replace( "\"", "" )                                         # remove the "'s around

		dev_strings = stream.split( "},{" )
		for s in dev_strings:
			item = dict()
			pairs = s.split(";")
			for p in pairs:
				if not p == "":
					key = p[0:p.find("=")]
					value = p[p.find("=")+1:]
					if re.search( "^0x[0-9a-fA-F]*", value ) != None: # convert hex-numbers to int
						value = int( value, 16 )              # or should it be long?
					item[ key ] = value
			self.addDevice( item )

	def getDeviceById( self, vendor, model ):
		#print "DeviceList::getDeviceById( %s, %s )" % (vendor, model )
		for dev in self.devices:
			if dev['vendorid'] == vendor and dev['modelid'] == model:
				return dev
		tmp = dict()
		self.devices.append( tmp )
		return tmp

	def addDevice( self, device_dict ):
		#print "DeviceList::addDevice()"
		dev = self.getDeviceById( device_dict['vendorid'], device_dict['modelid'] )
		dev.update( device_dict )


#
# Have kind of a test directly included...
#
if __name__ == "__main__":

	import sys

	file = sys.argv[1]

	devs = DeviceList( file )

	print devs.devices

