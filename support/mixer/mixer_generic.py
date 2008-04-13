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
# the Free Software Foundation, either version 2 of the License, or
# version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import sys
import dbus
from qt import *

class ContainerWidget( QFrame ):
	def __init__( self, name, interface, parent ):
		QFrame.__init__( self, parent )
		self.interface = interface
		self.setFrameStyle( QFrame.Raised|QFrame.StyledPanel )
		self.setLineWidth( 2 )
		l2 = QVBoxLayout( self )
		lbl = QLabel( "<qt><b>" + name + "</b></qt>", self )
		l2.addWidget( lbl )
		l2.setMargin( 2 )
		self.l = QHBoxLayout( self )
		l2.addLayout( self.l )

	def layout( self ):
		return self.l

class ContinuousWidget( QWidget ):
	def __init__( self, name, interface, parent ):
		QWidget.__init__( self, parent )
		self.interface = interface
		l = QVBoxLayout( self )
		l.addWidget( QLabel( name, self ) )
		self.slider = QSlider( self )
		l.addWidget( self.slider )
		# Not yet working:
		self.slider.setValue( self.interface.getValue() )

class EnumWidget( QWidget ):
	def __init__( self, name, interface, parent ):
		QWidget.__init__( self, parent )
		self.interface = interface
		l = QVBoxLayout( self )
		l.addWidget( QLabel( name, self ) )
		self.select = QComboBox( False, self )
		l.addWidget( self.select )
		for i in range( interface.count() ):
			self.select.insertItem( interface.getEnumLabel( i ) )
		self.select.setCurrentItem( self.interface.selected() )
		self.connect( self.select, SIGNAL( "activated(int)" ), self.currentChanged )

	def currentChanged( self, i ):
		print "EnumWidget::currentChanged(" + str(i) + ")"
		self.interface.select( i )

class GenericMixer( QWidget ):
	def __init__( self, bus, session, parent=None ):
		QWidget.__init__( self, parent )
		QHBoxLayout( self )
		self.introspect( bus, session, "/org/ffado/Control/DeviceManager", self )

	def introspect( self, bus, session, path, parentwidget ):
		element = bus.get_object( session, path )
		element._Introspect().block()
		interfacemap = element._introspect_method_map
		interfacelist = []
		for t in interfacemap:
			tmp = ".".join(t.split(".")[:-1])
			if tmp not in interfacelist:
				interfacelist.append( tmp )
		#print interfacelist

		name = "Unnamed"

		if interfacelist.count( "org.ffado.Control.Element.Element" ):
			name = dbus.Interface( element, "org.ffado.Control.Element.Element" ).getName()
			#print name

		if interfacelist.count( "org.ffado.Control.Element.Container" ):
			end = 0
			container = dbus.Interface( element, "org.ffado.Control.Element.Container" )
			end = container.getNbElements()
			w = ContainerWidget( name, container, parentwidget )
			parentwidget.layout().addWidget( w )
			for i in range( end ):
				print "%s %i %s" % ( path, i, container.getElementName( i ) )
				self.introspect( bus, session, path + "/" + container.getElementName( i ), w )

		if interfacelist.count( "org.ffado.Control.Element.Continuous" ):
			control = dbus.Interface( element, "org.ffado.Control.Element.Continuous" )
			#print control.getValue()
			w = ContinuousWidget( name, control, parentwidget )
			parentwidget.layout().addWidget( w )

		if interfacelist.count( "org.ffado.Control.Element.AttributeEnum" ):
			w = EnumWidget( name, dbus.Interface( element, "org.ffado.Control.Element.AttributeEnum" ), parentwidget )
			parentwidget.layout().addWidget( w )

