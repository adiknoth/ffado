# -*- coding: utf-8 -*-

# (C) 2007 Pieter Palmers

from qt import *
from mixer_af2ui import *

class AudioFire2Mixer(AudioFire2MixerUI):
    def __init__(self,parent = None,name = None,modal = 0,fl = 0):
        AudioFire2MixerUI.__init__(self,parent,name,modal,fl)

    def init(self):
            print "Init AF2 mixer window"

            self.VolumeControls={
                    self.sldOut1:    ['/Mixer/OUT0Gain'],
                    self.sldOut2:    ['/Mixer/OUT1Gain'],
                    self.sldOut3:    ['/Mixer/OUT2Gain'],
                    self.sldOut4:    ['/Mixer/OUT3Gain'],
                    self.sldOut5:    ['/Mixer/OUT4Gain'],
                    self.sldOut6:    ['/Mixer/OUT5Gain'],
                    self.sldPC1Out12:        ['/Mixer/PC0Gain'],
                    self.sldPC2Out12:        ['/Mixer/PC1Gain'],
                    self.sldPC3Out34:        ['/Mixer/PC2Gain'],
                    self.sldPC4Out34:        ['/Mixer/PC3Gain'],
                    self.sldPC5Out56:        ['/Mixer/PC4Gain'],
                    self.sldPC6Out56:        ['/Mixer/PC5Gain'],
                    }

            self.MatrixButtonControls={
                    self.btnIn1Out12Mute:    ['/Mixer/MonitorMute',0,0],
                    self.btnIn1Out12Solo:    ['/Mixer/MonitorSolo',0,0],
                    self.btnIn1Out34Mute:    ['/Mixer/MonitorMute',0,2],
                    self.btnIn1Out34Solo:    ['/Mixer/MonitorSolo',0,2],
                    self.btnIn1Out56Mute:    ['/Mixer/MonitorMute',0,4],
                    self.btnIn1Out56Solo:    ['/Mixer/MonitorSolo',0,4],
                    self.btnIn2Out12Mute:    ['/Mixer/MonitorMute',1,0],
                    self.btnIn2Out12Solo:    ['/Mixer/MonitorSolo',1,0],
                    self.btnIn2Out34Mute:    ['/Mixer/MonitorMute',1,2],
                    self.btnIn2Out34Solo:    ['/Mixer/MonitorSolo',1,2],
                    self.btnIn2Out56Mute:    ['/Mixer/MonitorMute',1,4],
                    self.btnIn2Out56Solo:    ['/Mixer/MonitorSolo',1,4],
                    self.btnIn3Out12Mute:    ['/Mixer/MonitorMute',2,0],
                    self.btnIn3Out12Solo:    ['/Mixer/MonitorSolo',2,0],
                    self.btnIn3Out34Mute:    ['/Mixer/MonitorMute',2,2],
                    self.btnIn3Out34Solo:    ['/Mixer/MonitorSolo',2,2],
                    self.btnIn3Out56Mute:    ['/Mixer/MonitorMute',2,4],
                    self.btnIn3Out56Solo:    ['/Mixer/MonitorSolo',2,4],
                    self.btnIn4Out12Mute:    ['/Mixer/MonitorMute',3,0],
                    self.btnIn4Out12Solo:    ['/Mixer/MonitorSolo',3,0],
                    self.btnIn4Out34Mute:    ['/Mixer/MonitorMute',3,2],
                    self.btnIn4Out34Solo:    ['/Mixer/MonitorSolo',3,2],
                    self.btnIn4Out56Mute:    ['/Mixer/MonitorMute',3,4],
                    self.btnIn4Out56Solo:    ['/Mixer/MonitorSolo',3,4],
                    }

            self.MatrixRotaryControls={
                    self.rotIn1Out12Pan:     ['/Mixer/MonitorPan',0,0],
                    self.rotIn1Out34Pan:     ['/Mixer/MonitorPan',0,2],
                    self.rotIn1Out56Pan:     ['/Mixer/MonitorPan',0,4],
                    self.rotIn2Out12Pan:     ['/Mixer/MonitorPan',1,0],
                    self.rotIn2Out34Pan:     ['/Mixer/MonitorPan',1,2],
                    self.rotIn2Out56Pan:     ['/Mixer/MonitorPan',1,4],
                    self.rotIn3Out12Pan:     ['/Mixer/MonitorPan',2,0],
                    self.rotIn3Out34Pan:     ['/Mixer/MonitorPan',2,2],
                    self.rotIn3Out56Pan:     ['/Mixer/MonitorPan',2,4],
                    self.rotIn4Out12Pan:     ['/Mixer/MonitorPan',3,0],
                    self.rotIn4Out34Pan:     ['/Mixer/MonitorPan',3,2],
                    self.rotIn4Out56Pan:     ['/Mixer/MonitorPan',3,4],
                    }

            self.MatrixVolumeControls={
                    self.sldIn1Out12:        ['/Mixer/MonitorGain',0,0],
                    self.sldIn1Out34:        ['/Mixer/MonitorGain',0,2],
                    self.sldIn1Out56:        ['/Mixer/MonitorGain',0,4],
                    self.sldIn2Out12:        ['/Mixer/MonitorGain',1,0],
                    self.sldIn2Out34:        ['/Mixer/MonitorGain',1,2],
                    self.sldIn2Out56:        ['/Mixer/MonitorGain',1,4],
                    self.sldIn3Out12:        ['/Mixer/MonitorGain',2,0],
                    self.sldIn3Out34:        ['/Mixer/MonitorGain',2,2],
                    self.sldIn3Out56:        ['/Mixer/MonitorGain',2,4],
                    self.sldIn4Out12:        ['/Mixer/MonitorGain',3,0],
                    self.sldIn4Out34:        ['/Mixer/MonitorGain',3,2],
                    self.sldIn4Out56:        ['/Mixer/MonitorGain',3,4],
                    }

            self.SelectorControls={
                    self.btnOut1Pad:        ['/Mixer/OUT0Nominal'],
                    self.btnOut2Pad:        ['/Mixer/OUT1Nominal'],
                    #self.btnIn1Pad:         ['/Mixer/IN0Nominal'],
                    #self.btnIn2Pad:         ['/Mixer/IN1Nominal'],
                    self.btnOut1Mute:       ['/Mixer/OUT0Mute'],
                    self.btnOut2Mute:       ['/Mixer/OUT1Mute'],
                    self.btnOut3Mute:       ['/Mixer/OUT2Mute'],
                    self.btnOut4Mute:       ['/Mixer/OUT3Mute'],
                    self.btnOut5Mute:       ['/Mixer/OUT4Mute'],
                    self.btnOut6Mute:       ['/Mixer/OUT5Mute'],
                    self.btnPC1Out12Mute:   ['/Mixer/PC0Mute'],
                    self.btnPC2Out12Mute:   ['/Mixer/PC1Mute'],
                    self.btnPC3Out34Mute:   ['/Mixer/PC2Mute'],
                    self.btnPC4Out34Mute:   ['/Mixer/PC4Mute'],
                    self.btnPC5Out56Mute:   ['/Mixer/PC3Mute'],
                    self.btnPC6Out56Mute:   ['/Mixer/PC5Mute'],
                    }


    def updateMatrixButton(self,a0):
        sender = self.sender()
        if a0:
            state = 1
        else:
            state = 0
        print "set %s %d %d to %d" % (
                    self.MatrixButtonControls[sender][0],
                    self.MatrixButtonControls[sender][1],
                    self.MatrixButtonControls[sender][2],
                    state)
        self.hw.setMatrixMixerValue(self.MatrixButtonControls[sender][0], 
                                    self.MatrixButtonControls[sender][1],
                                    self.MatrixButtonControls[sender][2],
                                    state)

    def updateMatrixRotary(self,a0):
        sender = self.sender()
        vol = a0
        print "set %s %d %d to %d" % (
                    self.MatrixRotaryControls[sender][0],
                    self.MatrixRotaryControls[sender][1],
                    self.MatrixRotaryControls[sender][2],
                    vol)
        self.hw.setMatrixMixerValue(self.MatrixRotaryControls[sender][0], 
                                    self.MatrixRotaryControls[sender][1],
                                    self.MatrixRotaryControls[sender][2],
                                    vol)

    def updateMatrixVolume(self,a0):
        sender = self.sender()
        vol = 0x01000000-a0
        print "set %s %d %d to %d" % (
                    self.MatrixVolumeControls[sender][0],
                    self.MatrixVolumeControls[sender][1],
                    self.MatrixVolumeControls[sender][2],
                    vol)
        self.hw.setMatrixMixerValue(self.MatrixVolumeControls[sender][0], 
                                    self.MatrixVolumeControls[sender][1],
                                    self.MatrixVolumeControls[sender][2],
                                    vol)

    def updateVolume(self,a0):
        sender = self.sender()
        vol = 0x01000000-a0
        print "set %s to %d" % (
                    self.VolumeControls[sender][0],
                    vol)
        self.hw.setContignuous(self.VolumeControls[sender][0],
                              vol)

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
        
    def initValues(self):
            for ctrl, info in self.MatrixVolumeControls.iteritems():
                vol = self.hw.getMatrixMixerValue(self.MatrixVolumeControls[ctrl][0],
                                                  self.MatrixVolumeControls[ctrl][1],
                                                  self.MatrixVolumeControls[ctrl][2])

                print "%s volume is %d" % (ctrl.name() , 0x01000000-vol)
                ctrl.setValue(0x01000000-vol)

                # connect the UI element
                QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateMatrixVolume)

            for ctrl, info in self.MatrixButtonControls.iteritems():
                state = self.hw.getMatrixMixerValue(self.MatrixButtonControls[ctrl][0],
                                                  self.MatrixButtonControls[ctrl][1],
                                                  self.MatrixButtonControls[ctrl][2])

                print "%s state is %d" % (ctrl.name() , state)
                if state:
                    ctrl.setOn(True)
                else:
                    ctrl.setOn(False)

                # connect the UI element
                QObject.connect(ctrl,SIGNAL('stateChanged(int)'),self.updateMatrixButton)
                
            for ctrl, info in self.MatrixRotaryControls.iteritems():
                vol = self.hw.getMatrixMixerValue(self.MatrixRotaryControls[ctrl][0],
                                                  self.MatrixRotaryControls[ctrl][1],
                                                  self.MatrixRotaryControls[ctrl][2])

                print "%s value is %d" % (ctrl.name(), vol)
                ctrl.setValue(vol)

                # connect the UI element
                QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateMatrixRotary)

            for ctrl, info in self.VolumeControls.iteritems():
                vol = self.hw.getContignuous(self.VolumeControls[ctrl][0])

                print "%s volume is %d" % (ctrl.name() , 0x01000000-vol)
                ctrl.setValue(0x01000000-vol)

                # connect the UI element
                QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateVolume)

            for ctrl, info in self.SelectorControls.iteritems():
                state = self.hw.getDiscrete(self.SelectorControls[ctrl][0])
                print "%s state is %d" % (ctrl.name() , state)
                if state:
                    ctrl.setOn(True)
                else:
                    ctrl.setOn(False)

                # connect the UI element
                QObject.connect(ctrl,SIGNAL('stateChanged(int)'),self.updateSelector)
