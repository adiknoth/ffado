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

from PyQt4.QtCore import SIGNAL, SLOT, QObject, Qt
from PyQt4.QtGui import QWidget, QApplication
from mixer_rmeui import *

import logging
log = logging.getLogger('rme')

# Model defines.  These must agree with what is used in rme_avdevice.h.
RME_MODEL_NONE      = 0x0000
RME_MODEL_FF800     = 0x0001
RME_MODEL_FF400     = 0x0002

class RmeMixer(QWidget, Ui_RmeMixerUI):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        self.setupUi(self)

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

    # Hide and disable a control
    def disable_hide(self,widget):
        widget.hide()
        widget.setEnabled(False)

    def initValues(self):
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
