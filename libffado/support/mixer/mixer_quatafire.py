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
    def __init__(self,parent = None,name = None,modal = 0,fl = 0):
        QuataFireMixerUI.__init__(self,parent,name,modal,fl)

    def init(self):
            print "Init Quatafire mixer window"

            self.VolumeControls={
                    self.sldFeature1: ['/Mixer/Feature_1'],
                    self.sldFeature2: ['/Mixer/Feature_2'],
                    self.sldFeature3: ['/Mixer/Feature_3'],
                    self.sldFeature4: ['/Mixer/Feature_4'],
                    }

    def updateVolume(self,a0):
        sender = self.sender()
        vol = -a0
        print "setting %s volume to %d" % (self.VolumeControls[sender][0], vol)
        self.hw.setContignuous(self.VolumeControls[sender][0], vol)

    def initValues(self):
        for ctrl, info in self.VolumeControls.iteritems():
            vol = self.hw.getContignuous(self.VolumeControls[ctrl][0])

            print "%s volume is %d" % (ctrl.name() , 0x7FFF-vol)
            ctrl.setValue(0x7FFF-vol)

            # connect the UI element
            QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateVolume)

