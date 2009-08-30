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

from PyQt4.QtCore import SIGNAL, SLOT, QObject
from PyQt4.QtGui import QWidget, QHBoxLayout
from ffado.config import *
from ffado.mixer.saffire_base import SaffireMixerBase

import logging
log = logging.getLogger('saffire')

#MIXER LAYOUT:
#
#   |-- Out9/10--| |-- Out1/2 --| |-- Out3/4 --| |-- Out5/6 --| |-- Out7/8 --|
#P5  0:    0/    0  1:  110/  110  2:    0/    0  3:    0/    0  4:    0/    0
#P1  5:    0/    0  6:32767/32767  7:    0/    0  8:    0/    0  9:    0/    0
#P2 10:    0/    0 11:    0/    0 12:32767/32767 13:    0/    0 14:    0/    0
#P3 15:    0/    0 16:    0/    0 17:    0/    0 18:32767/32767 19:    0/    0
#P4 20:    0/    0 21:    0/    0 22:    0/    0 23:    0/    0 24:32767/32767
#R1 25:    0/    0 26:    0/    0 27:    0/    0 28:    0/    0 29:    0/    0
#R2 30:    0/    0 31:    0/    0 32:    0/    0 33:    0/    0 34:    0/    0
#Fx 35:    0/    0 36:    0/    0 37:    0/    0 38:    0/    0 39:    0/    0
#
#P5: DAW ch 9/10
#P1: DAW ch 1/2
#P2: DAW ch 3/4
#P3: DAW ch 5/6
#P4: DAW ch 7/8
#R1: HW INPUT ch 1/2
#R2: HW INPUT ch 3/4
#Fx: reverb/fx return

class SaffireMixer(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self, parent)

        self.mono_mode = False
        self.is_saffire_le = False

        # make a layout
        self.layout = QHBoxLayout()
        self.setLayout(self.layout)

    def show(self):
        self.selectCorrectMode()
        QWidget.show(self)

    def getMonoMode(self):
        return self.hw.getDiscrete('/Mixer/MonoMode')
    def setMonoMode(self, mode):
        if mode:
            self.hw.setDiscrete('/Mixer/MonoMode', 1)
        else:
            self.hw.setDiscrete('/Mixer/MonoMode', 0)
        self.mono_mode = self.getMonoMode()

    def getDisplayTitle(self):
        if self.is_saffire_le:
            return "SaffireLE"
        else:
            return "Saffire"

    def selectCorrectMode(self):
        if self.is_saffire_le:
            if self.samplerate <= 48000:
                log.debug("large")
                self.small.hide()
                self.large.initValues()
                self.large.show()
            else:
                log.debug("small")
                self.large.hide()
                self.small.initValues()
                self.small.show()
        else:
            if self.mono_mode:
                self.stereo.hide()
                self.mono.initValues()
                self.mono.show()
            else:
                self.mono.hide()
                self.stereo.initValues()
                self.stereo.show()

    def initValues(self):
        selected = self.samplerateselect.selected()
        self.samplerate = int(self.samplerateselect.getEnumLabel( selected ))

        # Saffire:        0x130e010001????
        # SaffireLE:    0x130e010004????
        if int(self.configrom.getGUID(), 16) >= 0x130e0100040000:
            self.is_saffire_le = True
            log.debug("Found SaffireLE GUID")
        else:
            self.is_saffire_le = False
            log.debug("Found Saffire GUID")

        # init depending on what device we have
        # and what mode it is in
        if self.is_saffire_le:
            # create the child widgets
            self.small = SaffireLEMixerSmall(self)
            self.layout.addWidget(self.small)
            self.large = SaffireLEMixerLarge(self)
            self.layout.addWidget(self.large)

            self.small.hw = self.hw
            self.small.configrom = self.configrom

            self.large.hw = self.hw
            self.large.configrom = self.configrom
        else:
            # create the child widgets
            self.mono = SaffireMixerMono(self)
            self.layout.addWidget(self.mono)
            self.stereo = SaffireMixerStereo(self)
            self.layout.addWidget(self.stereo)

            self.mono_mode = self.getMonoMode()

            self.mono.hw = self.hw
            self.mono.configrom = self.configrom

            self.stereo.hw = self.hw
            self.stereo.configrom = self.configrom

        self.selectCorrectMode()

    def polledUpdate(self):
        if self.is_saffire_le:
            if self.samplerate <= 48000:
                self.large.polledUpdate()
            else:
                self.small.polledUpdate()
        else:
            if self.mono_mode:
                self.mono.polledUpdate()
            else:
                self.stereo.polledUpdate()

class SaffireMixerStereo(QWidget, SaffireMixerBase):
    def __init__(self,parent = None):
        self.my_parent = parent
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/saffire_stereo", self)
        SaffireMixerBase.__init__(self)
        QObject.connect(self.btnRefresh, SIGNAL('clicked()'), self.updateValues)
        QObject.connect(self.btnSwitchStereoMode, SIGNAL('clicked()'), self.switchStereoMode)

        self.VolumeControls={
                self.sldPC910Out910: ['/Mixer/MatrixMixerStereo', 0, 0],
                self.sldPC910Out12: ['/Mixer/MatrixMixerStereo', 0, 1],
                self.sldPC910Out34: ['/Mixer/MatrixMixerStereo', 0, 2],
                self.sldPC910Out56: ['/Mixer/MatrixMixerStereo', 0, 3],
                self.sldPC910Out78: ['/Mixer/MatrixMixerStereo', 0, 4],
                self.sldPC12Out910: ['/Mixer/MatrixMixerStereo', 1, 0],
                self.sldPC12Out12: ['/Mixer/MatrixMixerStereo', 1, 1],
                self.sldPC12Out34: ['/Mixer/MatrixMixerStereo', 1, 2],
                self.sldPC12Out56: ['/Mixer/MatrixMixerStereo', 1, 3],
                self.sldPC12Out78: ['/Mixer/MatrixMixerStereo', 1, 4],
                self.sldPC34Out910: ['/Mixer/MatrixMixerStereo', 2, 0],
                self.sldPC34Out12: ['/Mixer/MatrixMixerStereo', 2, 1],
                self.sldPC34Out34: ['/Mixer/MatrixMixerStereo', 2, 2],
                self.sldPC34Out56: ['/Mixer/MatrixMixerStereo', 2, 3],
                self.sldPC34Out78: ['/Mixer/MatrixMixerStereo', 2, 4],
                self.sldPC56Out910: ['/Mixer/MatrixMixerStereo', 3, 0],
                self.sldPC56Out12: ['/Mixer/MatrixMixerStereo', 3, 1],
                self.sldPC56Out34: ['/Mixer/MatrixMixerStereo', 3, 2],
                self.sldPC56Out56: ['/Mixer/MatrixMixerStereo', 3, 3],
                self.sldPC56Out78: ['/Mixer/MatrixMixerStereo', 3, 4],
                self.sldPC78Out910: ['/Mixer/MatrixMixerStereo', 4, 0],
                self.sldPC78Out12: ['/Mixer/MatrixMixerStereo', 4, 1],
                self.sldPC78Out34: ['/Mixer/MatrixMixerStereo', 4, 2],
                self.sldPC78Out56: ['/Mixer/MatrixMixerStereo', 4, 3],
                self.sldPC78Out78: ['/Mixer/MatrixMixerStereo', 4, 4],

                self.sldIN12Out910: ['/Mixer/MatrixMixerStereo', 5, 0],
                self.sldIN12Out12: ['/Mixer/MatrixMixerStereo', 5, 1],
                self.sldIN12Out34: ['/Mixer/MatrixMixerStereo', 5, 2],
                self.sldIN12Out56: ['/Mixer/MatrixMixerStereo', 5, 3],
                self.sldIN12Out78: ['/Mixer/MatrixMixerStereo', 5, 4],
                self.sldIN34Out910: ['/Mixer/MatrixMixerStereo', 6, 0],
                self.sldIN34Out12: ['/Mixer/MatrixMixerStereo', 6, 1],
                self.sldIN34Out34: ['/Mixer/MatrixMixerStereo', 6, 2],
                self.sldIN34Out56: ['/Mixer/MatrixMixerStereo', 6, 3],
                self.sldIN34Out78: ['/Mixer/MatrixMixerStereo', 6, 4],

                self.sldFXOut910: ['/Mixer/MatrixMixerStereo', 7, 0],
                self.sldFXOut12: ['/Mixer/MatrixMixerStereo', 7, 1],
                self.sldFXOut34: ['/Mixer/MatrixMixerStereo', 7, 2],
                self.sldFXOut56: ['/Mixer/MatrixMixerStereo', 7, 3],
                self.sldFXOut78: ['/Mixer/MatrixMixerStereo', 7, 4],
                }


        # First column is the DBUS subpath of the control.
        # Second column is a list of linked controls that should
        # be rewritten whenever this control is updated
        self.SelectorControls={
                self.chkSpdifSwitch:    ['/Mixer/SpdifSwitch'],
                self.chkOut12Mute:      ['/Mixer/Out12Mute', [self.chkOut12HwCtrl]],
                self.chkOut12HwCtrl:    ['/Mixer/Out12HwCtrl'],
                self.chkOut12Dim:       ['/Mixer/Out12Dim'],
                self.chkOut34Mute:      ['/Mixer/Out34Mute', [self.chkOut34HwCtrl]],
                self.chkOut34HwCtrl:    ['/Mixer/Out34HwCtrl'],
                self.chkOut56Mute:      ['/Mixer/Out56Mute', [self.chkOut56HwCtrl]],
                self.chkOut56HwCtrl:    ['/Mixer/Out56HwCtrl'],
                self.chkOut78Mute:      ['/Mixer/Out78Mute', [self.chkOut78HwCtrl]],
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
                self.btnSaveSettings:      ['/Mixer/SaveSettings'],
        }

        self.TextControls={
        }

        self.saveTextControls={
        }

        self.ComboControls={
        }

    def polledUpdate(self):
        self.polledUpdateHwCtrl(self.chkOut12HwCtrl, self.sldOut12Level)
        self.polledUpdateHwCtrl(self.chkOut34HwCtrl, self.sldOut34Level)
        self.polledUpdateHwCtrl(self.chkOut56HwCtrl, self.sldOut56Level)
        self.polledUpdateHwCtrl(self.chkOut78HwCtrl, self.sldOut78Level)

    def polledUpdateHwCtrl(self, selector, volctrl):
        state = selector.isChecked()
        if state:
            self.polledUpdateVolumeLowRes('/Mixer/MonitorDial', volctrl, 64)
            volctrl.setEnabled(False)
        else:
            volctrl.setEnabled(True)

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
    def switchStereoMode(self):
        log.debug("should switch to mono mode")
        self.my_parent.setMonoMode(1)
        self.my_parent.selectCorrectMode()

class SaffireMixerMono(QWidget, SaffireMixerBase):
    def __init__(self,parent = None):
        self.my_parent = parent
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/saffire_stereo", self)
        SaffireMixerBase.__init__(self)
        QObject.connect(self.btnRefresh, SIGNAL('clicked()'), self.updateValues)
        QObject.connect(self.btnSwitchStereoMode, SIGNAL('clicked()'), self.switchStereoMode)

        self.VolumeControls={
                self.sldIN1Out910: ['/Mixer/MatrixMixerMono', 0, 0],
                self.sldIN1Out12: ['/Mixer/MatrixMixerMono', 0, 1],
                self.sldIN1Out34: ['/Mixer/MatrixMixerMono', 0, 2],
                self.sldIN1Out56: ['/Mixer/MatrixMixerMono', 0, 3],
                self.sldIN1Out78: ['/Mixer/MatrixMixerMono', 0, 4],
                self.sldIN3Out910: ['/Mixer/MatrixMixerMono', 1, 0],
                self.sldIN3Out12: ['/Mixer/MatrixMixerMono', 1, 1],
                self.sldIN3Out34: ['/Mixer/MatrixMixerMono', 1, 2],
                self.sldIN3Out56: ['/Mixer/MatrixMixerMono', 1, 3],
                self.sldIN3Out78: ['/Mixer/MatrixMixerMono', 1, 4],
                self.sldFX1Out910: ['/Mixer/MatrixMixerMono', 2, 0],
                self.sldFX1Out12: ['/Mixer/MatrixMixerMono', 2, 1],
                self.sldFX1Out34: ['/Mixer/MatrixMixerMono', 2, 2],
                self.sldFX1Out56: ['/Mixer/MatrixMixerMono', 2, 3],
                self.sldFX1Out78: ['/Mixer/MatrixMixerMono', 2, 4],
                self.sldIN2Out910: ['/Mixer/MatrixMixerMono', 3, 0],
                self.sldIN2Out12: ['/Mixer/MatrixMixerMono', 3, 1],
                self.sldIN2Out34: ['/Mixer/MatrixMixerMono', 3, 2],
                self.sldIN2Out56: ['/Mixer/MatrixMixerMono', 3, 3],
                self.sldIN2Out78: ['/Mixer/MatrixMixerMono', 3, 4],
                self.sldIN4Out910: ['/Mixer/MatrixMixerMono', 4, 0],
                self.sldIN4Out12: ['/Mixer/MatrixMixerMono', 4, 1],
                self.sldIN4Out34: ['/Mixer/MatrixMixerMono', 4, 2],
                self.sldIN4Out56: ['/Mixer/MatrixMixerMono', 4, 3],
                self.sldIN4Out78: ['/Mixer/MatrixMixerMono', 4, 4],
                self.sldFX2Out910: ['/Mixer/MatrixMixerMono', 5, 0],
                self.sldFX2Out12: ['/Mixer/MatrixMixerMono', 5, 1],
                self.sldFX2Out34: ['/Mixer/MatrixMixerMono', 5, 2],
                self.sldFX2Out56: ['/Mixer/MatrixMixerMono', 5, 3],
                self.sldFX2Out78: ['/Mixer/MatrixMixerMono', 5, 4],

                self.sldPC910Out910: ['/Mixer/MatrixMixerMono', 6, 0],
                self.sldPC910Out12: ['/Mixer/MatrixMixerMono', 6, 1],
                self.sldPC910Out34: ['/Mixer/MatrixMixerMono', 6, 2],
                self.sldPC910Out56: ['/Mixer/MatrixMixerMono', 6, 3],
                self.sldPC910Out78: ['/Mixer/MatrixMixerMono', 6, 4],
                self.sldPC12Out910: ['/Mixer/MatrixMixerMono', 7, 0],
                self.sldPC12Out12: ['/Mixer/MatrixMixerMono', 7, 1],
                self.sldPC12Out34: ['/Mixer/MatrixMixerMono', 7, 2],
                self.sldPC12Out56: ['/Mixer/MatrixMixerMono', 7, 3],
                self.sldPC12Out78: ['/Mixer/MatrixMixerMono', 7, 4],
                self.sldPC34Out910: ['/Mixer/MatrixMixerMono', 8, 0],
                self.sldPC34Out12: ['/Mixer/MatrixMixerMono', 8, 1],
                self.sldPC34Out34: ['/Mixer/MatrixMixerMono', 8, 2],
                self.sldPC34Out56: ['/Mixer/MatrixMixerMono', 8, 3],
                self.sldPC34Out78: ['/Mixer/MatrixMixerMono', 8, 4],
                self.sldPC56Out910: ['/Mixer/MatrixMixerMono', 9, 0],
                self.sldPC56Out12: ['/Mixer/MatrixMixerMono', 9, 1],
                self.sldPC56Out34: ['/Mixer/MatrixMixerMono', 9, 2],
                self.sldPC56Out56: ['/Mixer/MatrixMixerMono', 9, 3],
                self.sldPC56Out78: ['/Mixer/MatrixMixerMono', 9, 4],
                self.sldPC78Out910: ['/Mixer/MatrixMixerMono', 10, 0],
                self.sldPC78Out12: ['/Mixer/MatrixMixerMono', 10, 1],
                self.sldPC78Out34: ['/Mixer/MatrixMixerMono', 10, 2],
                self.sldPC78Out56: ['/Mixer/MatrixMixerMono', 10, 3],
                self.sldPC78Out78: ['/Mixer/MatrixMixerMono', 10, 4],


                }


        # First column is the DBUS subpath of the control.
        # Second column is a list of linked controls that should
        # be rewritten whenever this control is updated
        self.SelectorControls={
                self.chkSpdifSwitch:    ['/Mixer/SpdifSwitch'],
                self.chkOut12Mute:      ['/Mixer/Out12Mute', [self.chkOut12HwCtrl]],
                self.chkOut12HwCtrl:    ['/Mixer/Out12HwCtrl'],
                self.chkOut12Dim:       ['/Mixer/Out12Dim'],
                self.chkOut34Mute:      ['/Mixer/Out34Mute', [self.chkOut34HwCtrl]],
                self.chkOut34HwCtrl:    ['/Mixer/Out34HwCtrl'],
                self.chkOut56Mute:      ['/Mixer/Out56Mute', [self.chkOut56HwCtrl]],
                self.chkOut56HwCtrl:    ['/Mixer/Out56HwCtrl'],
                self.chkOut78Mute:      ['/Mixer/Out78Mute', [self.chkOut78HwCtrl]],
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
                self.btnSaveSettings:      ['/Mixer/SaveSettings'],
        }

        self.TextControls={
        }

        self.saveTextControls={
        }

        self.ComboControls={
        }

    def polledUpdate(self):
        self.polledUpdateHwCtrl(self.chkOut12HwCtrl, self.sldOut12Level)
        self.polledUpdateHwCtrl(self.chkOut34HwCtrl, self.sldOut34Level)
        self.polledUpdateHwCtrl(self.chkOut56HwCtrl, self.sldOut56Level)
        self.polledUpdateHwCtrl(self.chkOut78HwCtrl, self.sldOut78Level)

    def polledUpdateHwCtrl(self, selector, volctrl):
        state = selector.isChecked()
        if state:
            self.polledUpdateVolumeLowRes('/Mixer/MonitorDial', volctrl, 4)
            volctrl.setEnabled(False)
        else:
            volctrl.setEnabled(True)

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

    def switchStereoMode(self):
        log.debug("should switch to stereo mode")
        self.my_parent.setMonoMode(0)
        self.my_parent.selectCorrectMode()

class SaffireLEMixerLarge(QWidget, SaffireMixerBase):
    def __init__(self,parent = None):
        self.my_parent = parent
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/saffirele_large", self)
        SaffireMixerBase.__init__(self)

        log.debug("Init large Saffire LE mixer window")

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

    def polledUpdate(self):
        #fixme do what it takes to make the gui follow the front panel dial
        pass

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

class SaffireLEMixerSmall(QWidget, SaffireMixerBase):
    def __init__(self,parent = None):
        self.my_parent = parent
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/saffirele_small", self)
        SaffireMixerBase.__init__(self)

        log.debug("Init small Saffire LE mixer window")

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

    def polledUpdate(self):
        #fixme do what it takes to make the gui follow the front panel dial
        pass

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

# vim: et
