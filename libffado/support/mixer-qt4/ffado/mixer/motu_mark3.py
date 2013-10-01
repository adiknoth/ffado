#
# Copyright (C) 2005-2008 by Pieter Palmers
# Copyright (C) 2008, 2013 by Jonathan Woithe
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
from ffado.config import *

import logging
log = logging.getLogger('motu_mark3')

# Model defines.  These must agree with what is used in motu_avdevice.h.
MOTU_MODEL_NONE             = 0x0000
MOTU_MODEL_828mkII          = 0x0001
MOTU_MODEL_TRAVELER         = 0x0002
MOTU_MODEL_ULTRALITE        = 0x0003
MOTU_MODEL_8PRE             = 0x0004
MOTU_MODEL_828MkI           = 0x0005
MOTU_MODEL_896HD            = 0x0006
MOTU_MODEL_828mk3           = 0x0007
MOTU_MODEL_ULTRALITEmk3     = 0x0008
MOTU_MODEL_ULTRALITEmk3_HYB = 0x0009
MOTU_MODEL_TRAVELERmk3      = 0x000a
MOTU_MODEL_896HDmk3         = 0x000b

class Motu_Mark3(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/motu_mark3", self)

        self.init()

    def init(self):
        # Initialise any object members as needed

        # Other mixer variables
        self.is_streaming = 0
        self.sample_rate = 0
        self.model = MOTU_MODEL_NONE

    # Hide and disable a control
    def disable_hide(self,widget):
        widget.hide()
        widget.setEnabled(False)

    def initValues_g3(self):
        # Set up widgets for generation-3 (aka Mark-3) devices
        return

    def initValues(self):
        # Is the device streaming?
        self.is_streaming = self.hw.getDiscrete('/Mixer/Info/IsStreaming')
        log.debug("device streaming flag: %d" % (self.is_streaming))

        # Retrieve other device settings as needed and customise the UI
        # based on these options.
        self.model = self.hw.getDiscrete('/Mixer/Info/Model')
        log.debug("device model identifier: %d" % (self.model))
        self.sample_rate = self.hw.getDiscrete('/Mixer/Info/SampleRate')
        log.debug("device sample rate: %d" % (self.sample_rate))

        self.initValues_g3()
