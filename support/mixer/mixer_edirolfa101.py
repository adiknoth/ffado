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
from mixer_edirolfa101ui import *

class EdirolFa101Control(EdirolFa101ControlUI):
    def __init__(self, parent = None, name = None, fl = 0):
        EdirolFa101ControlUI.__init__(self, parent, name, fl)

    def setVolumeIn1(self, vol):
            self.setVolume('in1', vol)

    def setVolumeIn2(self, vol):
            self.setVolume('in2', vol)

    def setVolumeIn3(self, vol):
            self.setVolume('in3', vol)

    def setVolumeIn4(self, vol):
            self.setVolume('in4', vol)

    def setVolumeIn5(self, vol):
            self.setVolume('in5', vol)

    def setVolumeIn6(self, vol):
            self.setVolume('in6', vol)

    def setVolumeIn7(self, vol):
            self.setVolume('in7', vol)

    def setVolumeIn8(self, vol):
            self.setVolume('in8', vol)
            
    def setVolumeIn9(self, vol):
            self.setVolume('in9', vol)

    def setVolumeIn10(self,vol):
            self.setVolume('in10', vol)

    def setVolume(self, name, vol):
            vol = -vol
            ctrl = self.VolumeControls[name]
            print "setting %s volume to %d" % (name, vol)
            self.hw.setContignuous(ctrl[0], vol, idx = ctrl[1])

    def init(self):
            print "Init Edirol FA-101 window"

            self.VolumeControls = {
                #          feature name, channel, qt slider
                'in1'  :   ['/Mixer/Feature_5', 1, self.sldInput1],
                'in2'  :   ['/Mixer/Feature_5', 2, self.sldInput2],
                'in3'  :   ['/Mixer/Feature_1', 1, self.sldInput3],
                'in4'  :   ['/Mixer/Feature_1', 2, self.sldInput4],
                'in5'  :   ['/Mixer/Feature_2', 1, self.sldInput5],
                'in6'  :   ['/Mixer/Feature_2', 2, self.sldInput6],
                'in7'  :   ['/Mixer/Feature_3', 1, self.sldInput7],
                'in8'  :   ['/Mixer/Feature_3', 2, self.sldInput8],
                'in9'  :   ['/Mixer/Feature_4', 1, self.sldInput9],
                'in10' :   ['/Mixer/Feature_4', 2, self.sldInput10],
                }

    def initValues(self):
            for name, ctrl in self.VolumeControls.iteritems():
                vol = self.hw.getContignuous(ctrl[0], idx = ctrl[1])
                print "%s volume is %d" % (name , vol)
                ctrl[2].setValue(-vol)
