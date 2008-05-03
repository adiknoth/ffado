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
from mixer_mackie_genericui import *

class MackieGenericControl(MackieGenericControlUI):
    def __init__(self,parent = None,name = None,fl = 0):
        MackieGenericControlUI.__init__(self,parent,name,fl)

    # public slot
    def setFB1(self,a0):
        self.setVolume('FB1', a0)

    # public slot
    def setFB2(self,a0):
        self.setVolume('FB2', a0)

    def setVolume(self,a0,a1):
            name=a0
            vol = -a1
            print "setting %s volume to %d" % (name, vol)
            self.hw.setContignuous(self.VolumeControls[name][0], vol)

    def setSelector(self,a0,a1):
            name=a0
            state = a1
            print "setting %s state to %d" % (name, state)
            self.hw.setDiscrete(self.SelectorControls[name][0], state)

    def init(self):
            print "Init Mackie Generic mixer window"

            self.VolumeControls={
                'FB1':    ['/Mixer/Feature_1', self.sldFB1], 
                'FB2' :   ['/Mixer/Feature_2', self.sldFB2],
                }

            self.SelectorControls={

            }

    def initValues(self):
            for name, ctrl in self.VolumeControls.iteritems():
                vol = self.hw.getContignuous(ctrl[0])
                print "%s volume is %d" % (name , vol)
                ctrl[1].setValue(-vol)

            for name, ctrl in self.SelectorControls.iteritems():
                state = self.hw.getDiscrete(ctrl[0])
                print "%s state is %d" % (name , state)
                ctrl[1].setCurrentItem(state)    
