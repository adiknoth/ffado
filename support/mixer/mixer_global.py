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

from qt import *
from mixer_globalui import *

class GlobalMixer( GlobalMixerUi ):
	def __init__( self, parent ):
		GlobalMixerUi.__init__( self, parent )

	def clockChanged( self, clock ):
		#print "updateClockSource( " + str(clock) + " )"
		self.clockselect.select( clock )

	def nicknameChanged( self, name ):
		#print "nicknameChanged( %s )" % name
		self.nickname.setText( name.latin1() )

	def initValues( self ):
		#print "GlobalMixer::initValues()"
		for i in range( self.clockselect.count() ):
			self.clocksource.insertItem( self.clockselect.getEnumLabel( i ) )
		self.clocksource.setCurrentItem( self.clockselect.selected() )
		self.txtNickname.setText( self.nickname.text() )

