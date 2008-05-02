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
		self.setFrameStyle( QFrame.Plain|QFrame.StyledPanel )
		self.setLineWidth( 1 )
		self.setMargin( 5 )
		l2 = QVBoxLayout( self )
		l2.setMargin( 5 )
		lbl = QLabel( "<qt><b>" + name + "</b></qt>", self )
		l2.addWidget( lbl )
		self.l = QHBoxLayout( None )
		self.l.setMargin( 0 )
		self.l.setSpacing( 2 )
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

		self.min = self.interface.getMinimum()
		self.max = self.interface.getMaximum()
		self.slider.setMinValue( 0 )
		self.slider.setMaxValue( 100 )
		self.slider.setValue( self.interfaceToSlider( self.interface.getValue() ) )
		self.connect( self.slider, SIGNAL( "valueChanged(int)" ), self.sliderMoved )

	def sliderToInterFace( self, slider ):
		return ( self.max - self.min ) * (100-slider)/100.0 + self.min
	def interfaceToSlider( self, interface ):
		return - ( interface - self.min ) / ( self.max - self.min ) * 100.0 + 100

	def sliderMoved( self, i ):
		#print "ContinuousWidget::sliderMoved( %i )" % i
		self.interface.setValue( self.sliderToInterFace( i ) )

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
		#print "EnumWidget::currentChanged(" + str(i) + ")"
		self.interface.select( i )

class TextWidget( QWidget ):
	def __init__( self, name, interface, parent ):
		QWidget.__init__( self, parent )
		self.interface = interface
		l = QVBoxLayout( self )
		l.addWidget( QLabel( name, self ) )
		self.text = QLineEdit( self )
		l.addWidget( self.text )
		self.text.setText( interface.getValue() )
		self.connect( self.text, SIGNAL( "textChanged( const QString & )" ), self.setText )

	def setText( self, text ):
		self.interface.setValue( text.latin1() )

class HLine( QFrame ):
	def __init__( self, parent ):
		QFrame.__init__( self, parent )
		self.setFrameShape( QFrame.HLine )
		self.setLineWidth( 2 )
		self.setMinimumHeight( 10 )

class GenericMixer( QWidget ):
	def __init__( self, bus, session, parent=None ):
		QWidget.__init__( self, parent )
		QVBoxLayout( self )
		self.layout().addWidget( QLabel( "<qt>This generic interface is only intended for developers. The controls shown here may or may not work and probably don't represent the devices real structure. This just displays the object-tree as exported by ffado-dbus-server.</qt>", self ) )
		self.layout().addWidget( HLine( self ) )
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

		name = "Unnamed"

		if interfacelist.count( "org.ffado.Control.Element.Element" ):
			name = dbus.Interface( element, "org.ffado.Control.Element.Element" ).getName()

		w = None

		if interfacelist.count( "org.ffado.Control.Element.Container" ):
			container = dbus.Interface( element, "org.ffado.Control.Element.Container" )
			w = ContainerWidget( name, container, parentwidget )
			parentwidget.layout().addWidget( w )
			for i in range( container.getNbElements() ):
				#print "%s %i %s" % ( path, i, container.getElementName( i ) )
				self.introspect( bus, session, path + "/" + container.getElementName( i ), w )

		if w == None and interfacelist.count( "org.ffado.Control.Element.Continuous" ):
			w = ContinuousWidget( name, dbus.Interface( element, "org.ffado.Control.Element.Continuous" ), parentwidget )
			parentwidget.layout().addWidget( w )

		if w == None and interfacelist.count( "org.ffado.Control.Element.AttributeEnum" ):
			w = EnumWidget( name, dbus.Interface( element, "org.ffado.Control.Element.AttributeEnum" ), parentwidget )
			parentwidget.layout().addWidget( w )

		if w == None and interfacelist.count( "org.ffado.Control.Element.Text" ):
			w = TextWidget( name, dbus.Interface( element, "org.ffado.Control.Element.Text" ), parentwidget )
			parentwidget.layout().addWidget( w )

		if w == None:
			w = QLabel( "<qt><b>%s</b> (Not yet implemented)</qt>" % name, parentwidget )
			parentwidget.layout().addWidget( w )

