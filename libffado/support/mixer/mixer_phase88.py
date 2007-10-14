# -*- coding: utf-8 -*-

# (C) 2007 Pieter Palmers

from qt import *
from mixer_phase88ui import *

class Phase88Control(Phase88ControlUI):
    def __init__(self,parent = None,name = None,modal = 0,fl = 0):
        Phase88ControlUI.__init__(self,parent,name,modal,fl)

    def switchFrontState(self,a0):
            self.setSelector('frontback', a0)

    def switchOutAssign(self,a0):
            self.setSelector('outassign', a0)

    def switchWaveInAssign(self,a0):
            self.setSelector('inassign', a0)

    def switchSyncSource(self,a0):
            self.setSelector('syncsource', a0)

    def switchExtSyncSource(self,a0):
            self.setSelector('externalsync', a0)

    def setVolume12(self,a0):
            self.setVolume('line12', a0)

    def setVolume34(self,a0):
            self.setVolume('line34', a0)

    def setVolume56(self,a0):
            self.setVolume('line56', a0)

    def setVolume78(self,a0):
            self.setVolume('line78', a0)

    def setVolumeSPDIF(self,a0):
            self.setVolume('spdif', a0)

    def setVolumeWavePlay(self,a0):
            self.setVolume('waveplay', a0)

    def setVolumeMaster(self,a0):
            self.setVolume('master', a0)

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
            print "Init Phase88 mixer window"

            self.VolumeControls={
                'master':    ['/Mixer/Feature_1', self.sldInputMaster], 
                'line12' :   ['/Mixer/Feature_2', self.sldInput12],
                'line34' :   ['/Mixer/Feature_3', self.sldInput34],
                'line56' :   ['/Mixer/Feature_4', self.sldInput56],
                'line78' :   ['/Mixer/Feature_5', self.sldInput78],
                'spdif' :    ['/Mixer/Feature_6', self.sldInputSPDIF],
                'waveplay' : ['/Mixer/Feature_7', self.sldInputWavePlay],
                }

            self.SelectorControls={
                'outassign':    ['/Mixer/Selector_6', self.comboOutAssign], 
                'inassign':     ['/Mixer/Selector_7', self.comboInAssign], 
                'externalsync': ['/Mixer/Selector_8', self.comboExtSync], 
                'syncsource':   ['/Mixer/Selector_9', self.comboSyncSource], 
                'frontback':    ['/Mixer/Selector_10', self.comboFrontBack], 
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
