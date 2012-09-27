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

from PyQt4.QtCore import SIGNAL, SLOT, QObject, Qt
from PyQt4.QtGui import QWidget, QApplication
from ffado.config import *

import logging
log = logging.getLogger('motu')

# Model defines.  These must agree with what is used in motu_avdevice.h.
MOTU_MODEL_NONE             = 0x0000
MOTU_MODEL_828mkII          = 0x0001
MOTU_MODEL_TRAVELER         = 0x0002
MOTU_MODEL_ULTRALITE        = 0x0003
MOTU_MODEL_8PRE             = 0x0004
MOTU_MODEL_828MkI           = 0x0005
MOTU_MODEL_896HD            = 0x0006
MOTU_MODEL_828mk3           = 0x0007
MOTU_MODEL_ULTRALITEmk3     = 0x0008
MOTU_MODEL_ULTRALITEmk3_HYB = 0x0009
MOTU_MODEL_TRAVELERmk3      = 0x000a
MOTU_MODEL_896HDmk3         = 0x000b

class Motu(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/motu", self)

        self.init()

    def init(self):

        # For matrix mixer controls (channel faders, pans, solos, mutes) the
        # first index (the row) is the zero-based mix number while the
        # second index (the column) is the zero-based channel number.  The
        # third index gives the control of the stereo pair of the control
        # used as the key.  The order of the channel enumeration must agree
        # with that used when creating the dbus controls within
        # motu_avdevice.cpp.
        self.ChannelFaders={
            self.mix1ana1_fader: ['/Mixer/fader', 0, 0, self.mix1ana2_fader],
            self.mix1ana2_fader: ['/Mixer/fader', 0, 1, self.mix1ana1_fader],
            self.mix1ana3_fader: ['/Mixer/fader', 0, 2, self.mix1ana4_fader],
            self.mix1ana4_fader: ['/Mixer/fader', 0, 3, self.mix1ana3_fader],
            self.mix1ana5_fader: ['/Mixer/fader', 0, 4, self.mix1ana6_fader],
            self.mix1ana6_fader: ['/Mixer/fader', 0, 5, self.mix1ana5_fader],
            self.mix1ana7_fader: ['/Mixer/fader', 0, 6, self.mix1ana8_fader],
            self.mix1ana8_fader: ['/Mixer/fader', 0, 7, self.mix1ana7_fader],
            self.mix1aes1_fader: ['/Mixer/fader', 0, 8, self.mix1aes2_fader],
            self.mix1aes2_fader: ['/Mixer/fader', 0, 9, self.mix1aes1_fader],
            self.mix1spdif1_fader: ['/Mixer/fader', 0, 10, self.mix1spdif2_fader],
            self.mix1spdif2_fader: ['/Mixer/fader', 0, 11, self.mix1spdif1_fader],
            self.mix1adat1_fader: ['/Mixer/fader', 0, 12, self.mix1adat2_fader],
            self.mix1adat2_fader: ['/Mixer/fader', 0, 13, self.mix1adat1_fader],
            self.mix1adat3_fader: ['/Mixer/fader', 0, 14, self.mix1adat4_fader],
            self.mix1adat4_fader: ['/Mixer/fader', 0, 15, self.mix1adat3_fader],
            self.mix1adat5_fader: ['/Mixer/fader', 0, 16, self.mix1adat6_fader],
            self.mix1adat6_fader: ['/Mixer/fader', 0, 17, self.mix1adat5_fader],
            self.mix1adat7_fader: ['/Mixer/fader', 0, 18, self.mix1adat8_fader],
            self.mix1adat8_fader: ['/Mixer/fader', 0, 19, self.mix1adat7_fader],

            self.mix2ana1_fader: ['/Mixer/fader', 1, 0, self.mix2ana2_fader],
            self.mix2ana2_fader: ['/Mixer/fader', 1, 1, self.mix2ana1_fader],
            self.mix2ana3_fader: ['/Mixer/fader', 1, 2, self.mix2ana4_fader],
            self.mix2ana4_fader: ['/Mixer/fader', 1, 3, self.mix2ana3_fader],
            self.mix2ana5_fader: ['/Mixer/fader', 1, 4, self.mix2ana6_fader],
            self.mix2ana6_fader: ['/Mixer/fader', 1, 5, self.mix2ana5_fader],
            self.mix2ana7_fader: ['/Mixer/fader', 1, 6, self.mix2ana8_fader],
            self.mix2ana8_fader: ['/Mixer/fader', 1, 7, self.mix2ana7_fader],
            self.mix2aes1_fader: ['/Mixer/fader', 1, 8, self.mix2aes2_fader],
            self.mix2aes2_fader: ['/Mixer/fader', 1, 9, self.mix2aes1_fader],
            self.mix2spdif1_fader: ['/Mixer/fader', 1, 10, self.mix2spdif2_fader],
            self.mix2spdif2_fader: ['/Mixer/fader', 1, 11, self.mix2spdif1_fader],
            self.mix2adat1_fader: ['/Mixer/fader', 1, 12, self.mix2adat2_fader],
            self.mix2adat2_fader: ['/Mixer/fader', 1, 13, self.mix2adat1_fader],
            self.mix2adat3_fader: ['/Mixer/fader', 1, 14, self.mix2adat4_fader],
            self.mix2adat4_fader: ['/Mixer/fader', 1, 15, self.mix2adat3_fader],
            self.mix2adat5_fader: ['/Mixer/fader', 1, 16, self.mix2adat6_fader],
            self.mix2adat6_fader: ['/Mixer/fader', 1, 17, self.mix2adat5_fader],
            self.mix2adat7_fader: ['/Mixer/fader', 1, 18, self.mix2adat8_fader],
            self.mix2adat8_fader: ['/Mixer/fader', 1, 19, self.mix2adat7_fader],

            self.mix3ana1_fader: ['/Mixer/fader', 2, 0, self.mix3ana2_fader],
            self.mix3ana2_fader: ['/Mixer/fader', 2, 1, self.mix3ana1_fader],
            self.mix3ana3_fader: ['/Mixer/fader', 2, 2, self.mix3ana4_fader],
            self.mix3ana4_fader: ['/Mixer/fader', 2, 3, self.mix3ana3_fader],
            self.mix3ana5_fader: ['/Mixer/fader', 2, 4, self.mix3ana6_fader],
            self.mix3ana6_fader: ['/Mixer/fader', 2, 5, self.mix3ana5_fader],
            self.mix3ana7_fader: ['/Mixer/fader', 2, 6, self.mix3ana8_fader],
            self.mix3ana8_fader: ['/Mixer/fader', 2, 7, self.mix3ana7_fader],
            self.mix3aes1_fader: ['/Mixer/fader', 2, 8, self.mix3aes2_fader],
            self.mix3aes2_fader: ['/Mixer/fader', 2, 9, self.mix3aes1_fader],
            self.mix3spdif1_fader: ['/Mixer/fader', 2, 10, self.mix3spdif2_fader],
            self.mix3spdif2_fader: ['/Mixer/fader', 2, 11, self.mix3spdif1_fader],
            self.mix3adat1_fader: ['/Mixer/fader', 2, 12, self.mix3adat2_fader],
            self.mix3adat2_fader: ['/Mixer/fader', 2, 13, self.mix3adat1_fader],
            self.mix3adat3_fader: ['/Mixer/fader', 2, 14, self.mix3adat4_fader],
            self.mix3adat4_fader: ['/Mixer/fader', 2, 15, self.mix3adat3_fader],
            self.mix3adat5_fader: ['/Mixer/fader', 2, 16, self.mix3adat6_fader],
            self.mix3adat6_fader: ['/Mixer/fader', 2, 17, self.mix3adat5_fader],
            self.mix3adat7_fader: ['/Mixer/fader', 2, 18, self.mix3adat8_fader],
            self.mix3adat8_fader: ['/Mixer/fader', 2, 19, self.mix3adat7_fader],

            self.mix4ana1_fader: ['/Mixer/fader', 3, 0, self.mix4ana2_fader],
            self.mix4ana2_fader: ['/Mixer/fader', 3, 1, self.mix4ana1_fader],
            self.mix4ana3_fader: ['/Mixer/fader', 3, 2, self.mix4ana4_fader],
            self.mix4ana4_fader: ['/Mixer/fader', 3, 3, self.mix4ana3_fader],
            self.mix4ana5_fader: ['/Mixer/fader', 3, 4, self.mix4ana6_fader],
            self.mix4ana6_fader: ['/Mixer/fader', 3, 5, self.mix4ana5_fader],
            self.mix4ana7_fader: ['/Mixer/fader', 3, 6, self.mix4ana8_fader],
            self.mix4ana8_fader: ['/Mixer/fader', 3, 7, self.mix4ana7_fader],
            self.mix4aes1_fader: ['/Mixer/fader', 3, 8, self.mix4aes2_fader],
            self.mix4aes2_fader: ['/Mixer/fader', 3, 9, self.mix4aes1_fader],
            self.mix4spdif1_fader: ['/Mixer/fader', 3, 10, self.mix4spdif2_fader],
            self.mix4spdif2_fader: ['/Mixer/fader', 3, 11, self.mix4spdif1_fader],
            self.mix4adat1_fader: ['/Mixer/fader', 3, 12, self.mix4adat2_fader],
            self.mix4adat2_fader: ['/Mixer/fader', 3, 13, self.mix4adat1_fader],
            self.mix4adat3_fader: ['/Mixer/fader', 3, 14, self.mix4adat4_fader],
            self.mix4adat4_fader: ['/Mixer/fader', 3, 15, self.mix4adat3_fader],
            self.mix4adat5_fader: ['/Mixer/fader', 3, 16, self.mix4adat6_fader],
            self.mix4adat6_fader: ['/Mixer/fader', 3, 17, self.mix4adat5_fader],
            self.mix4adat7_fader: ['/Mixer/fader', 3, 18, self.mix4adat8_fader],
            self.mix4adat8_fader: ['/Mixer/fader', 3, 19, self.mix4adat7_fader],
        }

        self.Faders={
            self.mix1_fader: ['/Mixer/Mix1/Mix_fader'],
            self.mix2_fader: ['/Mixer/Mix2/Mix_fader'],
            self.mix3_fader: ['/Mixer/Mix3/Mix_fader'],
            self.mix4_fader: ['/Mixer/Mix4/Mix_fader'],
            self.mainout_fader: ['/Mixer/Mainout_fader'],
            self.phones_fader:  ['/Mixer/Phones_fader'],
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
            self.ana5_trimgain:  ['/Mixer/Control/Ana5_trimgain'],
            self.ana6_trimgain:  ['/Mixer/Control/Ana6_trimgain'],
            self.ana7_trimgain:  ['/Mixer/Control/Ana7_trimgain'],
            self.ana8_trimgain:  ['/Mixer/Control/Ana8_trimgain'],
            self.spdif1_trimgain:  ['/Mixer/Control/Spdif1_trimgain'],
            self.spdif2_trimgain:  ['/Mixer/Control/Spdif2_trimgain'],
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
            self.ana5_pad:       ['/Mixer/Control/Ana5_pad'],
            self.ana6_pad:       ['/Mixer/Control/Ana6_pad'],
            self.ana7_pad:       ['/Mixer/Control/Ana7_pad'],
            self.ana8_pad:       ['/Mixer/Control/Ana8_pad'],
            self.ana1_invert:    ['/Mixer/Control/Ana1_invert'],
            self.ana2_invert:    ['/Mixer/Control/Ana2_invert'],
            self.ana3_invert:    ['/Mixer/Control/Ana3_invert'],
            self.ana4_invert:    ['/Mixer/Control/Ana4_invert'],
            self.ana5_invert:    ['/Mixer/Control/Ana5_invert'],
            self.ana6_invert:    ['/Mixer/Control/Ana6_invert'],
            self.ana7_invert:    ['/Mixer/Control/Ana7_invert'],
            self.ana8_invert:    ['/Mixer/Control/Ana8_invert'],
            self.spdif1_invert:  ['/Mixer/Control/Spdif1_invert'],
            self.spdif2_invert:  ['/Mixer/Control/Spdif2_invert'],
            self.ana1_level:     ['/Mixer/Control/Ana1_level'],
            self.ana2_level:     ['/Mixer/Control/Ana2_level'],
            self.ana3_level:     ['/Mixer/Control/Ana3_level'],
            self.ana4_level:     ['/Mixer/Control/Ana4_level'],
            self.ana5_level:     ['/Mixer/Control/Ana5_level'],
            self.ana6_level:     ['/Mixer/Control/Ana6_level'],
            self.ana7_level:     ['/Mixer/Control/Ana7_level'],
            self.ana8_level:     ['/Mixer/Control/Ana8_level'],
            self.ana1_boost:     ['/Mixer/Control/Ana1_boost'],
            self.ana2_boost:     ['/Mixer/Control/Ana2_boost'],
            self.ana3_boost:     ['/Mixer/Control/Ana3_boost'],
            self.ana4_boost:     ['/Mixer/Control/Ana4_boost'],
            self.ana5_boost:     ['/Mixer/Control/Ana5_boost'],
            self.ana6_boost:     ['/Mixer/Control/Ana6_boost'],
            self.ana7_boost:     ['/Mixer/Control/Ana7_boost'],
            self.ana8_boost:     ['/Mixer/Control/Ana8_boost'],
        }

        self.Selectors={
            self.mix1_dest:      ['/Mixer/Mix1/Mix_dest'],
            self.mix2_dest:      ['/Mixer/Mix2/Mix_dest'],
            self.mix3_dest:      ['/Mixer/Mix3/Mix_dest'],
            self.mix4_dest:      ['/Mixer/Mix4/Mix_dest'],

            self.phones_src:	 ['/Mixer/Control/Phones_src'],

            self.optical_in_mode:   ['/Mixer/Control/OpticalIn_mode'],
            self.optical_out_mode:  ['/Mixer/Control/OpticalOut_mode'],

            self.meter_src_ctrl:    ['/Mixer/Control/Meter_src'],
            self.aesebu_meter_ctrl: ['/Mixer/Control/Meter_aesebu_src'],
            self.peakhold_time_ctrl:['/Mixer/Control/Meter_peakhold_time'],
            self.cliphold_time_ctrl:['/Mixer/Control/Meter_cliphold_time'],
        }

        # Other mixer variables
        self.is_streaming = 0
        self.sample_rate = 0
        self.model = MOTU_MODEL_NONE


    # public slot: channel faders within a matrix mixer
    def updateChannelFader(self, a0):
        sender = self.sender()
        vol = a0
        log.debug("setting %s for mix %d channel %d to %d" % (self.ChannelFaders[sender][0], 
            self.ChannelFaders[sender][1], self.ChannelFaders[sender][2], vol))
        self.hw.setMatrixMixerValue(self.ChannelFaders[sender][0],
            self.ChannelFaders[sender][1], self.ChannelFaders[sender][2], vol)
        # Using the ctrl modifier key makes stereo pairs move in unison
        if (QApplication.keyboardModifiers() == Qt.ControlModifier):
            pair = self.ChannelFaders[sender][3]
            pair.setValue(vol)

    # public slot: a multivalue control within a matrix mixer
    def updateChannelControl(self, a0):
        sender = self.sender()
        val = a0
        log.debug("setting %s for mix %d channel %d to %d" % (self.ChannelControls[sender][0], 
            self.ChannelControls[sender][1], self.ChannelControls[sender][2], val))
        self.hw.setMatrixMixerValue(self.ChannelControls[sender][0], 
            self.ChannelControls[sender][1], self.ChannelControls[sender][2], val)

    # public slot: a generic single multivalue control
    def updateControl(self, a0):
        sender = self.sender()
        val = a0
        log.debug("setting %s control to %d" % (self.Controls[sender][0], val))
        self.hw.setDiscrete(self.Controls[sender][0], val)

    # public slot: a binary switch within a matrix mixer
    def updateChannelBinarySwitch(self, a0):
        sender = self.sender()
        val=a0
        log.debug("setting %s for mix %d channel %d switch to %d" % (self.ChannelBinarySwitches[sender][0], 
            self.ChannelBinarySwitches[sender][1], self.ChannelBinarySwitches[sender][2], val))
        self.hw.setMatrixMixerValue(self.ChannelBinarySwitches[sender][0], 
            self.ChannelBinarySwitches[sender][1], self.ChannelBinarySwitches[sender][2], val)

    # public slot: generic single binary switch
    def updateBinarySwitch(self, a0):
        sender = self.sender()
        val=a0
        log.debug("setting %s switch to %d" % (self.BinarySwitches[sender][0], val))
        self.hw.setDiscrete(self.BinarySwitches[sender][0], val)

    # public slot: a faders (not in a matrix mixer)
    def updateFader(self, a0):
        sender = self.sender()
        vol = a0
        log.debug("setting %s mix fader to %d" % (self.Faders[sender][0], vol))
        self.hw.setDiscrete(self.Faders[sender][0], vol)

    # public slot: selectors (eg: mix destination controls)
    def updateSelector(self, a0):
        sender = self.sender()
        dest=a0
        log.debug("setting %s selector to %d" % (self.Selectors[sender][0], dest))
        self.hw.setDiscrete(self.Selectors[sender][0], dest)

    # public slots: mix output controls
    def set_mix1_dest(self,a0):
        self.setMixDest('mix1', a0)

    def setSelector(self,a0,a1):
        name=a0
        state = a1
        log.debug("setting %s state to %d" % (name, state))
        self.hw.setDiscrete(self.SelectorControls[name][0], state)

    # Hide and disable a control
    def disable_hide(self,widget):
        widget.hide()
        widget.setEnabled(False)

    def initValues_g1(self):
        # Set up widgets for generation-1 devices (only the 828mk1 for now). 
        # For now disable all mix faders and analog controls since we don't
        # know how to control them (and it's not clear that they are even
        # present in the hardware).
        self.disable_hide(self.mixtab)
        self.disable_hide(self.masterbox)
        self.disable_hide(self.analog_settings_box)

    def initValues_g2(self):
        # Set up widgets for generation-2 devices

        # The 828Mk2 has separate Mic inputs but no AES/EBU, so use the
        # AES/EBU mixer controls as "Mic" controls.  If a device comes along
        # with both mic and AES inputs this approach will have to be
        # re-thought.
        # Doing this means that on the 828Mk2, the mixer matrix elements
        # used for AES/EBU on other models are used for the Mic channels. 
        # So long as the MixerChannels_828Mk2 definition in
        # motu_avdevice.cpp defines the mic channels immediately after the 8
        # analog channels we'll be right.  Note that we don't need to change
        # the matrix lookup tables (self.ChannelFaders etc) because the QT
        # controls are still named *aesebu*.
        if (self.model == MOTU_MODEL_828mkII):
            self.mix1_tab.setTabText(1, "Mic inputs");
            self.mix2_tab.setTabText(1, "Mic inputs");
            self.mix3_tab.setTabText(1, "Mic inputs");
            self.mix4_tab.setTabText(1, "Mic inputs");
        else:
            # Only the Traveler and 896HD have AES/EBU inputs, so disable the AES/EBU
            # tab for all other models.
            if (self.model!=MOTU_MODEL_TRAVELER and self.model!=MOTU_MODEL_896HD):
                self.mix1_tab.setTabEnabled(1, False)
                self.mix2_tab.setTabEnabled(1, False)
                self.mix3_tab.setTabEnabled(1, False)
                self.mix4_tab.setTabEnabled(1, False)

        # All models except the 896HD and 8pre have SPDIF inputs.
        if (self.model==MOTU_MODEL_8PRE or self.model==MOTU_MODEL_896HD):
            self.mix1_tab.setTabEnabled(2, False);
            self.mix2_tab.setTabEnabled(2, False);
            self.mix3_tab.setTabEnabled(2, False);
            self.mix4_tab.setTabEnabled(2, False);

        # Devices without AES/EBU inputs/outputs (currently all except the
        # Traveler and 896HD) have dedicated "MainOut" outputs instead. 
        # AES/EBU is normally ID 6 in the destination lists and "MainOut"
        # displaces it on non-AES/EBU models.  The 896HD has both AES/EBU
        # and MainOut which complicates this; it uses ID 6 for MainOut and
        # ID 7 (nominally SPDIF) for AES/EBU.  Therefore change ID 6 to
        # "MainOut" for everything but the Traveler, and set ID 7 (nominally
        # SPDIF) to AES/EBU for the 896HD.
        if (self.model != MOTU_MODEL_TRAVELER):
            self.mix1_dest.setItemText(6, "MainOut")
            self.mix2_dest.setItemText(6, "MainOut")
            self.mix3_dest.setItemText(6, "MainOut")
            self.mix4_dest.setItemText(6, "MainOut")
            self.phones_src.setItemText(6, "MainOut")
        if (self.model == MOTU_MODEL_896HD):
            self.mix1_dest.setItemText(7, "AES/EBU")
            self.mix2_dest.setItemText(7, "AES/EBU")
            self.mix3_dest.setItemText(7, "AES/EBU")
            self.mix4_dest.setItemText(7, "AES/EBU")
            self.phones_src.setItemText(7, "AES/EBU")

        # The Ultralite doesn't have ADAT channels (or any optical ports at
        # all)
        if (self.model == MOTU_MODEL_ULTRALITE):
            self.mix1_tab.setTabEnabled(3, False)  # ADAT page
            self.mix2_tab.setTabEnabled(3, False)  # ADAT page
            self.mix3_tab.setTabEnabled(3, False)  # ADAT page
            self.mix4_tab.setTabEnabled(3, False)  # ADAT page
            self.optical_in_mode.setEnabled(False)
            self.optical_out_mode.setEnabled(False)

        # The 896HD and 8pre don't have optical SPDIF (aka Toslink) capability
        if (self.model==MOTU_MODEL_896HD or self.model==MOTU_MODEL_8PRE):
            self.optical_in_mode.removeItem(2)
            self.optical_out_mode.removeItem(2)

        # The 8pre doesn't have software phones/main fader controls
        if (self.model==MOTU_MODEL_8PRE):
            self.disable_hide(self.mainout_fader)
            self.disable_hide(self.phones_fader)

        # Only the 896HD has meter controls
        if (self.model != MOTU_MODEL_896HD):
            self.disable_hide(self.meter_src)
            self.disable_hide(self.aesebu_meter)
            self.disable_hide(self.peakhold_time)
            self.disable_hide(self.cliphold_time)

        # Some controls must be disabled if the device is streaming
        if (self.is_streaming):
            log.debug("Disabling controls which require inactive streaming")
            self.optical_in_mode.setEnabled(False)
            self.optical_out_mode.setEnabled(False)

        # Some channels aren't available at higher sampling rates
        if (self.sample_rate > 96000):
            log.debug("Disabling controls not present above 96 kHz")
            self.mix1_tab.setTabEnabled(3, False)  # ADAT
            self.mix1_tab.setTabEnabled(2, False)  # SPDIF
            self.mix1_tab.setTabEnabled(1, False)  # AES/EBU
            self.mix2_tab.setTabEnabled(3, False)  # ADAT
            self.mix2_tab.setTabEnabled(2, False)  # SPDIF
            self.mix2_tab.setTabEnabled(1, False)  # AES/EBU
            self.mix3_tab.setTabEnabled(3, False)  # ADAT
            self.mix3_tab.setTabEnabled(2, False)  # SPDIF
            self.mix3_tab.setTabEnabled(1, False)  # AES/EBU
            self.mix4_tab.setTabEnabled(3, False)  # ADAT
            self.mix4_tab.setTabEnabled(2, False)  # SPDIF
            self.mix4_tab.setTabEnabled(1, False)  # AES/EBU
        if (self.sample_rate > 48000):
            log.debug("Disabling controls not present above 48 kHz")
            self.mix1adat5.setEnabled(False)
            self.mix1adat6.setEnabled(False)
            self.mix1adat7.setEnabled(False)
            self.mix1adat8.setEnabled(False)
            self.mix2adat5.setEnabled(False)
            self.mix2adat6.setEnabled(False)
            self.mix2adat7.setEnabled(False)
            self.mix2adat8.setEnabled(False)
            self.mix3adat5.setEnabled(False)
            self.mix3adat6.setEnabled(False)
            self.mix3adat7.setEnabled(False)
            self.mix3adat8.setEnabled(False)
            self.mix4adat5.setEnabled(False)
            self.mix4adat6.setEnabled(False)
            self.mix4adat7.setEnabled(False)
            self.mix4adat8.setEnabled(False)

        # Ensure the correct input controls are active for a given interface.
        # Only the Ultralite has phase inversion switches.
        if (not(self.model == MOTU_MODEL_ULTRALITE)):
            self.disable_hide(self.ana1_invert)
            self.disable_hide(self.ana2_invert)
            self.disable_hide(self.ana3_invert)
            self.disable_hide(self.ana4_invert)
            self.disable_hide(self.ana5_invert)
            self.disable_hide(self.ana6_invert)
            self.disable_hide(self.ana7_invert)
            self.disable_hide(self.ana8_invert)
            self.disable_hide(self.spdif1_invert)
            self.disable_hide(self.spdif2_invert)
        # The Traveler has pad switches for analog 1-4 only; other interfaces
        # don't have pad switches at all.
        if (not(self.model == MOTU_MODEL_TRAVELER)):
            self.disable_hide(self.ana1_pad)
            self.disable_hide(self.ana2_pad)
            self.disable_hide(self.ana3_pad)
            self.disable_hide(self.ana4_pad)
        self.disable_hide(self.ana5_pad)
        self.disable_hide(self.ana6_pad)
        self.disable_hide(self.ana7_pad)
        self.disable_hide(self.ana8_pad)
        # The Traveler has level and boost switches for analog 5-8.  The
        # 8pre, Ultralite and the 896HD don't implement them.  All other
        # interfaces have them over analog 1-8.
        if (self.model==MOTU_MODEL_TRAVELER or self.model==MOTU_MODEL_ULTRALITE or self.model==MOTU_MODEL_896HD or self.model==MOTU_MODEL_8PRE):
            self.disable_hide(self.ana1_level)
            self.disable_hide(self.ana2_level)
            self.disable_hide(self.ana3_level)
            self.disable_hide(self.ana4_level)
            self.disable_hide(self.ana1_boost)
            self.disable_hide(self.ana2_boost)
            self.disable_hide(self.ana3_boost)
            self.disable_hide(self.ana4_boost)
        if (self.model==MOTU_MODEL_ULTRALITE or self.model==MOTU_MODEL_896HD or self.model==MOTU_MODEL_8PRE):
            self.disable_hide(self.ana5_level)
            self.disable_hide(self.ana6_level)
            self.disable_hide(self.ana7_level)
            self.disable_hide(self.ana8_level)
            self.disable_hide(self.ana5_boost)
            self.disable_hide(self.ana6_boost)
            self.disable_hide(self.ana7_boost)
            self.disable_hide(self.ana8_boost)
        # The Traveler has trimgain for analog 1-4.  The Ultralite has trimgain for
        # analog 1-8 and SPDIF 1-2.  All other interfaces don't have trimgain.
        if (not(self.model==MOTU_MODEL_TRAVELER or self.model==MOTU_MODEL_ULTRALITE)):
            self.disable_hide(self.ana1_trimgain)
            self.disable_hide(self.ana1_trimgain_label)
            self.disable_hide(self.ana2_trimgain)
            self.disable_hide(self.ana2_trimgain_label)
            self.disable_hide(self.ana3_trimgain)
            self.disable_hide(self.ana3_trimgain_label)
            self.disable_hide(self.ana4_trimgain)
            self.disable_hide(self.ana4_trimgain_label)
        if (not(self.model == MOTU_MODEL_ULTRALITE)):
            self.disable_hide(self.ana5_trimgain)
            self.disable_hide(self.ana5_trimgain_label)
            self.disable_hide(self.ana6_trimgain)
            self.disable_hide(self.ana6_trimgain_label)
            self.disable_hide(self.ana7_trimgain)
            self.disable_hide(self.ana7_trimgain_label)
            self.disable_hide(self.ana8_trimgain)
            self.disable_hide(self.ana8_trimgain_label)
            self.disable_hide(self.spdif1_trimgain);
            self.disable_hide(self.spdif1_trimgain_label);
            self.disable_hide(self.spdif1ctrl);
            self.disable_hide(self.spdif2_trimgain);
            self.disable_hide(self.spdif2_trimgain_label);
            self.disable_hide(self.spdif2ctrl);

    def initValues(self):
        # Is the device streaming?
        self.is_streaming = self.hw.getDiscrete('/Mixer/Info/IsStreaming')
        log.debug("device streaming flag: %d" % (self.is_streaming))

        # Retrieve other device settings as needed and customise the UI
        # based on these options.
        self.model = self.hw.getDiscrete('/Mixer/Info/Model')
        log.debug("device model identifier: %d" % (self.model))
        self.sample_rate = self.hw.getDiscrete('/Mixer/Info/SampleRate')
        log.debug("device sample rate: %d" % (self.sample_rate))

        # For the moment none of the "Mk3" (aka Generation-3) devices are
        # supported by ffado-mixer.
        if (self.model==MOTU_MODEL_828mk3 or self.model==MOTU_MODEL_ULTRALITEmk3 or self.model==MOTU_MODEL_ULTRALITEmk3_HYB or self.model==MOTU_MODEL_TRAVELERmk3 or self.model==MOTU_MODEL_896HDmk3):
            log.debug("Generation-3 MOTU devices are not yet supported by ffado-mixer")
            return
        elif (self.model==MOTU_MODEL_828MkI):
            self.initValues_g1()
        else:
            self.initValues_g2()

        # Now fetch the current values into the respective controls.  Don't
        # bother fetching controls which are disabled.
        for ctrl, info in self.ChannelFaders.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            vol = self.hw.getMatrixMixerValue(info[0], info[1], info[2])
            log.debug("%s for mix %d channel %d is %d" % (info[0], info[1], info[2], vol))
            ctrl.setValue(vol)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateChannelFader)

        for ctrl, info in self.Faders.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            vol = self.hw.getDiscrete(info[0])
            log.debug("%s mix fader is %d" % (info[0] , vol))
            ctrl.setValue(vol)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateFader)

        for ctrl, info in self.ChannelControls.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            pan = self.hw.getMatrixMixerValue(info[0], info[1], info[2])
            log.debug("%s for mix %d channel %d is %d" % (info[0], info[1], info[2], pan))
            ctrl.setValue(pan)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateChannelControl)

        for ctrl, info in self.Controls.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            pan = self.hw.getDiscrete(info[0])
            log.debug("%s control is %d" % (info[0] , pan))
            ctrl.setValue(pan)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateControl)

        for ctrl, info in self.ChannelBinarySwitches.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = self.hw.getMatrixMixerValue(info[0], info[1], info[2])
            log.debug("%s for mix %d channel %d is %d" % (info[0] , info[1], info[2], val))
            if val:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateChannelBinarySwitch)

        for ctrl, info in self.BinarySwitches.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = self.hw.getDiscrete(info[0])
            log.debug("%s switch is %d" % (info[0] , val))
            if val:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateBinarySwitch)

        for ctrl, info in self.Selectors.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            dest = self.hw.getDiscrete(info[0])
            log.debug("%s selector is %d" % (info[0] , dest))
            ctrl.setCurrentIndex(dest)
            QObject.connect(ctrl, SIGNAL('activated(int)'), self.updateSelector)

        # We could enable/disable ADAT controls here depending on whether
        # the optical port is set to ADAT or something else.  A disable
        # can't be done earlier since we have to read the ADAT mixer
        # settings (which won't happen if they're disabled).  However, on
        # the other hand it may be more convenient to leave all controls
        # active at all times.  We'll see.

# vim: et
