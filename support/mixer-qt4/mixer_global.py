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

from PyQt4.QtGui import QWidget, QMessageBox
from mixer_globalui import Ui_GlobalMixerUi

import logging
log = logging.getLogger('global')

class GlobalMixer( QWidget, Ui_GlobalMixerUi ):
    def __init__( self, parent ):
        QWidget.__init__( self, parent )
        self.setupUi(self)

    def clockChanged( self, clock ):
        #print "updateClockSource( " + str(clock) + " )"
        if self.clockselect.canChangeValue():
            self.clockselect.select( clock )
        else:
            msg = QMessageBox()
            msg.question( msg, "Error", \
                "<qt>Clock source change not permitted. Is streaming active?</qt>", \
                QMessageBox.Ok )
            self.clocksource.setEnabled(False)
            return

        selected = self.clockselect.selected()
        if selected != clock:
            clockname = self.clockselect.getEnumLabel( clock )
            msg = QMessageBox()
            msg.question( msg, "Failed to select clock source", \
                "<qt>Could not select %s as clock source.</qt>" % clockname, \
                QMessageBox.Ok )
            self.clocksource.setCurrentIndex( selected )

    def samplerateChanged( self, sr ):
        log.debug("samplerateChanged( " + str(sr) + " )")
        if self.samplerateselect.canChangeValue():
            self.samplerateselect.select( sr )
        else:
            msg = QMessageBox()
            msg.question( msg, "Error", \
                "<qt>Sample rate change not permitted. Is streaming active?</qt>", \
                QMessageBox.Ok )
            self.samplerate.setEnabled(False)
            return

        selected = self.samplerateselect.selected()
        if selected != sr:
            srname = self.samplerateselect.getEnumLabel( sr )
            msg = QMessageBox()
            msg.question( msg, "Failed to select sample rate", \
                "<qt>Could not select %s as samplerate.</qt>" % srname, \
                QMessageBox.Ok )
            self.samplerate.setCurrentIndex( selected )

    def nicknameChanged( self, name ):
        #print "nicknameChanged( %s )" % name
        asciiData = name.toAscii()
        self.nickname.setText( asciiData.data() )

    def initValues( self ):
        #print "GlobalMixer::initValues()"
        nb_clocks = self.clockselect.count()
        for i in range( nb_clocks ):
            self.clocksource.insertItem( nb_clocks, self.clockselect.getEnumLabel( i ) )
        self.clocksource.setCurrentIndex( self.clockselect.selected() )
        
        nb_rates = self.samplerateselect.count()
        for i in range( nb_rates ):
            self.samplerate.insertItem( nb_rates, self.samplerateselect.getEnumLabel( i ) )
        self.samplerate.setCurrentIndex( self.samplerateselect.selected() )

        self.txtNickname.setText( self.nickname.text() )

    def polledUpdate(self):
        self.samplerate.setEnabled(self.samplerateselect.canChangeValue())
        self.clocksource.setEnabled(self.clockselect.canChangeValue())
