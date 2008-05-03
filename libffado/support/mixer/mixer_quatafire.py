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

from qt import *
from mixer_quatafireui import *

class QuataFireMixer(QuataFireMixerUI):
    def __init__(self,parent = None,name = None,fl = 0):
        QuataFireMixerUI.__init__(self,parent,name,fl)

    def init(self):
            print "Init Quatafire mixer window"

            self.VolumeControls={
                    self.sldCh1: ['/Mixer/Feature_1', 1],
                    self.sldCh2: ['/Mixer/Feature_1', 2],
                    self.sldCh34: ['/Mixer/Feature_2', 0],
                    self.sldCh56: ['/Mixer/Feature_3', 0],
                    self.sldDawAll: ['/Mixer/Feature_4', 0],
                    self.sldDawCH1: ['/Mixer/Feature_4', 1],
                    self.sldDawCH2: ['/Mixer/Feature_4', 2],
                    self.sldDawCH3: ['/Mixer/Feature_4', 3],
                    self.sldDawCH4: ['/Mixer/Feature_4', 4],
                    self.sldDawCH5: ['/Mixer/Feature_4', 5],
                    self.sldDawCH6: ['/Mixer/Feature_4', 6],
                    self.sldDawCH7: ['/Mixer/Feature_4', 7],
                    self.sldDawCH8: ['/Mixer/Feature_4', 8],
                    }
            self.PanControls={
                    #self.dialCh1: ['/Mixer/Feature_1'],
                    #self.dialCh2: ['/Mixer/Feature_1'],
                    self.dialCh34: ['/Mixer/Feature_2'],
                    self.dialCh56: ['/Mixer/Feature_3'],
                    }

    def updateVolume(self,a0):
        sender = self.sender()
        vol = -a0
        print "setting %s volume to %d" % (self.VolumeControls[sender][0], vol)
        self.hw.setContignuous(self.VolumeControls[sender][0], vol, self.VolumeControls[sender][1])
        
    def updatePan(self,a0):
        sender = self.sender()
        pan_left = a0
        if pan_left < 0:
            pan_left = 0

        pan_right = -a0
        if pan_right < 0:
            pan_right = 0

        print "setting %s pan left to %d" % (self.PanControls[sender][0], -pan_left)
        self.hw.setContignuous(self.PanControls[sender][0], -pan_left, 1)
        print "setting %s pan right to %d" % (self.PanControls[sender][0], -pan_right)
        self.hw.setContignuous(self.PanControls[sender][0], -pan_right, 2)

    def initValues(self):
        for ctrl, info in self.VolumeControls.iteritems():
            vol = self.hw.getContignuous(self.VolumeControls[ctrl][0], self.VolumeControls[ctrl][1])
            val = -vol
            print "%s volume is %d, set to %d" % (ctrl.name(), vol, val)
            ctrl.setValue(val)

            # connect the UI element
            QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateVolume)

        for ctrl, info in self.PanControls.iteritems():
            pan_left = self.hw.getContignuous(self.PanControls[ctrl][0], 1)
            pan_right = self.hw.getContignuous(self.PanControls[ctrl][0], 2)

            print "%s pan left is %d" % (ctrl.name() , pan_left)
            print "%s pan right is %d" % (ctrl.name() , pan_right)

            if pan_left == 0:
                val = pan_right
            else:
                val = -pan_left
            
            ctrl.setValue(val)
            # connect the UI element
            QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updatePan)

