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

from qt import *
from mixer_edirolfa66ui import *

class EdirolFa66Control(EdirolFa66ControlUI):
    def __init__(self, parent = None, name = None, fl = 0):
        EdirolFa66ControlUI.__init__(self, parent, name, fl)

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

    def setValue(self, name, val):
        val = -val
        ctrl = self.VolumeControls[name]
        print "setting %s to %d" % (name, val)
        self.hw.setContignuous(ctrl[0], val, idx = ctrl[1])

    def init(self):
        print "Init Edirol FA-66 window"

        self.VolumeControls = {
            #          feature name, channel, qt slider
            'vol1'  :   ['/Mixer/Feature_Volume_1', 1, self.sldInput1],
            'vol2'  :   ['/Mixer/Feature_Volume_1', 2, self.sldInput2],
            'vol3'  :   ['/Mixer/Feature_Volume_2', 1, self.sldInput3],
            'vol4'  :   ['/Mixer/Feature_Volume_2', 2, self.sldInput4],
            'vol5'  :   ['/Mixer/Feature_Volume_3', 1, self.sldInput5],
            'vol6'  :   ['/Mixer/Feature_Volume_3', 2, self.sldInput6],

            'bal1'  :   ['/Mixer/Feature_LRBalance_1', 1, self.sldBal1],
            'bal2'  :   ['/Mixer/Feature_LRBalance_1', 2, self.sldBal2],
            'bal3'  :   ['/Mixer/Feature_LRBalance_2', 1, self.sldBal3],
            'bal4'  :   ['/Mixer/Feature_LRBalance_2', 2, self.sldBal4],
            'bal5'  :   ['/Mixer/Feature_LRBalance_3', 1, self.sldBal5],
            'bal6'  :   ['/Mixer/Feature_LRBalance_3', 2, self.sldBal6],
            }

    def initValues(self):
        for name, ctrl in self.VolumeControls.iteritems():
            val = self.hw.getContignuous(ctrl[0], idx = ctrl[1])
            print "%s value is %d" % (name , val)

            # Workaround: The current value is not properly initialized
            # on the device and returns after bootup always 0.
            # Though we happen to know what the correct value should
            # be therefore we overwrite the 0 
            if name[0:3] == 'bal' and val == 0:
                if ctrl[1] == 1:
                    val = 32512
                else:
                    val = -32768
                    
            ctrl[2].setValue(-val)
