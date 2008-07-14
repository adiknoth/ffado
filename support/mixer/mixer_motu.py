#
# Copyright (C) 2005-2008 by Pieter Palmers
# Copyright (C) 2008 by Jonathan Woithe
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
from mixer_motuui import *

# Model defines.  These must agree with what is used in motu_avdevice.h.
MOTU_MODEL_NONE     = 0x0000
MOTU_MODEL_828mkII  = 0x0001
MOTU_MODEL_TRAVELER = 0x0002
MOTU_MODEL_ULTRALITE= 0x0003
MOTU_MODEL_8PRE     = 0x0004
MOTU_MODEL_828MkI   = 0x0005
MOTU_MODEL_896HD    = 0x0006
                            
class MotuMixer(MotuMixerUI):
    def __init__(self,parent = None,name = None,fl = 0):
        MotuMixerUI.__init__(self,parent,name,fl)

    # public slot: channel faders within a matrix mixer
    def updateChannelFader(self, a0):
        sender = self.sender()
        vol = 128-a0
        print "setting %s for mix %d channel %d to %d" % (self.ChannelFaders[sender][0], 
            self.ChannelFaders[sender][1], self.ChannelFaders[sender][2], vol)
        self.hw.setMatrixMixerValue(self.ChannelFaders[sender][0],
            self.ChannelFaders[sender][1], self.ChannelFaders[sender][2], vol)

    # public slot: a multivalue control within a matrix mixer
    def updateChannelControl(self, a0):
        sender = self.sender()
        val = a0
        print "setting %s for mix %d channel %d to %d" % (self.ChannelControls[sender][0], 
            self.ChannelControls[sender][1], self.ChannelControls[sender][2], val)
        self.hw.setMatrixMixerValue(self.ChannelControls[sender][0], 
            self.ChannelControls[sender][1], self.ChannelControls[sender][2], val)

    # public slot: a generic single multivalue control
    def updateControl(self, a0):
        sender = self.sender()
        val = a0
        print "setting %s control to %d" % (self.Controls[sender][0], val)
        self.hw.setDiscrete(self.Controls[sender][0], val)

    # public slot: a binary switch within a matrix mixer
    def updateChannelBinarySwitch(self, a0):
        sender = self.sender()
        val=a0
        print "setting %s for mix %d channel %d switch to %d" % (self.ChannelBinarySwitches[sender][0], 
            self.ChannelBinarySwitches[sender][1], self.ChannelBinarySwitches[sender][2], val)
        self.hw.setMatrixMixerValue(self.ChannelBinarySwitches[sender][0], 
            self.ChannelBinarySwitches[sender][1], self.ChannelBinarySwitches[sender][2], val)

    # public slot: generic single binary switch
    def updateBinarySwitch(self, a0):
        sender = self.sender()
        val=a0
        print "setting %s switch to %d" % (self.BinarySwitches[sender][0], val)
        self.hw.setDiscrete(self.BinarySwitches[sender][0], val)

    # public slot: a faders (not in a matrix mixer)
    def updateFader(self, a0):
        sender = self.sender()
        vol = 128-a0
        print "setting %s mix fader to %d" % (self.Faders[sender][0], vol)
        self.hw.setDiscrete(self.Faders[sender][0], vol)

    # public slot: mix destination control
    def updateMixDest(self, a0):
        sender = self.sender()
        dest=a0
        print "setting %s mix destination to %d" % (self.MixDests[sender][0], dest)
        self.hw.setDiscrete(self.MixDests[sender][0], dest)

    # public slots: mix output controls
    def set_mix1_dest(self,a0):
        self.setMixDest('mix1', a0)

    def setSelector(self,a0,a1):
        name=a0
        state = a1
        print "setting %s state to %d" % (name, state)
        self.hw.setDiscrete(self.SelectorControls[name][0], state)

    # Hide and disable a control
    def disable_hide(self,widget):
        widget.hide()
        widget.setEnabled(False)

    def init(self):
        print "Init MOTU mixer window"

        # For matrix mixer controls (channel faders, pans, solos, mutes) the
        # first index (the row) is the zero-based mix number while the
        # second index (the column) is the zero-based channel number.  The
        # order of the channel enumeration must agree with that used when
        # creating the dbus controls within motu_avdevice.cpp.
        self.ChannelFaders={
            self.mix1ana1_fader: ['/Mixer/fader', 0, 0],
            self.mix1ana2_fader: ['/Mixer/fader', 0, 1],
            self.mix1ana3_fader: ['/Mixer/fader', 0, 2],
            self.mix1ana4_fader: ['/Mixer/fader', 0, 3],
            self.mix1ana5_fader: ['/Mixer/fader', 0, 4],
            self.mix1ana6_fader: ['/Mixer/fader', 0, 5],
            self.mix1ana7_fader: ['/Mixer/fader', 0, 6],
            self.mix1ana8_fader: ['/Mixer/fader', 0, 7],
            self.mix1aes1_fader: ['/Mixer/fader', 0, 8],
            self.mix1aes2_fader: ['/Mixer/fader', 0, 9],
            self.mix1spdif1_fader: ['/Mixer/fader', 0, 10],
            self.mix1spdif2_fader: ['/Mixer/fader', 0, 11],
            self.mix1adat1_fader: ['/Mixer/fader', 0, 12],
            self.mix1adat2_fader: ['/Mixer/fader', 0, 13],
            self.mix1adat3_fader: ['/Mixer/fader', 0, 14],
            self.mix1adat4_fader: ['/Mixer/fader', 0, 15],
            self.mix1adat5_fader: ['/Mixer/fader', 0, 16],
            self.mix1adat6_fader: ['/Mixer/fader', 0, 17],
            self.mix1adat7_fader: ['/Mixer/fader', 0, 18],
            self.mix1adat8_fader: ['/Mixer/fader', 0, 19],

            self.mix2ana1_fader: ['/Mixer/fader', 1, 0],
            self.mix2ana2_fader: ['/Mixer/fader', 1, 1],
            self.mix2ana3_fader: ['/Mixer/fader', 1, 2],
            self.mix2ana4_fader: ['/Mixer/fader', 1, 3],
            self.mix2ana5_fader: ['/Mixer/fader', 1, 4],
            self.mix2ana6_fader: ['/Mixer/fader', 1, 5],
            self.mix2ana7_fader: ['/Mixer/fader', 1, 6],
            self.mix2ana8_fader: ['/Mixer/fader', 1, 7],
            self.mix2aes1_fader: ['/Mixer/fader', 1, 8],
            self.mix2aes2_fader: ['/Mixer/fader', 1, 9],
            self.mix2spdif1_fader: ['/Mixer/fader', 1, 10],
            self.mix2spdif2_fader: ['/Mixer/fader', 1, 11],
            self.mix2adat1_fader: ['/Mixer/fader', 1, 12],
            self.mix2adat2_fader: ['/Mixer/fader', 1, 13],
            self.mix2adat3_fader: ['/Mixer/fader', 1, 14],
            self.mix2adat4_fader: ['/Mixer/fader', 1, 15],
            self.mix2adat5_fader: ['/Mixer/fader', 1, 16],
            self.mix2adat6_fader: ['/Mixer/fader', 1, 17],
            self.mix2adat7_fader: ['/Mixer/fader', 1, 18],
            self.mix2adat8_fader: ['/Mixer/fader', 1, 19],

            self.mix3ana1_fader: ['/Mixer/fader', 2, 0],
            self.mix3ana2_fader: ['/Mixer/fader', 2, 1],
            self.mix3ana3_fader: ['/Mixer/fader', 2, 2],
            self.mix3ana4_fader: ['/Mixer/fader', 2, 3],
            self.mix3ana5_fader: ['/Mixer/fader', 2, 4],
            self.mix3ana6_fader: ['/Mixer/fader', 2, 5],
            self.mix3ana7_fader: ['/Mixer/fader', 2, 6],
            self.mix3ana8_fader: ['/Mixer/fader', 2, 7],
            self.mix3aes1_fader: ['/Mixer/fader', 2, 8],
            self.mix3aes2_fader: ['/Mixer/fader', 2, 9],
            self.mix3spdif1_fader: ['/Mixer/fader', 2, 10],
            self.mix3spdif2_fader: ['/Mixer/fader', 2, 11],
            self.mix3adat1_fader: ['/Mixer/fader', 2, 12],
            self.mix3adat2_fader: ['/Mixer/fader', 2, 13],
            self.mix3adat3_fader: ['/Mixer/fader', 2, 14],
            self.mix3adat4_fader: ['/Mixer/fader', 2, 15],
            self.mix3adat5_fader: ['/Mixer/fader', 2, 16],
            self.mix3adat6_fader: ['/Mixer/fader', 2, 17],
            self.mix3adat7_fader: ['/Mixer/fader', 2, 18],
            self.mix3adat8_fader: ['/Mixer/fader', 2, 19],

            self.mix4ana1_fader: ['/Mixer/fader', 3, 0],
            self.mix4ana2_fader: ['/Mixer/fader', 3, 1],
            self.mix4ana3_fader: ['/Mixer/fader', 3, 2],
            self.mix4ana4_fader: ['/Mixer/fader', 3, 3],
            self.mix4ana5_fader: ['/Mixer/fader', 3, 4],
            self.mix4ana6_fader: ['/Mixer/fader', 3, 5],
            self.mix4ana7_fader: ['/Mixer/fader', 3, 6],
            self.mix4ana8_fader: ['/Mixer/fader', 3, 7],
            self.mix4aes1_fader: ['/Mixer/fader', 3, 8],
            self.mix4aes2_fader: ['/Mixer/fader', 3, 9],
            self.mix4spdif1_fader: ['/Mixer/fader', 3, 10],
            self.mix4spdif2_fader: ['/Mixer/fader', 3, 11],
            self.mix4adat1_fader: ['/Mixer/fader', 3, 12],
            self.mix4adat2_fader: ['/Mixer/fader', 3, 13],
            self.mix4adat3_fader: ['/Mixer/fader', 3, 14],
            self.mix4adat4_fader: ['/Mixer/fader', 3, 15],
            self.mix4adat5_fader: ['/Mixer/fader', 3, 16],
            self.mix4adat6_fader: ['/Mixer/fader', 3, 17],
            self.mix4adat7_fader: ['/Mixer/fader', 3, 18],
            self.mix4adat8_fader: ['/Mixer/fader', 3, 19],
        }

        self.Faders={
            self.mix1_fader: ['/Mixer/Mix1/Mix_fader'],
            self.mix2_fader: ['/Mixer/Mix2/Mix_fader'],
            self.mix3_fader: ['/Mixer/Mix3/Mix_fader'],
            self.mix4_fader: ['/Mixer/Mix4/Mix_fader'],
        }

        self.ChannelControls={
            self.mix1ana1_pan:   ['/Mixer/pan', 0, 0],
            self.mix1ana2_pan:   ['/Mixer/pan', 0, 1],
            self.mix1ana3_pan:   ['/Mixer/pan', 0, 2],
            self.mix1ana4_pan:   ['/Mixer/pan', 0, 3],
            self.mix1ana5_pan:   ['/Mixer/pan', 0, 4],
            self.mix1ana6_pan:   ['/Mixer/pan', 0, 5],
            self.mix1ana7_pan:   ['/Mixer/pan', 0, 6],
            self.mix1ana8_pan:   ['/Mixer/pan', 0, 7],
            self.mix1aes1_pan:   ['/Mixer/pan', 0, 8],
            self.mix1aes2_pan:   ['/Mixer/pan', 0, 9],
            self.mix1spdif1_pan: ['/Mixer/pan', 0, 10],
            self.mix1spdif2_pan: ['/Mixer/pan', 0, 11],
            self.mix1adat1_pan:  ['/Mixer/pan', 0, 12],
            self.mix1adat2_pan:  ['/Mixer/pan', 0, 13],
            self.mix1adat3_pan:  ['/Mixer/pan', 0, 14],
            self.mix1adat4_pan:  ['/Mixer/pan', 0, 15],
            self.mix1adat5_pan:  ['/Mixer/pan', 0, 16],
            self.mix1adat6_pan:  ['/Mixer/pan', 0, 17],
            self.mix1adat7_pan:  ['/Mixer/pan', 0, 18],
            self.mix1adat8_pan:  ['/Mixer/pan', 0, 19],

            self.mix2ana1_pan:   ['/Mixer/pan', 1, 0],
            self.mix2ana2_pan:   ['/Mixer/pan', 1, 1],
            self.mix2ana3_pan:   ['/Mixer/pan', 1, 2],
            self.mix2ana4_pan:   ['/Mixer/pan', 1, 3],
            self.mix2ana5_pan:   ['/Mixer/pan', 1, 4],
            self.mix2ana6_pan:   ['/Mixer/pan', 1, 5],
            self.mix2ana7_pan:   ['/Mixer/pan', 1, 6],
            self.mix2ana8_pan:   ['/Mixer/pan', 1, 7],
            self.mix2aes1_pan:   ['/Mixer/pan', 1, 8],
            self.mix2aes2_pan:   ['/Mixer/pan', 1, 9],
            self.mix2spdif1_pan: ['/Mixer/pan', 1, 10],
            self.mix2spdif2_pan: ['/Mixer/pan', 1, 11],
            self.mix2adat1_pan:  ['/Mixer/pan', 1, 12],
            self.mix2adat2_pan:  ['/Mixer/pan', 1, 13],
            self.mix2adat3_pan:  ['/Mixer/pan', 1, 14],
            self.mix2adat4_pan:  ['/Mixer/pan', 1, 15],
            self.mix2adat5_pan:  ['/Mixer/pan', 1, 16],
            self.mix2adat6_pan:  ['/Mixer/pan', 1, 17],
            self.mix2adat7_pan:  ['/Mixer/pan', 1, 18],
            self.mix2adat8_pan:  ['/Mixer/pan', 1, 19],

            self.mix3ana1_pan:   ['/Mixer/pan', 2, 0],
            self.mix3ana2_pan:   ['/Mixer/pan', 2, 1],
            self.mix3ana3_pan:   ['/Mixer/pan', 2, 2],
            self.mix3ana4_pan:   ['/Mixer/pan', 2, 3],
            self.mix3ana5_pan:   ['/Mixer/pan', 2, 4],
            self.mix3ana6_pan:   ['/Mixer/pan', 2, 5],
            self.mix3ana7_pan:   ['/Mixer/pan', 2, 6],
            self.mix3ana8_pan:   ['/Mixer/pan', 2, 7],
            self.mix3aes1_pan:   ['/Mixer/pan', 2, 8],
            self.mix3aes2_pan:   ['/Mixer/pan', 2, 9],
            self.mix3spdif1_pan: ['/Mixer/pan', 2, 10],
            self.mix3spdif2_pan: ['/Mixer/pan', 2, 11],
            self.mix3adat1_pan:  ['/Mixer/pan', 2, 12],
            self.mix3adat2_pan:  ['/Mixer/pan', 2, 13],
            self.mix3adat3_pan:  ['/Mixer/pan', 2, 14],
            self.mix3adat4_pan:  ['/Mixer/pan', 2, 15],
            self.mix3adat5_pan:  ['/Mixer/pan', 2, 16],
            self.mix3adat6_pan:  ['/Mixer/pan', 2, 17],
            self.mix3adat7_pan:  ['/Mixer/pan', 2, 18],
            self.mix3adat8_pan:  ['/Mixer/pan', 2, 19],

            self.mix4ana1_pan:   ['/Mixer/pan', 3, 0],
            self.mix4ana2_pan:   ['/Mixer/pan', 3, 1],
            self.mix4ana3_pan:   ['/Mixer/pan', 3, 2],
            self.mix4ana4_pan:   ['/Mixer/pan', 3, 3],
            self.mix4ana5_pan:   ['/Mixer/pan', 3, 4],
            self.mix4ana6_pan:   ['/Mixer/pan', 3, 5],
            self.mix4ana7_pan:   ['/Mixer/pan', 3, 6],
            self.mix4ana8_pan:   ['/Mixer/pan', 3, 7],
            self.mix4aes1_pan:   ['/Mixer/pan', 3, 8],
            self.mix4aes2_pan:   ['/Mixer/pan', 3, 9],
            self.mix4spdif1_pan: ['/Mixer/pan', 3, 10],
            self.mix4spdif2_pan: ['/Mixer/pan', 3, 11],
            self.mix4adat1_pan:  ['/Mixer/pan', 3, 12],
            self.mix4adat2_pan:  ['/Mixer/pan', 3, 13],
            self.mix4adat3_pan:  ['/Mixer/pan', 3, 14],
            self.mix4adat4_pan:  ['/Mixer/pan', 3, 15],
            self.mix4adat5_pan:  ['/Mixer/pan', 3, 16],
            self.mix4adat6_pan:  ['/Mixer/pan', 3, 17],
            self.mix4adat7_pan:  ['/Mixer/pan', 3, 18],
            self.mix4adat8_pan:  ['/Mixer/pan', 3, 19],
        }

        self.Controls={
            self.ana1_trimgain:  ['/Mixer/Control/Ana1_trimgain'],
            self.ana2_trimgain:  ['/Mixer/Control/Ana2_trimgain'],
            self.ana3_trimgain:  ['/Mixer/Control/Ana3_trimgain'],
            self.ana4_trimgain:  ['/Mixer/Control/Ana4_trimgain'],
        }

        self.ChannelBinarySwitches={
            self.mix1ana1_mute:  ['/Mixer/mute', 0, 0],
            self.mix1ana2_mute:  ['/Mixer/mute', 0, 1],
            self.mix1ana3_mute:  ['/Mixer/mute', 0, 2],
            self.mix1ana4_mute:  ['/Mixer/mute', 0, 3],
            self.mix1ana5_mute:  ['/Mixer/mute', 0, 4],
            self.mix1ana6_mute:  ['/Mixer/mute', 0, 5],
            self.mix1ana7_mute:  ['/Mixer/mute', 0, 6],
            self.mix1ana8_mute:  ['/Mixer/mute', 0, 7],
            self.mix1aes1_mute:  ['/Mixer/mute', 0, 8],
            self.mix1aes2_mute:  ['/Mixer/mute', 0, 9],
            self.mix1spdif1_mute: ['/Mixer/mute', 0, 10],
            self.mix1spdif2_mute: ['/Mixer/mute', 0, 11],
            self.mix1adat1_mute: ['/Mixer/mute', 0, 12],
            self.mix1adat2_mute: ['/Mixer/mute', 0, 13],
            self.mix1adat3_mute: ['/Mixer/mute', 0, 14],
            self.mix1adat4_mute: ['/Mixer/mute', 0, 15],
            self.mix1adat5_mute: ['/Mixer/mute', 0, 16],
            self.mix1adat6_mute: ['/Mixer/mute', 0, 17],
            self.mix1adat7_mute: ['/Mixer/mute', 0, 18],
            self.mix1adat8_mute: ['/Mixer/mute', 0, 19],
            self.mix1ana1_solo:  ['/Mixer/solo', 0, 0],
            self.mix1ana2_solo:  ['/Mixer/solo', 0, 1],
            self.mix1ana3_solo:  ['/Mixer/solo', 0, 2],
            self.mix1ana4_solo:  ['/Mixer/solo', 0, 3],
            self.mix1ana5_solo:  ['/Mixer/solo', 0, 4],
            self.mix1ana6_solo:  ['/Mixer/solo', 0, 5],
            self.mix1ana7_solo:  ['/Mixer/solo', 0, 6],
            self.mix1ana8_solo:  ['/Mixer/solo', 0, 7],
            self.mix1aes1_solo:  ['/Mixer/solo', 0, 8],
            self.mix1aes2_solo:  ['/Mixer/solo', 0, 9],
            self.mix1spdif1_solo: ['/Mixer/solo', 0, 10],
            self.mix1spdif2_solo: ['/Mixer/solo', 0, 11],
            self.mix1adat1_solo: ['/Mixer/solo', 0, 12],
            self.mix1adat2_solo: ['/Mixer/solo', 0, 13],
            self.mix1adat3_solo: ['/Mixer/solo', 0, 14],
            self.mix1adat4_solo: ['/Mixer/solo', 0, 15],
            self.mix1adat5_solo: ['/Mixer/solo', 0, 16],
            self.mix1adat6_solo: ['/Mixer/solo', 0, 17],
            self.mix1adat7_solo: ['/Mixer/solo', 0, 18],
            self.mix1adat8_solo: ['/Mixer/solo', 0, 19],

            self.mix2ana1_mute:  ['/Mixer/mute', 1, 0],
            self.mix2ana2_mute:  ['/Mixer/mute', 1, 1],
            self.mix2ana3_mute:  ['/Mixer/mute', 1, 2],
            self.mix2ana4_mute:  ['/Mixer/mute', 1, 3],
            self.mix2ana5_mute:  ['/Mixer/mute', 1, 4],
            self.mix2ana6_mute:  ['/Mixer/mute', 1, 5],
            self.mix2ana7_mute:  ['/Mixer/mute', 1, 6],
            self.mix2ana8_mute:  ['/Mixer/mute', 1, 7],
            self.mix2aes1_mute:  ['/Mixer/mute', 1, 8],
            self.mix2aes2_mute:  ['/Mixer/mute', 1, 9],
            self.mix2spdif1_mute: ['/Mixer/mute', 1, 10],
            self.mix2spdif2_mute: ['/Mixer/mute', 1, 11],
            self.mix2adat1_mute: ['/Mixer/mute', 1, 12],
            self.mix2adat2_mute: ['/Mixer/mute', 1, 13],
            self.mix2adat3_mute: ['/Mixer/mute', 1, 14],
            self.mix2adat4_mute: ['/Mixer/mute', 1, 15],
            self.mix2adat5_mute: ['/Mixer/mute', 1, 16],
            self.mix2adat6_mute: ['/Mixer/mute', 1, 17],
            self.mix2adat7_mute: ['/Mixer/mute', 1, 18],
            self.mix2adat8_mute: ['/Mixer/mute', 1, 19],
            self.mix2ana1_solo:  ['/Mixer/solo', 1, 0],
            self.mix2ana2_solo:  ['/Mixer/solo', 1, 1],
            self.mix2ana3_solo:  ['/Mixer/solo', 1, 2],
            self.mix2ana4_solo:  ['/Mixer/solo', 1, 3],
            self.mix2ana5_solo:  ['/Mixer/solo', 1, 4],
            self.mix2ana6_solo:  ['/Mixer/solo', 1, 5],
            self.mix2ana7_solo:  ['/Mixer/solo', 1, 6],
            self.mix2ana8_solo:  ['/Mixer/solo', 1, 7],
            self.mix2aes1_solo:  ['/Mixer/solo', 1, 8],
            self.mix2aes2_solo:  ['/Mixer/solo', 1, 9],
            self.mix2spdif1_solo: ['/Mixer/solo', 1, 10],
            self.mix2spdif2_solo: ['/Mixer/solo', 1, 11],
            self.mix2adat1_solo: ['/Mixer/solo', 1, 12],
            self.mix2adat2_solo: ['/Mixer/solo', 1, 13],
            self.mix2adat3_solo: ['/Mixer/solo', 1, 14],
            self.mix2adat4_solo: ['/Mixer/solo', 1, 15],
            self.mix2adat5_solo: ['/Mixer/solo', 1, 16],
            self.mix2adat6_solo: ['/Mixer/solo', 1, 17],
            self.mix2adat7_solo: ['/Mixer/solo', 1, 18],
            self.mix2adat8_solo: ['/Mixer/solo', 1, 19],

            self.mix3ana1_mute:  ['/Mixer/mute', 2, 0],
            self.mix3ana2_mute:  ['/Mixer/mute', 2, 1],
            self.mix3ana3_mute:  ['/Mixer/mute', 2, 2],
            self.mix3ana4_mute:  ['/Mixer/mute', 2, 3],
            self.mix3ana5_mute:  ['/Mixer/mute', 2, 4],
            self.mix3ana6_mute:  ['/Mixer/mute', 2, 5],
            self.mix3ana7_mute:  ['/Mixer/mute', 2, 6],
            self.mix3ana8_mute:  ['/Mixer/mute', 2, 7],
            self.mix3aes1_mute:  ['/Mixer/mute', 2, 8],
            self.mix3aes2_mute:  ['/Mixer/mute', 2, 9],
            self.mix3spdif1_mute: ['/Mixer/mute', 2, 10],
            self.mix3spdif2_mute: ['/Mixer/mute', 2, 11],
            self.mix3adat1_mute: ['/Mixer/mute', 2, 12],
            self.mix3adat2_mute: ['/Mixer/mute', 2, 13],
            self.mix3adat3_mute: ['/Mixer/mute', 2, 14],
            self.mix3adat4_mute: ['/Mixer/mute', 2, 15],
            self.mix3adat5_mute: ['/Mixer/mute', 2, 16],
            self.mix3adat6_mute: ['/Mixer/mute', 2, 17],
            self.mix3adat7_mute: ['/Mixer/mute', 2, 18],
            self.mix3adat8_mute: ['/Mixer/mute', 2, 19],
            self.mix3ana1_solo:  ['/Mixer/solo', 2, 0],
            self.mix3ana2_solo:  ['/Mixer/solo', 2, 1],
            self.mix3ana3_solo:  ['/Mixer/solo', 2, 2],
            self.mix3ana4_solo:  ['/Mixer/solo', 2, 3],
            self.mix3ana5_solo:  ['/Mixer/solo', 2, 4],
            self.mix3ana6_solo:  ['/Mixer/solo', 2, 5],
            self.mix3ana7_solo:  ['/Mixer/solo', 2, 6],
            self.mix3ana8_solo:  ['/Mixer/solo', 2, 7],
            self.mix3aes1_solo:  ['/Mixer/solo', 2, 8],
            self.mix3aes2_solo:  ['/Mixer/solo', 2, 9],
            self.mix3spdif1_solo: ['/Mixer/solo', 2, 10],
            self.mix3spdif2_solo: ['/Mixer/solo', 2, 11],
            self.mix3adat1_solo: ['/Mixer/solo', 2, 12],
            self.mix3adat2_solo: ['/Mixer/solo', 2, 13],
            self.mix3adat3_solo: ['/Mixer/solo', 2, 14],
            self.mix3adat4_solo: ['/Mixer/solo', 2, 15],
            self.mix3adat5_solo: ['/Mixer/solo', 2, 16],
            self.mix3adat6_solo: ['/Mixer/solo', 2, 17],
            self.mix3adat7_solo: ['/Mixer/solo', 2, 18],
            self.mix3adat8_solo: ['/Mixer/solo', 2, 19],

            self.mix4ana1_mute:  ['/Mixer/mute', 3, 0],
            self.mix4ana2_mute:  ['/Mixer/mute', 3, 1],
            self.mix4ana3_mute:  ['/Mixer/mute', 3, 2],
            self.mix4ana4_mute:  ['/Mixer/mute', 3, 3],
            self.mix4ana5_mute:  ['/Mixer/mute', 3, 4],
            self.mix4ana6_mute:  ['/Mixer/mute', 3, 5],
            self.mix4ana7_mute:  ['/Mixer/mute', 3, 6],
            self.mix4ana8_mute:  ['/Mixer/mute', 3, 7],
            self.mix4aes1_mute:  ['/Mixer/mute', 3, 8],
            self.mix4aes2_mute:  ['/Mixer/mute', 3, 9],
            self.mix4spdif1_mute: ['/Mixer/mute', 3, 10],
            self.mix4spdif2_mute: ['/Mixer/mute', 3, 11],
            self.mix4adat1_mute: ['/Mixer/mute', 3, 12],
            self.mix4adat2_mute: ['/Mixer/mute', 3, 13],
            self.mix4adat3_mute: ['/Mixer/mute', 3, 14],
            self.mix4adat4_mute: ['/Mixer/mute', 3, 15],
            self.mix4adat5_mute: ['/Mixer/mute', 3, 16],
            self.mix4adat6_mute: ['/Mixer/mute', 3, 17],
            self.mix4adat7_mute: ['/Mixer/mute', 3, 18],
            self.mix4adat8_mute: ['/Mixer/mute', 3, 19],
            self.mix4ana1_solo:  ['/Mixer/solo', 3, 0],
            self.mix4ana2_solo:  ['/Mixer/solo', 3, 1],
            self.mix4ana3_solo:  ['/Mixer/solo', 3, 2],
            self.mix4ana4_solo:  ['/Mixer/solo', 3, 3],
            self.mix4ana5_solo:  ['/Mixer/solo', 3, 4],
            self.mix4ana6_solo:  ['/Mixer/solo', 3, 5],
            self.mix4ana7_solo:  ['/Mixer/solo', 3, 6],
            self.mix4ana8_solo:  ['/Mixer/solo', 3, 7],
            self.mix4aes1_solo:  ['/Mixer/solo', 3, 8],
            self.mix4aes2_solo:  ['/Mixer/solo', 3, 9],
            self.mix4spdif1_solo: ['/Mixer/solo', 3, 10],
            self.mix4spdif2_solo: ['/Mixer/solo', 3, 11],
            self.mix4adat1_solo: ['/Mixer/solo', 3, 12],
            self.mix4adat2_solo: ['/Mixer/solo', 3, 13],
            self.mix4adat3_solo: ['/Mixer/solo', 3, 14],
            self.mix4adat4_solo: ['/Mixer/solo', 3, 15],
            self.mix4adat5_solo: ['/Mixer/solo', 3, 16],
            self.mix4adat6_solo: ['/Mixer/solo', 3, 17],
            self.mix4adat7_solo: ['/Mixer/solo', 3, 18],
            self.mix4adat8_solo: ['/Mixer/solo', 3, 19],
        }

        self.BinarySwitches={
            self.mix1_mute:      ['/Mixer/Mix1/Mix_mute'],
            self.mix2_mute:      ['/Mixer/Mix2/Mix_mute'],
            self.mix3_mute:      ['/Mixer/Mix3/Mix_mute'],
            self.mix4_mute:      ['/Mixer/Mix4/Mix_mute'],

            self.ana1_pad:       ['/Mixer/Control/Ana1_pad'],
            self.ana2_pad:       ['/Mixer/Control/Ana2_pad'],
            self.ana3_pad:       ['/Mixer/Control/Ana3_pad'],
            self.ana4_pad:       ['/Mixer/Control/Ana4_pad'],
            self.ana5_level:     ['/Mixer/Control/Ana5_level'],
            self.ana6_level:     ['/Mixer/Control/Ana6_level'],
            self.ana7_level:     ['/Mixer/Control/Ana7_level'],
            self.ana8_level:     ['/Mixer/Control/Ana8_level'],
            self.ana5_boost:     ['/Mixer/Control/Ana5_boost'],
            self.ana6_boost:     ['/Mixer/Control/Ana6_boost'],
            self.ana7_boost:     ['/Mixer/Control/Ana7_boost'],
            self.ana8_boost:     ['/Mixer/Control/Ana8_boost'],

            # Some interfaces have level/boost on analog 1-4 in place of trimgain/pad
            self.ana1_level:     ['/Mixer/Control/Ana1_level'],
            self.ana2_level:     ['/Mixer/Control/Ana2_level'],
            self.ana3_level:     ['/Mixer/Control/Ana3_level'],
            self.ana4_level:     ['/Mixer/Control/Ana4_level'],
            self.ana1_boost:     ['/Mixer/Control/Ana1_boost'],
            self.ana2_boost:     ['/Mixer/Control/Ana2_boost'],
            self.ana3_boost:     ['/Mixer/Control/Ana3_boost'],
            self.ana4_boost:     ['/Mixer/Control/Ana4_boost'],

        }

        # Ultimately these may be rolled into the BinarySwitches controls,
        # but since they aren't implemented and therefore need to be
        # disabled it's easier to keep them separate for the moment.  The
        # dbus path for these is yet to be finalised too - for example we
        # may end up using a matrix mixer.
        self.PairSwitches={
            self.mix1ana1_2_pair:   ['Mixer/Mix1/Ana1_2_pair'],
            self.mix1ana3_4_pair:   ['Mixer/Mix1/Ana3_4_pair'],
            self.mix1ana5_6_pair:   ['Mixer/Mix1/Ana5_6_pair'],
            self.mix1ana7_8_pair:   ['Mixer/Mix1/Ana7_8_pair'],
            self.mix1aes1_2_pair:   ['Mixer/Mix1/Aes1_2_pair'],
            self.mix1adat1_2_pair:  ['Mixer/Mix1/Adat1_2_pair'],
            self.mix1adat3_4_pair:  ['Mixer/Mix1/Adat3_4_pair'],
            self.mix1adat5_6_pair:  ['Mixer/Mix1/Adat5_6_pair'],
            self.mix1adat7_8_pair:  ['Mixer/Mix1/Adat7_8_pair'],
            self.mix1spdif1_2_pair: ['Mixer/Mix1/Spdif1_2_pair'],

            self.mix2ana1_2_pair:   ['Mixer/Mix2/Ana1_2_pair'],
            self.mix2ana3_4_pair:   ['Mixer/Mix2/Ana3_4_pair'],
            self.mix2ana5_6_pair:   ['Mixer/Mix2/Ana5_6_pair'],
            self.mix2ana7_8_pair:   ['Mixer/Mix2/Ana7_8_pair'],
            self.mix2aes1_2_pair:   ['Mixer/Mix2/Aes1_2_pair'],
            self.mix2adat1_2_pair:  ['Mixer/Mix2/Adat1_2_pair'],
            self.mix2adat3_4_pair:  ['Mixer/Mix2/Adat3_4_pair'],
            self.mix2adat5_6_pair:  ['Mixer/Mix2/Adat5_6_pair'],
            self.mix2adat7_8_pair:  ['Mixer/Mix2/Adat7_8_pair'],
            self.mix2spdif1_2_pair: ['Mixer/Mix2/Spdif1_2_pair'],

            self.mix3ana1_2_pair:   ['Mixer/Mix3/Ana1_2_pair'],
            self.mix3ana3_4_pair:   ['Mixer/Mix3/Ana3_4_pair'],
            self.mix3ana5_6_pair:   ['Mixer/Mix3/Ana5_6_pair'],
            self.mix3ana7_8_pair:   ['Mixer/Mix3/Ana7_8_pair'],
            self.mix3aes1_2_pair:   ['Mixer/Mix3/Aes1_2_pair'],
            self.mix3adat1_2_pair:  ['Mixer/Mix3/Adat1_2_pair'],
            self.mix3adat3_4_pair:  ['Mixer/Mix3/Adat3_4_pair'],
            self.mix3adat5_6_pair:  ['Mixer/Mix3/Adat5_6_pair'],
            self.mix3adat7_8_pair:  ['Mixer/Mix3/Adat7_8_pair'],
            self.mix3spdif1_2_pair: ['Mixer/Mix3/Spdif1_2_pair'],

            self.mix4ana1_2_pair:   ['Mixer/Mix4/Ana1_2_pair'],
            self.mix4ana3_4_pair:   ['Mixer/Mix4/Ana3_4_pair'],
            self.mix4ana5_6_pair:   ['Mixer/Mix4/Ana5_6_pair'],
            self.mix4ana7_8_pair:   ['Mixer/Mix4/Ana7_8_pair'],
            self.mix4aes1_2_pair:   ['Mixer/Mix4/Aes1_2_pair'],
            self.mix4adat1_2_pair:  ['Mixer/Mix4/Adat1_2_pair'],
            self.mix4adat3_4_pair:  ['Mixer/Mix4/Adat3_4_pair'],
            self.mix4adat5_6_pair:  ['Mixer/Mix4/Adat5_6_pair'],
            self.mix4adat7_8_pair:  ['Mixer/Mix4/Adat7_8_pair'],
            self.mix4spdif1_2_pair: ['Mixer/Mix4/Spdif1_2_pair'],
        }

        self.MixDests={
            self.mix1_dest:      ['/Mixer/Mix1/Mix_dest'],
            self.mix2_dest:      ['/Mixer/Mix2/Mix_dest'],
            self.mix3_dest:      ['/Mixer/Mix3/Mix_dest'],
            self.mix4_dest:      ['/Mixer/Mix4/Mix_dest'],

            self.phones_src:	 ['/Mixer/Control/Phones_src'],

            self.optical_in_mode:   ['/Mixer/Control/OpticalIn_mode'],
            self.optical_out_mode:  ['/Mixer/Control/OpticalOut_mode'],
        }

        self.SelectorControls={

        }

        # Other mixer variables
        self.is_streaming = 0
        self.sample_rate = 0
        self.model = MOTU_MODEL_NONE

    def initValues(self):
        # Is the device streaming?
        self.is_streaming = self.hw.getDiscrete('/Mixer/Info/IsStreaming')
        print "device streaming flag: %d" % (self.is_streaming)

        # Retrieve other device settings as needed
        self.model = self.hw.getDiscrete('/Mixer/Info/Model')
        print "device model identifier: %d" % (self.model)
        self.sample_rate = self.hw.getDiscrete('/Mixer/Info/SampleRate')
        print "device sample rate: %d" % (self.sample_rate)
        self.has_mic_inputs = self.hw.getDiscrete('/Mixer/Info/HasMicInputs')
        print "device has mic inputs: %d" % (self.has_mic_inputs)
        self.has_aesebu_inputs = self.hw.getDiscrete('/Mixer/Info/HasAESEBUInputs')
        print "device has AES/EBU inputs: %d" % (self.has_aesebu_inputs)
        self.has_spdif_inputs = self.hw.getDiscrete('/Mixer/Info/HasSPDIFInputs')
        print "device has SPDIF inputs: %d" % (self.has_spdif_inputs)
        self.has_optical_spdif = self.hw.getDiscrete('/Mixer/Info/HasOpticalSPDIF')
        print "device has optical SPDIF: %d" % (self.has_optical_spdif)

        # Customise the UI based on device options retrieved
        if (self.has_mic_inputs):
            # Mic input controls displace AES/EBU since no current device
            # has both.
            self.mix1_aes_group.setTitle("Mic inputs")
            self.mix2_aes_group.setTitle("Mic inputs")
            self.mix3_aes_group.setTitle("Mic inputs")
            self.mix4_aes_group.setTitle("Mic inputs")
            # FIXME: when implmented, will mic channels just reuse the AES/EBU
            # dbus path?  If not we'll have to reset the respective values in
            # the control arrays (self.ChannelFaders etc).
        else:
            if (not(self.has_aesebu_inputs)):
                self.mix1_aes_group.setEnabled(False)
                self.mix2_aes_group.setEnabled(False)
                self.mix3_aes_group.setEnabled(False)
                self.mix4_aes_group.setEnabled(False)
        if (not(self.has_spdif_inputs)):
            self.mix1_spdif_group.setEnabled(False)
            self.mix2_spdif_group.setEnabled(False)
            self.mix3_spdif_group.setEnabled(False)
            self.mix4_spdif_group.setEnabled(False)

        # Devices without AES/EBU inputs/outputs have dedicated "MainOut"
        # outputs instead.
        if (not(self.has_aesebu_inputs)):
            self.mix1_dest.changeItem("MainOut", 6)
            self.mix2_dest.changeItem("MainOut", 6)
            self.mix3_dest.changeItem("MainOut", 6)
            self.mix4_dest.changeItem("MainOut", 6)

        # Some devices don't have the option of selecting an optical SPDIF
        # mode.
        if (not(self.has_optical_spdif)):
            self.optical_in_mode.removeItem(2)
            self.optical_out_mode.removeItem(2)

        # Some controls must be disabled if the device is streaming
        if (self.is_streaming):
            print "Disabling controls which require inactive streaming"
            self.optical_in_mode.setEnabled(False)
            self.optical_out_mode.setEnabled(False)

        # Some channels aren't available at higher sampling rates
        if (self.sample_rate > 96000):
            print "Disabling controls not present above 96 kHz"
            self.mix1_adat_group.setEnabled(False)
            self.mix1_aes_group.setEnabled(False)
            self.mix1_spdif_group.setEnabled(False)
            self.mix2_adat_group.setEnabled(False)
            self.mix2_aes_group.setEnabled(False)
            self.mix2_spdif_group.setEnabled(False)
            self.mix3_adat_group.setEnabled(False)
            self.mix3_aes_group.setEnabled(False)
            self.mix3_spdif_group.setEnabled(False)
            self.mix4_adat_group.setEnabled(False)
            self.mix4_aes_group.setEnabled(False)
            self.mix4_spdif_group.setEnabled(False)
        if (self.sample_rate > 48000):
            print "Disabling controls not present above 48 kHz"
            self.mix1_adat58_group.setEnabled(False)
            self.mix2_adat58_group.setEnabled(False)
            self.mix3_adat58_group.setEnabled(False)
            self.mix4_adat58_group.setEnabled(False)

        # Ensure the correct input controls are active for a given interface
        if (self.model == MOTU_MODEL_TRAVELER):
            self.disable_hide(self.ana1_level)
            self.disable_hide(self.ana1_level_label)
            self.disable_hide(self.ana2_level)
            self.disable_hide(self.ana2_level_label)
            self.disable_hide(self.ana3_level)
            self.disable_hide(self.ana3_level_label)
            self.disable_hide(self.ana4_level)
            self.disable_hide(self.ana4_level_label)
            self.disable_hide(self.ana1_boost)
            self.disable_hide(self.ana1_boost_label)
            self.disable_hide(self.ana2_boost)
            self.disable_hide(self.ana2_boost_label)
            self.disable_hide(self.ana3_boost)
            self.disable_hide(self.ana3_boost_label)
            self.disable_hide(self.ana4_boost)
            self.disable_hide(self.ana4_boost_label)
        else:
            self.disable_hide(self.ana1_trimgain)
            self.disable_hide(self.ana1_trimgain_label)
            self.disable_hide(self.ana2_trimgain)
            self.disable_hide(self.ana2_trimgain_label)
            self.disable_hide(self.ana3_trimgain)
            self.disable_hide(self.ana3_trimgain_label)
            self.disable_hide(self.ana4_trimgain)
            self.disable_hide(self.ana4_trimgain_label)
            self.disable_hide(self.ana1_pad)
            self.disable_hide(self.ana1_pad_label)
            self.disable_hide(self.ana2_pad)
            self.disable_hide(self.ana2_pad_label)
            self.disable_hide(self.ana3_pad)
            self.disable_hide(self.ana3_pad_label)
            self.disable_hide(self.ana4_pad)
            self.disable_hide(self.ana4_pad_label)

        # Now fetch the current values into the respective controls.  Don't
        # bother fetching controls which are disabled.
        for ctrl, info in self.ChannelFaders.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            vol = 128-self.hw.getMatrixMixerValue(info[0], info[1], info[2])
            print "%s for mix %d channel %d is %d" % (info[0], info[1], info[2], vol)
            ctrl.setValue(vol)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateChannelFader)

        for ctrl, info in self.Faders.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            vol = 128-self.hw.getDiscrete(info[0])
            print "%s mix fader is %d" % (info[0] , vol)
            ctrl.setValue(vol)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateFader)

        for ctrl, info in self.ChannelControls.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            pan = self.hw.getMatrixMixerValue(info[0], info[1], info[2])
            print "%s for mix %d channel %d is %d" % (info[0], info[1], info[2], pan)
            ctrl.setValue(pan)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateChannelControl)

        for ctrl, info in self.Controls.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            pan = self.hw.getDiscrete(info[0])
            print "%s control is %d" % (info[0] , pan)
            ctrl.setValue(pan)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateControl)

        # Disable the channel pair controls since they aren't yet implemented
        for ctrl, info in self.PairSwitches.iteritems():
            print "%s control is not implemented yet: disabling" % (info[0])
            ctrl.setEnabled(False)

        for ctrl, info in self.ChannelBinarySwitches.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = self.hw.getMatrixMixerValue(info[0], info[1], info[2])
            print "%s for mix %d channel %d is %d" % (info[0] , info[1], info[2], val)
            ctrl.setChecked(val)
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateChannelBinarySwitch)

        for ctrl, info in self.BinarySwitches.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = self.hw.getDiscrete(info[0])
            print "%s switch is %d" % (info[0] , val)
            ctrl.setChecked(val)
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateBinarySwitch)

        for ctrl, info in self.MixDests.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            dest = self.hw.getDiscrete(info[0])
            print "%s mix destination is %d" % (info[0] , dest)
            ctrl.setCurrentItem(dest)
            QObject.connect(ctrl, SIGNAL('activated(int)'), self.updateMixDest)

        for name, ctrl in self.SelectorControls.iteritems():
            state = self.hw.getDiscrete(ctrl[0])
            print "%s state is %d" % (name , state)
            ctrl[1].setCurrentItem(state)    

        # FIXME: If optical mode is not ADAT, disable ADAT controls here. 
        # It can't be done earlier because we need the current values of the
        # ADAT channel controls in case the user goes ahead and enables the
        # ADAT optical mode.
