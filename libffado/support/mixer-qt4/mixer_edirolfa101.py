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
from ffadomixer_config import *

import logging
log = logging.getLogger('edirolfa101')

class EdirolFa101Control(QWidget):
    def __init__(self, parent = None):
        QWidget.__init__(self, parent)
        uicLoad("mixer_edirolfa101", self)

    def setVolumeIn1(self, vol):
        self.setValue('vol1', vol)

    def setVolumeIn2(self, vol):
        self.setValue('vol2', vol)

    def setVolumeIn3(self, vol):
        self.setValue('vol3', vol)

    def setVolumeIn4(self, vol):
        self.setValue('vol4', vol)

    def setVolumeIn5(self, vol):
        self.setValue('vol5', vol)

    def setVolumeIn6(self, vol):
        self.setValue('vol6', vol)

    def setVolumeIn7(self, vol):
        self.setValue('vol7', vol)

    def setVolumeIn8(self, vol):
        self.setValue('vol8', vol)

    def setVolumeIn9(self, vol):
        self.setValue('vol9', vol)

    def setVolumeIn10(self,vol):
        self.setValue('vol10', vol)

    def setBalanceIn1(self, bal):
        self.setValue('bal1', bal)

    def setBalanceIn2(self, bal):
        self.setValue('bal2', bal)

    def setBalanceIn3(self, bal):
        self.setValue('bal3', bal)

    def setBalanceIn4(self, bal):
        self.setValue('bal4', bal)

    def setBalanceIn5(self, bal):
        self.setValue('bal5', bal)

    def setBalanceIn6(self, bal):
        self.setValue('bal6', bal)

    def setBalanceIn7(self, bal):
        self.setValue('bal7', bal)

    def setBalanceIn8(self, bal):
        self.setValue('bal8', bal)

    def setBalanceIn9(self, bal):
        self.setValue('bal9', bal)

    def setBalanceIn10(self,bal):
        self.setValue('bal10', bal)

    def getIndexByName(self, name):
        index = int(name.lstrip('volba')) - 1
        streamingMap    = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]
        nonStreamingMap = [ 9, 10, 1, 2, 3, 4, 5, 6, 7, 8 ]

        if self.is_streaming:
            index = streamingMap[index]
        else:
            index = nonStreamingMap[index]
        return index
        
    def getWidgetByName(self, name):
        index = self.getIndexByName(name)
        widgetName = ''
        if name[0:3] == 'vol':
            widgetName = 'sldInput%d' % (index)
        else:
            widgetName = 'sldBal%d' % (index)
        return getattr(self, widgetName)
            
    def getFeatureByName(self, name):
        index = self.getIndexByName(name)
        featureName = ''
        if name[0:3] == 'vol':
            featureName = '/Mixer/Feature_Volume_%d' % ((index + 1) / 2)
        else:
            featureName = '/Mixer/Feature_LRBalance_%d' % ((index + 1) / 2)
        return featureName

    def getChannelIndexByName(self, name):
        index = self.getIndexByName(name)
        return ((index - 1) % 2) + 1

    def setValue(self, name, val):
        log.debug("setting %s to %d" % (name, val))
        self.updateStreamingState()
        feature = self.getFeatureByName(name)
        widget = self.getWidgetByName(name)
        channel = self.getChannelIndexByName(name)
        self.hw.setContignuous(feature, val, idx = channel)

    def updateStreamingState(self):
        ss = self.streamingstatus.selected()
        ss_txt = self.streamingstatus.getEnumLabel(ss)
        if ss_txt != 'Idle':
            self.is_streaming = True
        else:
            self.is_streaming = False
        
    def initValues(self):
        self.updateStreamingState()
        
        for i in range(1, 11):
            name = 'vol%d' % i
            feature = self.getFeatureByName(name)
            widget = self.getWidgetByName(name)
            channel = self.getChannelIndexByName(name)

            val = self.hw.getContignuous(feature, idx = channel)
            log.debug("%s value is %d" % (name , val))
            widget.setValue(val)
            
        for i in range(1, 11):
            name = 'bal%d' % i
            feature = self.getFeatureByName(name)
            widget = self.getWidgetByName(name)
            channel = self.getChannelIndexByName(name)

            val = self.hw.getContignuous(feature, idx = channel)
            # Workaround: The current value is not properly initialized
            # on the device and returns after bootup always 0.
            # Though we happen to know what the correct value should
            # be therefore we overwrite the 0 
            if channel == 1:
                val = 32512
            else:
                val = -32768
            log.debug("%s value is %d" % (name , val))
            widget.setValue(val)

# vim: et
