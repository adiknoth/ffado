#
# Copyright (C) 2005-2007 by Pieter Palmers
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# FFADO is based upon FreeBoB.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
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
from mixer_saffireproui import *

class SaffireProMixer(SaffireProMixerUI):
    def __init__(self,parent = None,name = None,modal = 0,fl = 0):
        SaffireProMixerUI.__init__(self,parent,name,modal,fl)

    def updateMatrixVolume(self,a0):
        sender = self.sender()
        vol = 0x7FFF-a0
        print "set %s %d %d to %d" % (
                    self.VolumeControls[sender][0],
                    self.VolumeControls[sender][1],
                    self.VolumeControls[sender][2],
                    vol)
        self.hw.setMatrixMixerValue(self.VolumeControls[sender][0], 
                                    self.VolumeControls[sender][1],
                                    self.VolumeControls[sender][2],
                                    vol)
    def updateLowResVolume(self,a0):
        sender = self.sender()
        vol = a0
        print "set %s to %d" % (
                    self.VolumeControlsLowRes[sender][0],
                    vol)
        self.hw.setDiscrete(self.VolumeControlsLowRes[sender][0], vol)

    def updateSelector(self,a0):
        sender = self.sender()
        if a0:
            state = 1
        else:
            state = 0
        print "set %s to %d" % (
                    self.SelectorControls[sender][0],
                    state)
        self.hw.setDiscrete(self.SelectorControls[sender][0], state)

    def triggerButton(self):
        sender = self.sender()
        print "trigger %s" % (
                    self.TriggerButtonControls[sender][0])
        self.hw.setDiscrete(self.TriggerButtonControls[sender][0], 1)

    def init(self):
            print "Init Saffire Pro mixer window"

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
            }

            self.VolumeControlsLowRes={
                self.sldOut12Level:      ['/Mixer/Out12Level'],
                self.sldOut34Level:      ['/Mixer/Out34Level'],
                self.sldOut56Level:      ['/Mixer/Out56Level'],
                self.sldOut78Level:      ['/Mixer/Out78Level'],
            }

            self.TriggerButtonControls={
                self.btnReboot:      ['/Control/Reboot'],
                self.btnIdentify:    ['/Control/FlashLed'],
            }

    def updateValues(self):
        for ctrl, info in self.VolumeControls.iteritems():
            vol = self.hw.getMatrixMixerValue(self.VolumeControls[ctrl][0],
                                                self.VolumeControls[ctrl][1],
                                                self.VolumeControls[ctrl][2])

            print "%s volume is %d" % (ctrl.name() , 0x7FFF-vol)
            ctrl.setValue(0x7FFF-vol)

        for ctrl, info in self.VolumeControlsLowRes.iteritems():
            vol = self.hw.getDiscrete(self.VolumeControlsLowRes[ctrl][0])

            print "%s volume is %d" % (ctrl.name() , vol)
            ctrl.setValue(vol)

        for ctrl, info in self.SelectorControls.iteritems():
            state = self.hw.getDiscrete(self.SelectorControls[ctrl][0])
            print "%s state is %d" % (ctrl.name() , state)
            if state:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)

        for ctrl, info in self.TriggerButtonControls.iteritems():
            pass

    def initValues(self):
        self.updateValues()
        for ctrl, info in self.VolumeControls.iteritems():
            # connect the UI element
            QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateMatrixVolume)

        for ctrl, info in self.VolumeControlsLowRes.iteritems():
            # connect the UI element
            QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateLowResVolume)

        for ctrl, info in self.SelectorControls.iteritems():
            # connect the UI element
            QObject.connect(ctrl,SIGNAL('stateChanged(int)'),self.updateSelector)

        for ctrl, info in self.TriggerButtonControls.iteritems():
            # connect the UI element
            QObject.connect(ctrl,SIGNAL('clicked()'),self.triggerButton)

