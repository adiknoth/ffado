#
# Copyright (C) 2009 by Jonathan Woithe
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

from PyQt4 import QtGui

from PyQt4.QtCore import SIGNAL, SLOT, QObject, Qt
from PyQt4.QtGui import QWidget, QApplication
from ffado.config import *

from ffado.widgets.matrixmixer import MatrixMixer

import logging
log = logging.getLogger('rme')

# Model defines.  These must agree with what is used in rme_avdevice.h.
RME_MODEL_NONE      = 0x0000
RME_MODEL_FF800     = 0x0001
RME_MODEL_FF400     = 0x0002

class Rme(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/rme", self)

        self.init()

    def init(self):

        self.PhantomSwitches={
            self.phantom_0: ['/Control/Phantom', 0],
            self.phantom_1: ['/Control/Phantom', 1],
            self.phantom_2: ['/Control/Phantom', 2],
            self.phantom_3: ['/Control/Phantom', 3],
        }

        self.Switches={
            self.ff400_chan3_opt_instr: ['/Control/Chan3_opt_instr'],
            self.ff400_chan3_opt_pad:   ['/Control/Chan3_opt_pad'],
            self.ff400_chan4_opt_instr: ['/Control/Chan4_opt_instr'],
            self.ff400_chan4_opt_pad:   ['/Control/Chan4_opt_pad'],
        }

        self.Radiobuttons={
            self.level_in_lo_gain: ['/Control/Input_level', 1],
            self.level_in_p4dBu:   ['/Control/Input_level', 2],
            self.level_in_m10dBV:  ['/Control/Input_level', 3],

            self.level_out_hi_gain: ['/Control/Output_level', 1],
            self.level_out_p4dBu:   ['/Control/Output_level', 2],
            self.level_out_m10dBV:  ['/Control/Output_level', 3],

            self.phones_hi_gain: ['/Control/Phones_level', 1],
            self.phones_p4dBu:   ['/Control/Phones_level', 2],
            self.phones_m10dBV:  ['/Control/Phones_level', 3],
        }


        self.Gains={
            self.gain_mic1: ['/Control/Gains', 0],
            self.gain_mic2: ['/Control/Gains', 1],
            self.gain_input3: ['/Control/Gains', 2],
            self.gain_input4: ['/Control/Gains', 3],
        }

        # Other mixer variables
        self.is_streaming = 0
        self.sample_rate = 0
        self.model = 0
        self.tco_present = 0

    # Public slot: update phantom power hardware switches
    def updatePhantomSwitch(self, a0):
        sender = self.sender()
        # Value is the phantom switch value, with a corresponding enable
        # bit in the high 16 bit word
        val = (a0 << self.PhantomSwitches[sender][1]) | (0x00010000 << self.PhantomSwitches[sender][1])
        log.debug("phantom switch %d set to %d" % (self.PhantomSwitches[sender][1], a0))
        self.hw.setDiscrete(self.PhantomSwitches[sender][0], val)

    # Public slot: update generic switches
    def updateSwitch(self, a0):
        sender = self.sender()
        log.debug("switch %s set to %d" % (self.Switches[sender][0], a0))
        self.hw.setDiscrete(self.Switches[sender][0], a0)

    # Public slot: update generic radiobuttons
    def updateRadiobutton(self, a0):
        sender = self.sender()
        if (a0 != 0):
            # Only change the control state on a button being "checked"
            log.debug("radiobutton group %s set to %d" % (self.Radiobuttons[sender][0], self.Radiobuttons[sender][1]))
            self.hw.setDiscrete(self.Radiobuttons[sender][0], self.Radiobuttons[sender][1])

    # Public slot: update gains
    def updateGain(self, a0):
        sender = self.sender()
        log.debug("gain %s[%d] set to %d" % (self.Gains[sender][0], self.Gains[sender][1], a0))
        self.hw.setMatrixMixerValue(self.Gains[sender][0], 0, self.Gains[sender][1], a0)

    # Hide and disable a control
    def disable_hide(self,widget):
        widget.hide()
        widget.setEnabled(False)

    def initValues(self):

        # Initial experiments with the MatrixMixer widget
        # print self.hw.servername
        # print self.hw.basepath
        # self.inputmatrix = MatrixMixer(self.hw.servername, self.hw.basepath+"/Mixer/InputFaders", self)
        # self.mbox = QtGui.QVBoxLayout(self.mixer)
        # self.mbox.addWidget(self.inputmatrix)

        # Is the device streaming?
        #self.is_streaming = self.hw.getDiscrete('/Mixer/Info/IsStreaming')
        self.is_streaming = 0
        #log.debug("device streaming flag: %d" % (self.is_streaming))

        # Retrieve other device settings as needed and customise the UI
        # based on these options.
        self.model = self.hw.getDiscrete('/Control/Model')
        log.debug("device model identifier: %d" % (self.model))
        self.tco_present = self.hw.getDiscrete('/Control/TCO_present')
        log.debug("device has TCO: %d" % (self.tco_present))
        #self.sample_rate = self.hw.getDiscrete('/Mixer/Info/SampleRate')
        #log.debug("device sample rate: %d" % (self.sample_rate))

        # The Fireface-400 only has 2 phantom-capable channels
        if (self.model == RME_MODEL_FF400):
            self.disable_hide(self.phantom_2)
            self.disable_hide(self.phantom_3)
        else:
            self.phantom_0.setText("Mic 7")
            self.phantom_1.setText("Mic 8")
            self.phantom_2.setText("Mic 9")
            self.phantom_3.setText("Mic 10")

        # Instrument options, input jack selection controls and an ADAT2
        # input are applicable only to the FF800
        if (self.model != RME_MODEL_FF800):
            self.instrument_options_group.setEnabled(False)
            self.input_plug_select_group.setEnabled(False)
            self.sync_ref_adat2.setEnabled(False)
            self.sync_check_adat2_label.setEnabled(False)
            self.sync_check_adat2_status.setEnabled(False)

        # Only the FF400 has specific channel 3/4 options, input gain
        # controls and switchable phones level
        if (self.model != RME_MODEL_FF400):
            self.disable_hide(self.input_gains_group)
            self.disable_hide(self.channel_3_4_options_group)
            self.phones_level_group.setEnabled(False)

        # Get current hardware values and connect GUI element signals to 
        # their respective slots
        for ctrl, info in self.PhantomSwitches.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = (self.hw.getDiscrete(info[0]) >> info[1]) & 0x01
            log.debug("phantom switch %d is %d" % (info[1], val))
            if val:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updatePhantomSwitch)

        for ctrl, info in self.Switches.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = self.hw.getDiscrete(info[0])
            log.debug("switch %s is %d" % (info[0], val))
            if val:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateSwitch)

        for ctrl, info in self.Radiobuttons.iteritems():
            if (not(ctrl.isEnabled())):
                continue;
            # This is a touch wasteful since it means we retrieve the control
            # value once per radio button rather than once per radio button
            # group.  In time we might introduce radiobutton groupings in the
            # self.* datastructures to avoid this, but for the moment this is
            # easy and it works.
            val = self.hw.getDiscrete(info[0])
            if (val == info[1]):
                val = 1
            else:
                val = 0
            ctrl.setChecked(val)
            log.debug("Radiobutton %s[%d] is %d" % (info[0], info[1], val))
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateRadiobutton)

        for ctrl, info in self.Gains.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = self.hw.getMatrixMixerValue(info[0], 0, info[1])
            log.debug("gain %s[%d] is %d" % (info[0], info[1], val))
            ctrl.setValue(val);
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateGain)

# vim: et
