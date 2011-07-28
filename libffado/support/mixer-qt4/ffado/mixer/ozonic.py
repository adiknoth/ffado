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
from ffado.config import *
import logging
log = logging.getLogger('panelmanager')

class Ozonic(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/ozonic", self)

        self.Controls={
            'fader1' :   ['/Mixer/Feature_Volume_1', 1,  self.fader1],
            'fader2' :   ['/Mixer/Feature_Volume_1', 2,  self.fader2],
            'fader3' :   ['/Mixer/Feature_Volume_2', 1,  self.fader3],
            'fader4' :   ['/Mixer/Feature_Volume_2', 2,  self.fader4],
            'fader5' :   ['/Mixer/Feature_Volume_3', 1,  self.fader5],
            'fader6' :   ['/Mixer/Feature_Volume_3', 2,  self.fader6],
            'fader7' :   ['/Mixer/Feature_Volume_4', 1,  self.fader7],
            'fader8' :   ['/Mixer/Feature_Volume_4', 2,  self.fader8],
            'pan1' :   ['/Mixer/Feature_LRBalance_1', 1,  self.pan1],
            'pan2' :   ['/Mixer/Feature_LRBalance_1', 2,  self.pan2],
            'pan3' :   ['/Mixer/Feature_LRBalance_2', 1,  self.pan3],
            'pan4' :   ['/Mixer/Feature_LRBalance_2', 2,  self.pan4],
            'pan5' :   ['/Mixer/Feature_LRBalance_3', 1,  self.pan5],
            'pan6' :   ['/Mixer/Feature_LRBalance_3', 2,  self.pan6],
            'pan7' :   ['/Mixer/Feature_LRBalance_4', 1,  self.pan7],
            'pan8' :   ['/Mixer/Feature_LRBalance_4', 2,  self.pan8],
            }

    def setFader1(self,a0):
        self.setValue('fader1', a0)

    def setFader2(self,a0):
        self.setValue('fader2', a0)

    def setFader3(self,a0):
        self.setValue('fader3', a0)

    def setFader4(self,a0):
        self.setValue('fader4', a0)

    def setFader5(self,a0):
        self.setValue('fader5', a0)

    def setFader6(self,a0):
        self.setValue('fader6', a0)

    def setFader7(self,a0):
        self.setValue('fader7', a0)

    def setFader8(self,a0):
        self.setValue('fader8', a0)

    def setPan1(self,a0):
        self.setValue('pan1', a0)

    def setPan2(self,a0):
        self.setValue('pan2', a0)

    def setPan3(self,a0):
        self.setValue('pan3', a0)

    def setPan4(self,a0):
        self.setValue('pan4', a0)

    def setPan5(self,a0):
        self.setValue('pan5', a0)

    def setPan6(self,a0):
        self.setValue('pan6', a0)

    def setPan7(self,a0):
        self.setValue('pan7', a0)

    def setPan8(self,a0):
        self.setValue('pan8', a0)

    def setValue(self,a0,a1):
        name = a0
        val = a1
        ctrl = self.Controls[name]
        if name[0:3] == 'pan' :
            val = -a1
        log.debug("setting %s to %d" % (name, val))
        self.hw.setContignuous(ctrl[0],  val,  idx = ctrl[1])

    def initValues(self):
        for name, ctrl in self.Controls.iteritems():
            val = self.hw.getContignuous(ctrl[0],  idx = ctrl[1])
            if name[0:3] == 'pan' :
                val = -val
            log.debug("%s value is %d" % (name , val))
            ctrl[2].setValue(val)

# vim: et
