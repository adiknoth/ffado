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
from mixer_saffire_base import SaffireMixerBase
from mixer_saffireui import SaffireMixerUI

class SaffireMixer(SaffireMixerUI, SaffireMixerBase):
    def __init__(self,parent = None,name = None,fl = 0):
        SaffireMixerUI.__init__(self,parent,name,fl)
        SaffireMixerBase.__init__(self)

    def init(self):
        print "Init large Saffire mixer window"

        self.VolumeControls={
                self.sldPC12Out12: ['/Mixer/PCMix', 0, 0],
                self.sldPC12Out34: ['/Mixer/PCMix', 0, 1],
                self.sldPC12Out56: ['/Mixer/PCMix', 0, 2],
                self.sldPC12Out78: ['/Mixer/PCMix', 0, 3],
                self.sldPC12Out910: ['/Mixer/PCMix', 0, 4],
                self.sldPC34Out12: ['/Mixer/PCMix', 1, 0],
                self.sldPC34Out34: ['/Mixer/PCMix', 1, 1],
                self.sldPC34Out56: ['/Mixer/PCMix', 1, 2],
                self.sldPC34Out78: ['/Mixer/PCMix', 1, 3],
                self.sldPC34Out910: ['/Mixer/PCMix', 1, 4],
                self.sldPC56Out12: ['/Mixer/PCMix', 2, 0],
                self.sldPC56Out34: ['/Mixer/PCMix', 2, 1],
                self.sldPC56Out56: ['/Mixer/PCMix', 2, 2],
                self.sldPC56Out78: ['/Mixer/PCMix', 2, 3],
                self.sldPC56Out910: ['/Mixer/PCMix', 2, 4],
                self.sldPC78Out12: ['/Mixer/PCMix', 3, 0],
                self.sldPC78Out34: ['/Mixer/PCMix', 3, 1],
                self.sldPC78Out56: ['/Mixer/PCMix', 3, 2],
                self.sldPC78Out78: ['/Mixer/PCMix', 3, 3],
                self.sldPC78Out910: ['/Mixer/PCMix', 3, 4],
                self.sldPC910Out12: ['/Mixer/PCMix', 4, 0],
                self.sldPC910Out34: ['/Mixer/PCMix', 4, 1],
                self.sldPC910Out56: ['/Mixer/PCMix', 4, 2],
                self.sldPC910Out78: ['/Mixer/PCMix', 4, 3],
                self.sldPC910Out910: ['/Mixer/PCMix', 4, 4],
                self.sldIN1Out1: ['/Mixer/InputMix', 0, 0],
                self.sldIN1Out3: ['/Mixer/InputMix', 0, 2],
                self.sldIN1Out5: ['/Mixer/InputMix', 0, 4],
                self.sldIN1Out7: ['/Mixer/InputMix', 0, 6],
                self.sldIN1Out9: ['/Mixer/InputMix', 0, 8],
                self.sldIN2Out2: ['/Mixer/InputMix', 1, 1],
                self.sldIN2Out4: ['/Mixer/InputMix', 1, 3],
                self.sldIN2Out6: ['/Mixer/InputMix', 1, 5],
                self.sldIN2Out8: ['/Mixer/InputMix', 1, 7],
                self.sldIN2Out10: ['/Mixer/InputMix', 1, 9],
                self.sldIN3Out1: ['/Mixer/InputMix', 2, 0],
                self.sldIN3Out3: ['/Mixer/InputMix', 2, 2],
                self.sldIN3Out5: ['/Mixer/InputMix', 2, 4],
                self.sldIN3Out7: ['/Mixer/InputMix', 2, 6],
                self.sldIN3Out9: ['/Mixer/InputMix', 2, 8],
                self.sldIN4Out2: ['/Mixer/InputMix', 3, 1],
                self.sldIN4Out4: ['/Mixer/InputMix', 3, 3],
                self.sldIN4Out6: ['/Mixer/InputMix', 3, 5],
                self.sldIN4Out8: ['/Mixer/InputMix', 3, 7],
                self.sldIN4Out10: ['/Mixer/InputMix', 3, 9],
                self.sldREV1Out1: ['/Mixer/InputMix', 4, 0],
                self.sldREV1Out3: ['/Mixer/InputMix', 4, 2],
                self.sldREV1Out5: ['/Mixer/InputMix', 4, 4],
                self.sldREV1Out7: ['/Mixer/InputMix', 4, 6],
                self.sldREV1Out9: ['/Mixer/InputMix', 4, 8],
                self.sldREV2Out2: ['/Mixer/InputMix', 5, 1],
                self.sldREV2Out4: ['/Mixer/InputMix', 5, 3],
                self.sldREV2Out6: ['/Mixer/InputMix', 5, 5],
                self.sldREV2Out8: ['/Mixer/InputMix', 5, 7],
                self.sldREV2Out10: ['/Mixer/InputMix', 5, 9],
                }


        self.SelectorControls={
                self.chkSpdifSwitch:    ['/Mixer/SpdifSwitch'],
                self.chkOut12Mute:      ['/Mixer/Out12Mute'],
                self.chkOut12HwCtrl:    ['/Mixer/Out12HwCtrl'],
                self.chkOut12Dim:       ['/Mixer/Out12Dim'],
                self.chkOut34Mute:      ['/Mixer/Out34Mute'],
                self.chkOut34HwCtrl:    ['/Mixer/Out34HwCtrl'],
                self.chkOut56Mute:      ['/Mixer/Out56Mute'],
                self.chkOut56HwCtrl:    ['/Mixer/Out56HwCtrl'],
                self.chkOut78Mute:      ['/Mixer/Out78Mute'],
                self.chkOut78HwCtrl:    ['/Mixer/Out78HwCtrl'],
                self.chkOut910Mute:     ['/Mixer/Out910Mute'],
                }

        self.VolumeControlsLowRes={
                self.sldOut12Level:      ['/Mixer/Out12Level'],
                self.sldOut34Level:      ['/Mixer/Out34Level'],
                self.sldOut56Level:      ['/Mixer/Out56Level'],
                self.sldOut78Level:      ['/Mixer/Out78Level'],
                }

        self.TriggerButtonControls={
        }

        self.TextControls={
        }

        self.saveTextControls={
        }

        self.ComboControls={
        }

