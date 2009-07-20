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

        # Other mixer variables
        self.is_streaming = 0
        self.sample_rate = 0
        self.model = 0

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
        #self.model = self.hw.getDiscrete('/Mixer/Info/Model')
        #log.debug("device model identifier: %d" % (self.model))
        #self.sample_rate = self.hw.getDiscrete('/Mixer/Info/SampleRate')
        #log.debug("device sample rate: %d" % (self.sample_rate))
