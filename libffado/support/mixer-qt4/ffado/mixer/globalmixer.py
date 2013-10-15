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

from PyQt4.QtCore import QObject, pyqtSignature
from PyQt4.QtGui import QWidget, QMessageBox
from ffado.config import *

import logging
log = logging.getLogger('global')

class GlobalMixer(QWidget):
    def __init__(self, parent, name=None):
        QWidget.__init__(self, parent)
        uicLoad("ffado/mixer/globalmixer", self)
        self.setName(name)

    def setName(self,name):
        if name is not None:
            self.lblName.setText(name)
            self.lblName.show()
        else:
            self.lblName.hide()

    @pyqtSignature("int")
    def on_clocksource_activated( self, clock ):
        #log.debug("updateClockSource( " + str(clock) + " )")
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

    @pyqtSignature("int")
    def on_samplerate_activated( self, sr ):
        log.debug("on_samplerate_activated( " + str(sr) + " )")
        # If there's no clock, don't bother trying to set the sample rate
        if (self.no_clock == 1):
            return
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

    @pyqtSignature("")
    def on_txtNickname_returnPressed( self ):
        if self.nickname.canChangeValue():
            asciiData = self.txtNickname.text().toAscii()
            self.nickname.setText( asciiData.data() )
        else:
            self.txtNickname.setText( self.nickname.text() )

    def refreshSampleRates( self ):
        no_clock_status = 0
        n_rates = self.samplerateselect.count()
        if (n_rates<1 or self.samplerateselect.getEnumLabel(0)=="0"):
            no_clock_status = 1;

        # Except for changes to the "no clock" status (where the number of
        # "frequencies" can remain at 1), the following test won't account
        # for cases where the frequency list changes but the total number of
        # frequencies remains the same.  If a device comes along for which
        # this is a problem, an alternative approach will be needed.
        if (no_clock_status!=self.no_clock or n_rates!=self.num_rates):
            self.no_clock = 0;
            self.num_rates = n_rates
            self.samplerate.clear()
            for i in range( self.num_rates ):
                label = self.samplerateselect.getEnumLabel( i )
                if (label == "0"):
                    label = "No clock found";
                    self.no_clock = 1;
                self.samplerate.insertItem( self.num_rates, label )
            if (self.no_clock != 1):
                self.samplerate.setCurrentIndex( self.samplerateselect.selected() )
            else:
                self.samplerate.setCurrentIndex(0);

    def initValues( self ):
        #print "GlobalMixer::initValues()"
        nb_clocks = self.clockselect.count()
        for i in range( nb_clocks ):
            self.clocksource.insertItem( nb_clocks, self.clockselect.getEnumLabel( i ) )
        self.clocksource.setCurrentIndex( self.clockselect.selected() )

        self.no_clock = 0;
        self.num_rates = -1;
        self.refreshSampleRates();

        self.txtNickname.setText( self.nickname.text() )

        self.samplerate.setEnabled(self.samplerateselect.canChangeValue())
        self.clocksource.setEnabled(self.clockselect.canChangeValue())
        if self.nickname.canChangeValue():
            self.txtNickname.setEnabled(True)
        else:
            self.txtNickname.setEnabled(False)

        self.streaming_status = self.streamingstatus.selected()

    def polledUpdate(self):
        self.samplerate.setEnabled(self.samplerateselect.canChangeValue())
        self.clocksource.setEnabled(self.clockselect.canChangeValue())
        self.txtNickname.setEnabled(self.nickname.canChangeValue())
        ss = self.streamingstatus.selected()
        ss_txt = self.streamingstatus.getEnumLabel(ss)
        if ss_txt == 'Idle':
            self.chkStreamIn.setChecked(False)
            self.chkStreamOut.setChecked(False)
        elif ss_txt == 'Sending':
            self.chkStreamIn.setChecked(False)
            self.chkStreamOut.setChecked(True)
        elif ss_txt == 'Receiving':
            self.chkStreamIn.setChecked(True)
            self.chkStreamOut.setChecked(False)
        elif ss_txt == 'Both':
            self.chkStreamIn.setChecked(True)
            self.chkStreamOut.setChecked(True)

        if (ss!=self.streaming_status and ss_txt!='Idle'):
            sr = self.samplerate.currentIndex()
            self.samplerate.setCurrentIndex( self.samplerateselect.selected() )
            # Check (and update) if device configuration needs to be updated
            if ( self.samplerateselect.devConfigChanged(sr) ):
              log.debug(" Mixer configuration would need to be updated ")
        self.streaming_status = ss

        # Allow for devices whose sample rates can change dynamically (for
        # example, in response to changes in external clock frequency)
        self.refreshSampleRates();

# vim: et
