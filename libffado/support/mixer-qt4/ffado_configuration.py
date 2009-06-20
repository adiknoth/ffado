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

import shlex

import logging
log = logging.getLogger('configparser')

class DeviceList:
	def __init__( self, filename="" ):
		self.devices = list()
		if not filename == "":
			self.updateFromFile( filename )

	def updateFromFile( self, filename ):
		filename = os.path.expanduser(filename)
		log.debug("DeviceList::updateFromFile(%s)" % filename)
		if not os.path.exists( filename ):
			log.error("cannot open file")
			return
		f = open( filename, "r" )

		lex = Parser( f )
		config = {}
		while lex.peaktoken() != lex.eof:
			tmp = lex.parsenamedvalue()
			if tmp != None:
				config[ tmp[0] ] = tmp[1]
		for dev in config["device_definitions"]:
			self.addDevice( dev )

	def getDeviceById( self, vendor, model ):
		log.debug("DeviceList::getDeviceById( %s, %s )" % (vendor, model ))
		for dev in self.devices:
			if int("%s" % dev['vendorid'], 0) == int("%s" % vendor, 0) and \
			   int("%s" % dev['modelid'], 0) == int("%s" % model, 0):
				return dev
		tmp = dict()
		self.devices.append( tmp )
		return tmp

	def addDevice( self, device_dict ):
		log.debug("DeviceList::addDevice()")
		dev = self.getDeviceById( device_dict['vendorid'], device_dict['modelid'] )
		dev.update( device_dict )

class Parser:
	def __init__( self, file ):
		self.lex = shlex.shlex( file )
		self.eof = self.lex.eof

	def peaktoken( self ):
		token = self.lex.get_token()
		self.lex.push_token( token )
		return token

	def parselist( self, level="" ):
		token = self.peaktoken()
		if token != "(":
			return
		self.lex.get_token()
		log.debug("%sWill parse list" % level)
		ret = []
		token = self.peaktoken()
		while token != ")":
			ret.append( self.parsenamedvalue( level+"  " ) )
			token = self.peaktoken()
		log.debug("%slist is %s" % (level, str(ret)))
		self.lex.get_token()
		if self.peaktoken() == ",":
			log.debug("%sFound a delimiter" % level)
			self.lex.get_token()
		return ret

	def parsemap( self, level="" ):
		token = self.lex.get_token()
		if token != "{":
			return
		log.debug("%sWill parse map" % level)
		ret = {}
		token = self.peaktoken()
		while token != "}":
			#self.push_token( token )
			tmp = self.parsenamedvalue( level+"  " )
			if tmp != None:
				ret[ tmp[0] ] = tmp[1]
			token = self.peaktoken()
		token = self.lex.get_token()
		log.debug("%sMap ended with '%s' and '%s'"% (level,token,self.peaktoken()))
		if self.peaktoken() in (",",";"):
			log.debug("%sFound a delimiter!" % level)
			self.lex.get_token()
		return ret

	def parsevalue( self, level="" ):
		token = self.lex.get_token()
		log.debug("%sparsevalue() called on token '%s'" % (level, token))
		self.lex.push_token( token )
		if token == "(":
			value = self.parselist( level+"  " )
		elif token == "{":
			value = self.parsemap( level+"  " )
		else:
			value = self.lex.get_token().replace( "\"", "" )
		token = self.peaktoken()
		if token == ";":
			log.debug("%sFound a delimiter!" % level)
			self.lex.get_token()
		return value

	def parsenamedvalue( self, level="" ):
		token = self.lex.get_token()
		log.debug("%sparsenamedvalue() called on token '%s'" % (level, token))
		if token == "{":
			self.lex.push_token( token )
			return self.parsemap( level+"  " )
		if len(token) > 0 and token not in ("{","}","(",")",",",";"):
			log.debug("%sGot name '%s'" %(level,token))
			name = token
			token = self.lex.get_token()
			if token in ("=",":"):
				#print "%sWill parse value" % level
				value = self.parsevalue( level )
				return (name,value)
		log.debug("%sparsenamedvalue() will return None!" % level)
		return

#
# Have kind of a test directly included...
#
if __name__ == "__main__":

	import sys

	file = sys.argv[1]

	log.setLevel(logging.DEBUG)
	devs = DeviceList( file )

	print devs.devices

