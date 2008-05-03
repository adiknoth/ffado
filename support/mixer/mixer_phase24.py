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
from mixer_phase24ui import *

class PhaseX24Control(PhaseX24ControlUI):
    def __init__(self,parent = None,name = None,fl = 0):
        PhaseX24ControlUI.__init__(self,parent,name,fl)
        self.init()

    # public slot
    def setVolume12(self,a0):
        self.setVolume('out12', a0)

    # public slot
    def setVolume34(self,a0):
        self.setVolume('out34', a0)

    # public slot
    def setVolumeLineIn(self,a0):
        self.setVolume('analogin', a0)

    # public slot
    def setVolumeSPDIFOut(self,a0):
        self.setVolume('outspdif', a0)

    # public slot
    def setVolumeSPDIFIn(self,a0):
        self.setVolume('spdifin', a0)

    # public slot
    def setVolumeMaster(self,a0):
        self.setVolume('master', a0)

    # public slot
    def setLineLevel(self,a0):
        print "setting line level to %d" % (a0 * -768)
        self.hw.setContignuous('/Mixer/Feature_2', a0 * -768)

    # public slot
    def setFrontLevel(self,a0):
        if(a0 == 0):
            print "setting front level to %d" % (0)
            self.hw.setContignuous('/Mixer/Feature_8', 0)
        else:
            print "setting front level to %d" % (1536)
            self.hw.setContignuous('/Mixer/Feature_8', 1536)

    # public slot
    def setOutSource12(self,a0):
        self.setSelector('outsource12', a0)

    # public slot
    def setOutSource34(self,a0):
        self.setSelector('outsource34', a0)

    # public slot
    def setOutSourceSPDIF(self,a0):
        self.setSelector('outsourcespdif', a0)

    # public slot
    def setSyncSource(self,a0):
        self.setSelector('syncsource', a0)


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
            print "Init PhaseX24 mixer window"

            self.VolumeControls={
                'master':         ['/Mixer/Feature_1', self.sldMaster], 
                'analogin' :      ['/Mixer/Feature_6', self.sldLineIn],
                'spdifin' :       ['/Mixer/Feature_7', self.sldSPDIFIn],
                'out12' :         ['/Mixer/Feature_3', self.sldInput12],
                'out34' :         ['/Mixer/Feature_4', self.sldInput34],
                'outspdif' :      ['/Mixer/Feature_5', self.sldSPDIFOut],
                }

            self.SelectorControls={
                'outsource12':    ['/Mixer/Selector_1', self.cmbOutSource12], 
                'outsource34':    ['/Mixer/Selector_2', self.cmbOutSource34], 
                'outsourcespdif': ['/Mixer/Selector_3', self.cmbOutSourceSPDIF], 
                'syncsource':     ['/Mixer/Selector_4', self.cmbSetSyncSource], 
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

            val=self.hw.getContignuous('/Mixer/Feature_2')/-768
            if val>4:
                self.cmbFrontLevel.setCurrentItem(4)
            else:
                self.cmbFrontLevel.setCurrentItem(val)

            val=self.hw.getContignuous('/Mixer/Feature_8')
            if val>0:
                self.cmbFrontLevel.setCurrentItem(1)
            else:
                self.cmbFrontLevel.setCurrentItem(0)
