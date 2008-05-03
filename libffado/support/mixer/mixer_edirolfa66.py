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
    def __init__(self,parent = None,name = None,fl = 0):
        EdirolFa66ControlUI.__init__(self,parent,name,fl)

    def setComboMixSource(self,a0):
            self.setSelector('line34source', a0)

    def setVolumeIn12(self,a0):
            self.setVolume('line12', a0)

    def setVolumeIn34(self,a0):
            self.setVolume('line34', a0)

    def setVolumeIn56(self,a0):
            self.setVolume('spdif', a0)

    def setVolume(self,a0,a1):
            name = a0
            vol = -a1
            print "setting %s volume to %d" % (name, vol)
            self.hw.setContignuous(self.VolumeControls[name][0], vol)

    def init(self):
            print "Init Edirol FA-66 window"

            self.VolumeControls={
                'line12'  :   ['/Mixer/Feature_1', self.sldInput12],
                'line34'  :   ['/Mixer/Feature_2', self.sldInput34],
                'spdif'   :   ['/Mixer/Feature_3', self.sldInput56],
                }

    def initValues(self):
            for name, ctrl in self.VolumeControls.iteritems():
                vol = self.hw.getContignuous(ctrl[0])
                print "%s volume is %d" % (name , vol)
                ctrl[1].setValue(-vol)
