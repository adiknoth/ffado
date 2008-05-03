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
from mixer_bcoaudio5ui import *

class BCoAudio5Control(BCoAudio5ControlUI):
    def __init__(self,parent = None,name = None,fl = 0):
        BCoAudio5ControlUI.__init__(self,parent,name,fl)

    def setComboMixSource(self,a0):
            self.setSelector('line34source', a0)

    def setVolumeIn12(self,a0):
            self.setVolume('in_line12', a0)

    def setVolumeIn34(self,a0):
            self.setVolume('in_line34', a0)

    def setVolumeInSPDIF(self,a0):
            self.setVolume('in_spdif', a0)

    def setVolumeOut12(self,a0):
            self.setVolume('out_line12', a0)

    def setVolumeOut34(self,a0):
            self.setVolume('out_line34', a0)

    def setCrossA(self,a0):
            self.setVolume('cross_a', a0)

    def setCrossB(self,a0):
            self.setVolume('cross_b', a0)

    def setVolume(self,a0,a1):
            name = a0
            vol = -a1
            print "setting %s volume to %d" % (name, vol)
            self.hw.setContignuous(self.VolumeControls[name][0], vol)

    def setSelector(self,a0,a1):
            name = a0
            state = a1
            print "setting %s state to %d" % (name, state)
            self.hw.setDiscrete(self.SelectorControls[name][0], state)

    def updateClockSelection(self,a0):
        #disable the combobox
        self.comboClockSelect.setEnabled(False)
        #change the clock source
        self.clockselect.select(a0)
        #refresh the clock source selection box
        self.initClockSelector()
        #make the box available again
        self.comboClockSelect.setEnabled(True)

    def initClockSelector(self):
        self.comboClockSelect.clear()
        nbsources = self.clockselect.count()
        for idx in range(nbsources):
            desc = self.clockselect.getEnumLabel(idx)
            self.comboClockSelect.insertItem(desc)
        active_idx = self.clockselect.selected();
        if active_idx >= 0:
            self.comboClockSelect.setCurrentItem(active_idx)

    def init(self):
            print "Init BridgeCo Audio 5 window"

            self.VolumeControls={
                'in_line12'  :   ['/Mixer/Feature_1', self.sldInput12],
                'in_line34'  :   ['/Mixer/Feature_2', self.sldInput34],
                'in_spdif'   :   ['/Mixer/Feature_3', self.sldInputSPDIF],
                'out_line12' :   ['/Mixer/Feature_6', self.sldOutput12],
                'out_line34' :   ['/Mixer/Feature_7', self.sldOutput34],
                'cross_a'    :   ['/Mixer/Feature_4', self.sldCrossA],
                'cross_b'    :   ['/Mixer/Feature_5', self.sldCrossB],
                }

            self.SelectorControls={
                'line34source':   ['/Mixer/Selector_1', self.comboMixSource], 
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

            self.initClockSelector()
            # connect the clock selector UI element
            QObject.connect(self.comboClockSelect, SIGNAL('activated(int)'), self.updateClockSelection)
