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

from PyQt4.QtGui import QWidget, QHBoxLayout
from mixer_saffire_base import SaffireMixerBase
from mixer_saffirepro_largeui import Ui_SaffireProMixerLargeUI
from mixer_saffirepro_smallui import Ui_SaffireProMixerSmallUI

class SaffireProMixer(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self, parent)

        self.have_adat = False
        self.samplerate = None
        self.is_pro10 = None

        # make a layout
        self.layout = QHBoxLayout()
        self.setLayout(self.layout)

    def show(self):
        self.selectCorrectMode()
        QWidget.show(self)

    def getDisplayTitle(self):
        try:
            return self.nickname.text()
        except:
            if self.is_pro10:
                return "SaffirePRO10"
            else:
                return "SaffirePRO26"

    def selectCorrectMode(self):
        if self.samplerate <= 96000:
            print "large"
            self.small.hide()
            self.large.initValues()
            self.large.show()
        else:
            print "small"
            self.large.hide()
            self.small.initValues()
            self.small.show()

    def initValues(self):
        selected = self.samplerateselect.selected()
        self.samplerate = int(self.samplerateselect.getEnumLabel( selected ))
        print "detected samplerate %d" % self.samplerate

        # adat on PRO26 makes a difference
        modelId = self.configrom.getModelId()
        if modelId == 0x00000003: # PRO26
            self.is_pro10 = False
            state = self.hw.getDiscrete('/Control/ADATDisable')
            if state:
                self.have_adat = False
                print "detected PRO26, ADAT disabled"
            else:
                self.have_adat = True
                print "detected PRO26, ADAT enabled"
        elif modelId == 0x00000006: # PRO10
            self.is_pro10 = True
            self.have_adat = False

        # create the child widgets
        self.small = SaffireProMixerSmall(self)
        self.layout.addWidget(self.small)
        self.large = SaffireProMixerLarge(self)
        self.layout.addWidget(self.large)

        self.small.hw = self.hw
        self.small.configrom = self.configrom

        self.large.hw = self.hw
        self.large.configrom = self.configrom

        self.selectCorrectMode()

    def polledUpdate(self):
        # fixme: todo
        pass

class SaffireProMixerLarge(QWidget, Ui_SaffireProMixerLargeUI, SaffireMixerBase):
    def __init__(self,parent = None):
        self.my_parent = parent
        QWidget.__init__(self,parent)
        SaffireMixerBase.__init__(self)
        self.setupUi(self)
        self.have_adat = False
        print "Init large Saffire Pro mixer window"

        self.VolumeControls={
            self.sldIMixAnalog1L: ['/Mixer/InputMix', 0, 0], 
            self.sldIMixAnalog1R: ['/Mixer/InputMix', 0, 1], 
            self.sldIMixAnalog2L: ['/Mixer/InputMix', 1, 0], 
            self.sldIMixAnalog2R: ['/Mixer/InputMix', 1, 1], 
            self.sldIMixAnalog3L: ['/Mixer/InputMix', 2, 0], 
            self.sldIMixAnalog3R: ['/Mixer/InputMix', 2, 1], 
            self.sldIMixAnalog4L: ['/Mixer/InputMix', 3, 0], 
            self.sldIMixAnalog4R: ['/Mixer/InputMix', 3, 1], 
            self.sldIMixAnalog5L: ['/Mixer/InputMix', 4, 0], 
            self.sldIMixAnalog5R: ['/Mixer/InputMix', 4, 1], 
            self.sldIMixAnalog6L: ['/Mixer/InputMix', 5, 0], 
            self.sldIMixAnalog6R: ['/Mixer/InputMix', 5, 1], 
            self.sldIMixAnalog7L: ['/Mixer/InputMix', 6, 0], 
            self.sldIMixAnalog7R: ['/Mixer/InputMix', 6, 1], 
            self.sldIMixAnalog8L: ['/Mixer/InputMix', 7, 0], 
            self.sldIMixAnalog8R: ['/Mixer/InputMix', 7, 1], 
            self.sldIMixAnalog9L: ['/Mixer/InputMix', 8, 0], 
            self.sldIMixAnalog9R: ['/Mixer/InputMix', 8, 1], 
            self.sldIMixAnalog10L: ['/Mixer/InputMix', 9, 0], 
            self.sldIMixAnalog10R: ['/Mixer/InputMix', 9, 1], 
            self.sldIMixADAT11L: ['/Mixer/InputMix', 10, 0], 
            self.sldIMixADAT11R: ['/Mixer/InputMix', 10, 1], 
            self.sldIMixADAT12L: ['/Mixer/InputMix', 11, 0], 
            self.sldIMixADAT12R: ['/Mixer/InputMix', 11, 1], 
            self.sldIMixADAT13L: ['/Mixer/InputMix', 12, 0], 
            self.sldIMixADAT13R: ['/Mixer/InputMix', 12, 1], 
            self.sldIMixADAT14L: ['/Mixer/InputMix', 13, 0], 
            self.sldIMixADAT14R: ['/Mixer/InputMix', 13, 1], 
            self.sldIMixADAT15L: ['/Mixer/InputMix', 14, 0], 
            self.sldIMixADAT15R: ['/Mixer/InputMix', 14, 1], 
            self.sldIMixADAT16L: ['/Mixer/InputMix', 15, 0], 
            self.sldIMixADAT16R: ['/Mixer/InputMix', 15, 1], 
            self.sldIMixADAT17L: ['/Mixer/InputMix', 16, 0], 
            self.sldIMixADAT17R: ['/Mixer/InputMix', 16, 1], 
            self.sldIMixADAT18L: ['/Mixer/InputMix', 17, 0], 
            self.sldIMixADAT18R: ['/Mixer/InputMix', 17, 1], 
            self.sldIMixADAT21L: ['/Mixer/InputMix', 18, 0], 
            self.sldIMixADAT21R: ['/Mixer/InputMix', 18, 1], 
            self.sldIMixADAT22L: ['/Mixer/InputMix', 19, 0], 
            self.sldIMixADAT22R: ['/Mixer/InputMix', 19, 1], 
            self.sldIMixADAT23L: ['/Mixer/InputMix', 20, 0], 
            self.sldIMixADAT23R: ['/Mixer/InputMix', 20, 1], 
            self.sldIMixADAT24L: ['/Mixer/InputMix', 21, 0], 
            self.sldIMixADAT24R: ['/Mixer/InputMix', 21, 1], 
            self.sldIMixADAT25L: ['/Mixer/InputMix', 22, 0], 
            self.sldIMixADAT25R: ['/Mixer/InputMix', 22, 1], 
            self.sldIMixADAT26L: ['/Mixer/InputMix', 23, 0], 
            self.sldIMixADAT26R: ['/Mixer/InputMix', 23, 1], 
            self.sldIMixADAT27L: ['/Mixer/InputMix', 24, 0], 
            self.sldIMixADAT27R: ['/Mixer/InputMix', 24, 1], 
            self.sldIMixADAT28L: ['/Mixer/InputMix', 25, 0], 
            self.sldIMixADAT28R: ['/Mixer/InputMix', 25, 1],
            
            self.sldOMixPC1O1: ['/Mixer/OutputMix', 0, 0], 
            self.sldOMixPC2O2: ['/Mixer/OutputMix', 1, 1], 
            self.sldOMixPC3O3: ['/Mixer/OutputMix', 2, 2], 
            self.sldOMixPC4O4: ['/Mixer/OutputMix', 3, 3], 
            self.sldOMixPC5O5: ['/Mixer/OutputMix', 4, 4], 
            self.sldOMixPC6O6: ['/Mixer/OutputMix', 5, 5], 
            self.sldOMixPC7O7: ['/Mixer/OutputMix', 6, 6], 
            self.sldOMixPC8O8: ['/Mixer/OutputMix', 7, 7], 
            self.sldOMixPC9O9: ['/Mixer/OutputMix', 8, 8], 
            self.sldOMixPC10O10: ['/Mixer/OutputMix', 9, 9],
            
            self.sldOMixPC1O3: ['/Mixer/OutputMix', 0, 2], 
            self.sldOMixPC2O4: ['/Mixer/OutputMix', 1, 3], 
            self.sldOMixPC1O5: ['/Mixer/OutputMix', 0, 4], 
            self.sldOMixPC2O6: ['/Mixer/OutputMix', 1, 5], 
            self.sldOMixPC1O7: ['/Mixer/OutputMix', 0, 6], 
            self.sldOMixPC2O8: ['/Mixer/OutputMix', 1, 7], 
            self.sldOMixPC1O9: ['/Mixer/OutputMix', 0, 8], 
            self.sldOMixPC2O10: ['/Mixer/OutputMix', 1, 9], 
            
            self.sldOMixIMixO1: ['/Mixer/OutputMix', 10, 0], 
            self.sldOMixIMixO2: ['/Mixer/OutputMix', 11, 1], 
            self.sldOMixIMixO3: ['/Mixer/OutputMix', 10, 2], 
            self.sldOMixIMixO4: ['/Mixer/OutputMix', 11, 3], 
            self.sldOMixIMixO5: ['/Mixer/OutputMix', 10, 4], 
            self.sldOMixIMixO6: ['/Mixer/OutputMix', 11, 5], 
            self.sldOMixIMixO7: ['/Mixer/OutputMix', 10, 6], 
            self.sldOMixIMixO8: ['/Mixer/OutputMix', 11, 7], 
            self.sldOMixIMixO9: ['/Mixer/OutputMix', 10, 8], 
            self.sldOMixIMixO10: ['/Mixer/OutputMix', 11, 9], 
            }


        self.SelectorControls={
            # control elements
            self.chkInsert1: ['/Control/Insert1'], 
            self.chkInsert2: ['/Control/Insert2'], 
            self.chkPhantom14: ['/Control/Phantom_1to4'], 
            self.chkPhantom58: ['/Control/Phantom_5to8'], 
            self.chkAC3: ['/Control/AC3pass'], 
            self.chkMidiThru: ['/Control/MidiTru'], 
            self.chkHighVoltage: ['/Control/UseHighVoltageRail'], 
            #self.chkEnableADAT1: ['/Control/EnableAdat1'], 
            #self.chkEnableADAT2: ['/Control/EnableAdat2'],
            #self.chkEnableSPDIF1: ['/Control/EnableSPDIF1'],
            self.chkMidiEnable: ['/Control/MIDIEnable'],
            self.chkAdatDisable: ['/Control/ADATDisable'],
            # Mixer switches
            self.chkMute12: ['/Mixer/Out12Mute'],
            self.chkHwCtrl12: ['/Mixer/Out12HwCtrl'],
            self.chkPad12: ['/Mixer/Out12Pad'],
            self.chkDim12: ['/Mixer/Out12Dim'],
            self.chkMute34: ['/Mixer/Out34Mute'],
            self.chkHwCtrl34: ['/Mixer/Out34HwCtrl'],
            self.chkPad34: ['/Mixer/Out34Pad'],
            self.chkDim34: ['/Mixer/Out34Dim'],
            self.chkMute56: ['/Mixer/Out56Mute'],
            self.chkHwCtrl56: ['/Mixer/Out56HwCtrl'],
            self.chkPad56: ['/Mixer/Out56Pad'],
            self.chkDim56: ['/Mixer/Out56Dim'],
            self.chkMute78: ['/Mixer/Out78Mute'],
            self.chkHwCtrl78: ['/Mixer/Out78HwCtrl'],
            self.chkPad78: ['/Mixer/Out78Pad'],
            self.chkDim78: ['/Mixer/Out78Dim'],
            # direct monitoring
            self.chkMonitor1: ['/Mixer/DirectMonitorCH1'],
            self.chkMonitor2: ['/Mixer/DirectMonitorCH2'],
            self.chkMonitor3: ['/Mixer/DirectMonitorCH3'],
            self.chkMonitor4: ['/Mixer/DirectMonitorCH4'],
            self.chkMonitor5: ['/Mixer/DirectMonitorCH5'],
            self.chkMonitor6: ['/Mixer/DirectMonitorCH6'],
            self.chkMonitor7: ['/Mixer/DirectMonitorCH7'],
            self.chkMonitor8: ['/Mixer/DirectMonitorCH8'],
        }

        self.VolumeControlsLowRes={
            self.sldOut12Level:      ['/Mixer/Out12Level'],
            self.sldOut34Level:      ['/Mixer/Out34Level'],
            self.sldOut56Level:      ['/Mixer/Out56Level'],
            self.sldOut78Level:      ['/Mixer/Out78Level'],
        }

        self.TriggerButtonControls={
            self.btnReboot:        ['/Control/Reboot'],
            self.btnIdentify:      ['/Control/FlashLed'],
            self.btnSaveSettings:  ['/Control/SaveSettings'],
        }

        self.TextControls={
        }

        self.saveTextControls={
        }

        self.ComboControls={
            self.comboStandalone:        ['/Control/StandaloneConfig'],
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
        for i in range(self.tabInputMix.count()):
            self.tabInputMix.setTabEnabled(i, True)

        if not self.my_parent.have_adat:
            for i in range(self.tabInputMix.count()):
                page = self.tabInputMix.widget(i)
                name = page.objectName()
                if name[0:4] == "adat":
                    self.tabInputMix.setTabEnabled(i, False)
                else:
                    self.tabInputMix.setTabEnabled(i, True)

        self.tabInputMix.setCurrentWidget(self.tabInputMix.widget(0))
        SaffireMixerBase.updateValues(self)

class SaffireProMixerSmall(QWidget, Ui_SaffireProMixerSmallUI, SaffireMixerBase):
    def __init__(self,parent = None):
        self.my_parent = parent
        QWidget.__init__(self,parent)
        SaffireMixerBase.__init__(self)
        self.setupUi(self)
        print "Init small Saffire Pro mixer window"

        self.VolumeControls={

            self.sldOMixPC1O1: ['/Mixer/OutputMix', 0, 0], 
            self.sldOMixPC2O2: ['/Mixer/OutputMix', 1, 1], 
            self.sldOMixPC3O3: ['/Mixer/OutputMix', 2, 2], 
            self.sldOMixPC4O4: ['/Mixer/OutputMix', 3, 3], 
            self.sldOMixPC5O5: ['/Mixer/OutputMix', 4, 4], 
            self.sldOMixPC6O6: ['/Mixer/OutputMix', 5, 5], 
            self.sldOMixPC7O7: ['/Mixer/OutputMix', 6, 6], 
            self.sldOMixPC8O8: ['/Mixer/OutputMix', 7, 7], 
            self.sldOMixPC9O9: ['/Mixer/OutputMix', 8, 8], 
            self.sldOMixPC10O10: ['/Mixer/OutputMix', 9, 9],
            
            self.sldOMixPC1O3: ['/Mixer/OutputMix', 0, 2], 
            self.sldOMixPC2O4: ['/Mixer/OutputMix', 1, 3], 
            self.sldOMixPC1O5: ['/Mixer/OutputMix', 0, 4], 
            self.sldOMixPC2O6: ['/Mixer/OutputMix', 1, 5], 
            self.sldOMixPC1O7: ['/Mixer/OutputMix', 0, 6], 
            self.sldOMixPC2O8: ['/Mixer/OutputMix', 1, 7], 
            self.sldOMixPC1O9: ['/Mixer/OutputMix', 0, 8], 
            self.sldOMixPC2O10: ['/Mixer/OutputMix', 1, 9], 
            
            self.sldOMixIMixO1: ['/Mixer/OutputMix', 10, 0], 
            self.sldOMixIMixO2: ['/Mixer/OutputMix', 11, 1], 
            self.sldOMixIMixO3: ['/Mixer/OutputMix', 10, 2], 
            self.sldOMixIMixO4: ['/Mixer/OutputMix', 11, 3], 
            self.sldOMixIMixO5: ['/Mixer/OutputMix', 10, 4], 
            self.sldOMixIMixO6: ['/Mixer/OutputMix', 11, 5], 
            self.sldOMixIMixO7: ['/Mixer/OutputMix', 10, 6], 
            self.sldOMixIMixO8: ['/Mixer/OutputMix', 11, 7], 
            self.sldOMixIMixO9: ['/Mixer/OutputMix', 10, 8], 
            self.sldOMixIMixO10: ['/Mixer/OutputMix', 11, 9], 
            }


        self.SelectorControls={
            # control elements
            self.chkInsert1: ['/Control/Insert1'], 
            self.chkInsert2: ['/Control/Insert2'], 
            self.chkPhantom14: ['/Control/Phantom_1to4'], 
            self.chkPhantom58: ['/Control/Phantom_5to8'], 
            self.chkAC3: ['/Control/AC3pass'], 
            self.chkMidiThru: ['/Control/MidiTru'], 
            self.chkHighVoltage: ['/Control/UseHighVoltageRail'], 
            #self.chkEnableADAT1: ['/Control/EnableAdat1'], 
            #self.chkEnableADAT2: ['/Control/EnableAdat2'],
            #self.chkEnableSPDIF1: ['/Control/EnableSPDIF1'],
            self.chkMidiEnable: ['/Control/MIDIEnable'],
            self.chkAdatDisable: ['/Control/ADATDisable'],
            # Mixer switches
            self.chkMute12: ['/Mixer/Out12Mute'],
            self.chkHwCtrl12: ['/Mixer/Out12HwCtrl'],
            self.chkPad12: ['/Mixer/Out12Pad'],
            self.chkDim12: ['/Mixer/Out12Dim'],
            self.chkMute34: ['/Mixer/Out34Mute'],
            self.chkHwCtrl34: ['/Mixer/Out34HwCtrl'],
            self.chkPad34: ['/Mixer/Out34Pad'],
            self.chkDim34: ['/Mixer/Out34Dim'],
            self.chkMute56: ['/Mixer/Out56Mute'],
            self.chkHwCtrl56: ['/Mixer/Out56HwCtrl'],
            self.chkPad56: ['/Mixer/Out56Pad'],
            self.chkDim56: ['/Mixer/Out56Dim'],
            self.chkMute78: ['/Mixer/Out78Mute'],
            self.chkHwCtrl78: ['/Mixer/Out78HwCtrl'],
            self.chkPad78: ['/Mixer/Out78Pad'],
            self.chkDim78: ['/Mixer/Out78Dim'],
            # direct monitoring
            self.chkMonitor1: ['/Mixer/DirectMonitorCH1'],
            self.chkMonitor2: ['/Mixer/DirectMonitorCH2'],
            self.chkMonitor3: ['/Mixer/DirectMonitorCH3'],
            self.chkMonitor4: ['/Mixer/DirectMonitorCH4'],
            self.chkMonitor5: ['/Mixer/DirectMonitorCH5'],
            self.chkMonitor6: ['/Mixer/DirectMonitorCH6'],
            self.chkMonitor7: ['/Mixer/DirectMonitorCH7'],
            self.chkMonitor8: ['/Mixer/DirectMonitorCH8'],
        }

        self.VolumeControlsLowRes={
            self.sldOut12Level:      ['/Mixer/Out12Level'],
            self.sldOut34Level:      ['/Mixer/Out34Level'],
            self.sldOut56Level:      ['/Mixer/Out56Level'],
            self.sldOut78Level:      ['/Mixer/Out78Level'],
        }

        self.TriggerButtonControls={
            self.btnReboot:        ['/Control/Reboot'],
            self.btnIdentify:      ['/Control/FlashLed'],
            self.btnSaveSettings:  ['/Control/SaveSettings'],
        }

        self.TextControls={
        }

        self.saveTextControls={
        }

        self.ComboControls={
            self.comboStandalone:        ['/Control/StandaloneConfig'],
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
