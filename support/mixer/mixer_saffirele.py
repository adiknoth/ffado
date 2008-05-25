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
from mixer_saffirelelargeui import SaffireLEMixerLargeUI
from mixer_saffirelesmallui import SaffireLEMixerSmallUI

class SaffireLEMixerLarge(SaffireLEMixerLargeUI, SaffireMixerBase):
    def __init__(self,parent = None,name = None,fl = 0):
        SaffireLEMixerLargeUI.__init__(self,parent,name,fl)
        SaffireMixerBase.__init__(self)

    def init(self):
        print "Init large Saffire LE mixer window"

        self.VolumeControls={
                self.sldIN1Out1: ['/Mixer/LEMix48', 0, 0],
                self.sldIN1Out2: ['/Mixer/LEMix48', 0, 1],
                self.sldIN1Out3: ['/Mixer/LEMix48', 0, 2],
                self.sldIN1Out4: ['/Mixer/LEMix48', 0, 3],
                self.sldIN2Out1: ['/Mixer/LEMix48', 1, 0],
                self.sldIN2Out2: ['/Mixer/LEMix48', 1, 1],
                self.sldIN2Out3: ['/Mixer/LEMix48', 1, 2],
                self.sldIN2Out4: ['/Mixer/LEMix48', 1, 3],
                self.sldIN3Out1: ['/Mixer/LEMix48', 2, 0],
                self.sldIN3Out2: ['/Mixer/LEMix48', 2, 1],
                self.sldIN3Out3: ['/Mixer/LEMix48', 2, 2],
                self.sldIN3Out4: ['/Mixer/LEMix48', 2, 3],
                self.sldIN4Out1: ['/Mixer/LEMix48', 3, 0],
                self.sldIN4Out2: ['/Mixer/LEMix48', 3, 1],
                self.sldIN4Out3: ['/Mixer/LEMix48', 3, 2],
                self.sldIN4Out4: ['/Mixer/LEMix48', 3, 3],
                self.sldSPDIF1Out1: ['/Mixer/LEMix48', 4, 0],
                self.sldSPDIF1Out2: ['/Mixer/LEMix48', 4, 1],
                self.sldSPDIF1Out3: ['/Mixer/LEMix48', 4, 2],
                self.sldSPDIF1Out4: ['/Mixer/LEMix48', 4, 3],
                self.sldSPDIF2Out1: ['/Mixer/LEMix48', 5, 0],
                self.sldSPDIF2Out2: ['/Mixer/LEMix48', 5, 1],
                self.sldSPDIF2Out3: ['/Mixer/LEMix48', 5, 2],
                self.sldSPDIF2Out4: ['/Mixer/LEMix48', 5, 3],
                
                self.sldPC1Out1: ['/Mixer/LEMix48', 6, 0],
                self.sldPC1Out2: ['/Mixer/LEMix48', 6, 1],
                self.sldPC1Out3: ['/Mixer/LEMix48', 6, 2],
                self.sldPC1Out4: ['/Mixer/LEMix48', 6, 3],
                self.sldPC2Out1: ['/Mixer/LEMix48', 7, 0],
                self.sldPC2Out2: ['/Mixer/LEMix48', 7, 1],
                self.sldPC2Out3: ['/Mixer/LEMix48', 7, 2],
                self.sldPC2Out4: ['/Mixer/LEMix48', 7, 3],
                self.sldPC3Out1: ['/Mixer/LEMix48', 8, 0],
                self.sldPC3Out2: ['/Mixer/LEMix48', 8, 1],
                self.sldPC3Out3: ['/Mixer/LEMix48', 8, 2],
                self.sldPC3Out4: ['/Mixer/LEMix48', 8, 3],
                self.sldPC4Out1: ['/Mixer/LEMix48', 9, 0],
                self.sldPC4Out2: ['/Mixer/LEMix48', 9, 1],
                self.sldPC4Out3: ['/Mixer/LEMix48', 9, 2],
                self.sldPC4Out4: ['/Mixer/LEMix48', 9, 3],
                self.sldPC5Out1: ['/Mixer/LEMix48', 10, 0],
                self.sldPC5Out2: ['/Mixer/LEMix48', 10, 1],
                self.sldPC5Out3: ['/Mixer/LEMix48', 10, 2],
                self.sldPC5Out4: ['/Mixer/LEMix48', 10, 3],
                self.sldPC6Out1: ['/Mixer/LEMix48', 11, 0],
                self.sldPC6Out2: ['/Mixer/LEMix48', 11, 1],
                self.sldPC6Out3: ['/Mixer/LEMix48', 11, 2],
                self.sldPC6Out4: ['/Mixer/LEMix48', 11, 3],
                self.sldPC7Out1: ['/Mixer/LEMix48', 12, 0],
                self.sldPC7Out2: ['/Mixer/LEMix48', 12, 1],
                self.sldPC7Out3: ['/Mixer/LEMix48', 12, 2],
                self.sldPC7Out4: ['/Mixer/LEMix48', 12, 3],
                self.sldPC8Out1: ['/Mixer/LEMix48', 13, 0],
                self.sldPC8Out2: ['/Mixer/LEMix48', 13, 1],
                self.sldPC8Out3: ['/Mixer/LEMix48', 13, 2],
                self.sldPC8Out4: ['/Mixer/LEMix48', 13, 3],
                }

        self.SelectorControls={
                self.chkOut12Mute:          ['/Mixer/Out12Mute'],
                self.chkOut12HwCtrl:        ['/Mixer/Out12HwCtrl'],
                self.chkOut34Mute:          ['/Mixer/Out34Mute'],
                self.chkOut34HwCtrl:        ['/Mixer/Out34HwCtrl'],
                self.chkOut56Mute:          ['/Mixer/Out56Mute'],
                self.chkOut56HwCtrl:        ['/Mixer/Out56HwCtrl'],
                self.chkSPDIFTransparent:   ['/Mixer/SpdifTransparent'],
                self.chkMIDITru:            ['/Mixer/MidiThru'],
                self.chkHighGain3:          ['/Mixer/HighGainLine3'],
                self.chkHighGain4:          ['/Mixer/HighGainLine4'],
                }

        self.VolumeControlsLowRes={
                self.sldOut12Level:      ['/Mixer/Out12Level'],
                self.sldOut34Level:      ['/Mixer/Out34Level'],
                self.sldOut56Level:      ['/Mixer/Out56Level'],
                }

        self.TriggerButtonControls={
            self.btnSaveSettings:        ['/Mixer/SaveSettings'],
        }

        self.TextControls={
        }

        self.saveTextControls={
        }

        self.ComboControls={
        }

    def updateMatrixVolume(self,a0):
        SaffireMixerBase.updateMatrixVolume(self,a0)
    def updateLowResVolume(self,a0):
        SaffireMixerBase.updateLowResVolume(self,a0)
    def updateSelector(self,a0):
        SaffireMixerBase.updateSelector(self,a0)
    def triggerButton(self):
        SaffireMixerBase.triggerButton(self)
    def saveText(self):
        SaffireMixerBase.saveText(self)
    def initCombo(self, combo):
        SaffireMixerBase.initCombo(self,combo)
    def selectCombo(self, mode):
        SaffireMixerBase.selectCombo(self,mode)

    def updateValues(self):
        SaffireMixerBase.updateValues(self)

class SaffireLEMixerSmall(SaffireLEMixerSmallUI, SaffireMixerBase):
    def __init__(self,parent = None,name = None,fl = 0):
        SaffireLEMixerSmallUI.__init__(self,parent,name,fl)
        SaffireMixerBase.__init__(self)

    def init(self):
        print "Init small Saffire LE mixer window"

        self.VolumeControls={
                self.sldIN1RecMix:    ['/Mixer/LEMix96', 0, 4],
                self.sldIN2RecMix:    ['/Mixer/LEMix96', 1, 4],
                self.sldIN3RecMix:    ['/Mixer/LEMix96', 2, 4],
                self.sldIN4RecMix:    ['/Mixer/LEMix96', 3, 4],
                self.sldSPDIF1RecMix: ['/Mixer/LEMix96', 4, 4],
                self.sldSPDIF2RecMix: ['/Mixer/LEMix96', 5, 4],

                self.sldPC1Out1: ['/Mixer/LEMix96', 6, 0],
                self.sldPC1Out2: ['/Mixer/LEMix96', 6, 1],
                self.sldPC1Out3: ['/Mixer/LEMix96', 6, 2],
                self.sldPC1Out4: ['/Mixer/LEMix96', 6, 3],
                self.sldPC2Out1: ['/Mixer/LEMix96', 7, 0],
                self.sldPC2Out2: ['/Mixer/LEMix96', 7, 1],
                self.sldPC2Out3: ['/Mixer/LEMix96', 7, 2],
                self.sldPC2Out4: ['/Mixer/LEMix96', 7, 3],
                }

        self.SelectorControls={
                self.chkOut12Mute:          ['/Mixer/Out12Mute'],
                self.chkOut12HwCtrl:        ['/Mixer/Out12HwCtrl'],
                self.chkOut34Mute:          ['/Mixer/Out34Mute'],
                self.chkOut34HwCtrl:        ['/Mixer/Out34HwCtrl'],
                self.chkOut56Mute:          ['/Mixer/Out56Mute'],
                self.chkOut56HwCtrl:        ['/Mixer/Out56HwCtrl'],
                self.chkSPDIFTransparent:   ['/Mixer/SpdifTransparent'],
                self.chkMIDITru:            ['/Mixer/MidiThru'],
                self.chkHighGain3:          ['/Mixer/HighGainLine3'],
                self.chkHighGain4:          ['/Mixer/HighGainLine4'],
                }

        self.VolumeControlsLowRes={
                self.sldOut12Level:      ['/Mixer/Out12Level'],
                self.sldOut34Level:      ['/Mixer/Out34Level'],
                self.sldOut56Level:      ['/Mixer/Out56Level'],
                }

        self.TriggerButtonControls={
            self.btnSaveSettings:        ['/Mixer/SaveSettings'],
        }

        self.TextControls={
        }

        self.saveTextControls={
        }

        self.ComboControls={
        }

    def updateMatrixVolume(self,a0):
        SaffireMixerBase.updateMatrixVolume(self,a0)
    def updateLowResVolume(self,a0):
        SaffireMixerBase.updateLowResVolume(self,a0)
    def updateSelector(self,a0):
        SaffireMixerBase.updateSelector(self,a0)
    def triggerButton(self):
        SaffireMixerBase.triggerButton(self)
    def saveText(self):
        SaffireMixerBase.saveText(self)
    def initCombo(self, combo):
        SaffireMixerBase.initCombo(self,combo)
    def selectCombo(self, mode):
        SaffireMixerBase.selectCombo(self,mode)

    def updateValues(self):
        SaffireMixerBase.updateValues(self)
