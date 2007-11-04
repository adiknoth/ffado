# -*- coding: utf-8 -*-

# (C) 2007 Pieter Palmers

from qt import *
from mixer_saffireui import *

class SaffireMixer(SaffireMixerUI):
    def __init__(self,parent = None,name = None,modal = 0,fl = 0):
        SaffireMixerUI.__init__(self,parent,name,modal,fl)

    def init(self):
            print "Init Saffire mixer window"

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
        
    def initValues(self):
            for ctrl, info in self.VolumeControls.iteritems():
                vol = self.hw.getMatrixMixerValue(self.VolumeControls[ctrl][0],
                                                  self.VolumeControls[ctrl][1],
                                                  self.VolumeControls[ctrl][2])

                print "%s volume is %d" % (ctrl.name() , 0x7FFF-vol)
                ctrl.setValue(0x7FFF-vol)

                # connect the UI element
                QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateMatrixVolume)

            for ctrl, info in self.VolumeControlsLowRes.iteritems():
                vol = self.hw.getDiscrete(self.VolumeControlsLowRes[ctrl][0])

                print "%s volume is %d" % (ctrl.name() , vol)
                ctrl.setValue(vol)

                # connect the UI element
                QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateLowResVolume)

            for ctrl, info in self.SelectorControls.iteritems():
                state = self.hw.getDiscrete(self.SelectorControls[ctrl][0])
                print "%s state is %d" % (ctrl.name() , state)
                if state:
                    ctrl.setChecked(True)
                else:
                    ctrl.setChecked(False)

                # connect the UI element
                QObject.connect(ctrl,SIGNAL('stateChanged(int)'),self.updateSelector)
