#
# Copyright (C) 2005-2008 by Daniel Wagner
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

from PyQt4.QtCore import SIGNAL, SLOT, QObject
from PyQt4.QtGui import QWidget
from mixer_bcoaudio5ui import *
import logging
log = logging.getLogger('bridgeco')

class BCoAudio5Control(QWidget, Ui_BCoAudio5ControlUI):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        self.setupUi(self)

        self.VolumeControls={
            'in_line12'  :   ['/Mixer/Feature_Volume_1', self.sldInput12],
            'in_line34'  :   ['/Mixer/Feature_Volume_2', self.sldInput34],
            'in_spdif'   :   ['/Mixer/Feature_Volume_3', self.sldInputSPDIF],
            'out_line12' :   ['/Mixer/Feature_Volume_6', self.sldOutput12],
            'out_line34' :   ['/Mixer/Feature_Volume_7', self.sldOutput34],
            'cross_a'    :   ['/Mixer/Feature_Volume_4', self.sldCrossA],
            'cross_b'    :   ['/Mixer/Feature_Volume_5', self.sldCrossB],
            }

        self.ComboControls={
            'line34source':   ['/Mixer/Selector_1', self.comboMixSource], 
        }

    def setComboMixSource(self,a0):
        self.setSelector('line34source', a0)

    def setVolumeIn12(self,a0):
        self.setVolume('in_line12', a0)

    def setVolumeIn34(self,a0):
        self.setVolume('in_line34', a0)

    def setVolumeInSPDIF(self,a0):
        self.setVolume('in_spdif', a0)

    def setVolumeOut12(self,a0):
        self.setVolume('out_line12', a0)

    def setVolumeOut34(self,a0):
        self.setVolume('out_line34', a0)

    def setCrossA(self,a0):
        self.setVolume('cross_a', a0)

    def setCrossB(self,a0):
        self.setVolume('cross_b', a0)

    def setVolume(self,a0,a1):
        name = a0
        vol = -a1
        log.debug("setting %s volume to %d" % (name, vol))
        self.hw.setContignuous(self.VolumeControls[name][0], vol)

    def setSelector(self,a0,a1):
        name = a0
        state = a1
        log.debug("setting %s state to %d" % (name, state))
        self.hw.setDiscrete(self.ComboControls[name][0], state)
        # verify
        state = self.hw.getDiscrete(self.ComboControls[name][0])
        self.hw.setDiscrete(self.ComboControls[name][0], state)

    def initValues(self):
        for name, ctrl in self.VolumeControls.iteritems():
            vol = self.hw.getContignuous(ctrl[0])
            log.debug("%s volume is %d" % (name , vol))
            ctrl[1].setValue(-vol)

        for name, ctrl in self.ComboControls.iteritems():
            state = self.hw.getDiscrete(ctrl[0])
            log.debug("%s state is %d" % (name , state))
            ctrl[1].setCurrentIndex( state )

