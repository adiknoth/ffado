#
# Copyright (C) 2005-2008 by Pieter Palmers
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
from ffado.config import *

import logging
log = logging.getLogger('phase88')

class Phase88Control(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/phase88", self)

        self.VolumeControls={
            'master':    ['/Mixer/Feature_Volume_1', self.sldInputMaster], 
            'line12' :   ['/Mixer/Feature_Volume_2', self.sldInput12],
            'line34' :   ['/Mixer/Feature_Volume_3', self.sldInput34],
            'line56' :   ['/Mixer/Feature_Volume_4', self.sldInput56],
            'line78' :   ['/Mixer/Feature_Volume_5', self.sldInput78],
            'spdif' :    ['/Mixer/Feature_Volume_6', self.sldInputSPDIF],
            'waveplay' : ['/Mixer/Feature_Volume_7', self.sldInputWavePlay],
            }

        self.SelectorControls={
            'outassign':    ['/Mixer/Selector_6', self.comboOutAssign], 
            'inassign':     ['/Mixer/Selector_7', self.comboInAssign], 
            'externalsync': ['/Mixer/Selector_8', self.comboExtSync], 
            'syncsource':   ['/Mixer/Selector_9', self.comboSyncSource], 
            'frontback':    ['/Mixer/Selector_10', self.comboFrontBack], 
        }

    def switchFrontState(self,a0):
        self.setSelector('frontback', a0)

    def switchOutAssign(self,a0):
        self.setSelector('outassign', a0)

    def switchWaveInAssign(self,a0):
        self.setSelector('inassign', a0)

    def switchSyncSource(self,a0):
        self.setSelector('syncsource', a0)

    def switchExtSyncSource(self,a0):
        self.setSelector('externalsync', a0)

    def setVolume12(self,a0):
        self.setVolume('line12', a0)

    def setVolume34(self,a0):
        self.setVolume('line34', a0)

    def setVolume56(self,a0):
        self.setVolume('line56', a0)

    def setVolume78(self,a0):
        self.setVolume('line78', a0)

    def setVolumeSPDIF(self,a0):
        self.setVolume('spdif', a0)

    def setVolumeWavePlay(self,a0):
        self.setVolume('waveplay', a0)

    def setVolumeMaster(self,a0):
        self.setVolume('master', a0)

    def setVolume(self,a0,a1):
        name=a0
        vol = -a1
        log.debug("setting %s volume to %d" % (name, vol))
        self.hw.setContignuous(self.VolumeControls[name][0], vol)

    def setSelector(self,a0,a1):
        name=a0
        state = a1
        log.debug("setting %s state to %d" % (name, state))
        self.hw.setDiscrete(self.SelectorControls[name][0], state)

    def initValues(self):
        for name, ctrl in self.VolumeControls.iteritems():
            vol = self.hw.getContignuous(ctrl[0])
            log.debug("%s volume is %d" % (name , vol))
            ctrl[1].setValue(-vol)

        for name, ctrl in self.SelectorControls.iteritems():
            state = self.hw.getDiscrete(ctrl[0])
            log.debug("%s state is %d" % (name , state))
            ctrl[1].setCurrentIndex(state)

# vim: et
