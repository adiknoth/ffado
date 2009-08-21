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

from PyQt4.QtCore import SIGNAL, SLOT, QObject, Qt
from PyQt4.QtGui import QWidget, QHBoxLayout, QVBoxLayout, \
                        QGroupBox, QTabWidget, QLabel, \
                        QPushButton, QSpacerItem, QSizePolicy
from ffadomixer_config import *
import logging
log = logging.getLogger('audiofire')

class AfMonitorWidget(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        uicLoad("mixer_audiofire_strip", self)

class AfSettingsWidget(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        uicLoad("mixer_audiofire_settings", self)

class AudioFireMixer(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        log.debug("Init AudioFire mixer window")

    def getDisplayTitle(self):
        modelId = self.configrom.getModelId()
        if modelId == 0x0AF2:
            return "AudioFire2"
        if modelId == 0x0AF4:
            return "AudioFire4"
        if modelId == 0x0AF8:
            return "AudioFire8"
        if modelId == 0x0AF12:
            return "AudioFire12"
        return "Generic FireWorks"

    def updateMatrixButton(self,a0):
        sender = self.sender()
        if a0:
            state = 1
        else:
            state = 0
        log.debug("set %s %d %d to %d" % (
                    self.MatrixButtonControls[sender][0],
                    self.MatrixButtonControls[sender][1],
                    self.MatrixButtonControls[sender][2],
                    state))
        self.hw.setMatrixMixerValue(self.MatrixButtonControls[sender][0], 
                                    self.MatrixButtonControls[sender][1],
                                    self.MatrixButtonControls[sender][2],
                                    state)

    def updateMatrixRotary(self,a0):
        sender = self.sender()
        vol = a0
        log.debug("set %s %d %d to %d" % (
                    self.MatrixRotaryControls[sender][0],
                    self.MatrixRotaryControls[sender][1],
                    self.MatrixRotaryControls[sender][2],
                    vol))
        self.hw.setMatrixMixerValue(self.MatrixRotaryControls[sender][0], 
                                    self.MatrixRotaryControls[sender][1],
                                    self.MatrixRotaryControls[sender][2],
                                    vol)

    def updateMatrixVolume(self,a0):
        sender = self.sender()
        vol = a0
        #vol = 0x01000000-vol
        log.debug("set %s %d %d to %d" % (
                    self.MatrixVolumeControls[sender][0],
                    self.MatrixVolumeControls[sender][1],
                    self.MatrixVolumeControls[sender][2],
                    vol))
        self.hw.setMatrixMixerValue(self.MatrixVolumeControls[sender][0], 
                                    self.MatrixVolumeControls[sender][1],
                                    self.MatrixVolumeControls[sender][2],
                                    vol)

    def updateVolume(self,a0):
        sender = self.sender()
        vol = a0
        #vol = 0x01000000-vol
        log.debug("set %s to %d" % (
                    self.VolumeControls[sender][0],
                    vol))
        self.hw.setContignuous(self.VolumeControls[sender][0],
                              vol)

    def updateSelector(self,a0):
        sender = self.sender()
        if a0:
            state = 1
        else:
            state = 0
        log.debug("set %s to %d" % (
                    self.SelectorControls[sender][0],
                    state))
        self.hw.setDiscrete(self.SelectorControls[sender][0], state)

    def updateTrigger(self):
        sender = self.sender()
        log.debug("trigger %s" % (self.TriggerControls[sender][0]))
        self.hw.setDiscrete(self.TriggerControls[sender][0], 1)

    def updateSPDIFmodeControl(self,a0):
        sender = self.sender()
        if a0:
            state = 1
        else:
            state = 0
        if state:
            log.debug("set %s to %d" % (
                        self.SPDIFmodeControls[sender][0],
                        self.SPDIFmodeControls[sender][1]))
            self.hw.setDiscrete(self.SPDIFmodeControls[sender][0], self.SPDIFmodeControls[sender][1])

    def buildMixer(self):
        log.debug("Building mixer")
        self.MatrixButtonControls={}
        self.MatrixRotaryControls={}
        self.MatrixVolumeControls={}
        self.VolumeControls={}
        self.SelectorControls={}
        self.SPDIFmodeControls={}
        self.TriggerControls={}

        nb_pys_out = self.hw.getDiscrete("/HwInfo/PhysicalAudioOutCount")
        nb_pys_in = self.hw.getDiscrete("/HwInfo/PhysicalAudioInCount")

        outputtabslayout = QHBoxLayout( self )
        outputtabs = QTabWidget(self)
        outputtabslayout.addWidget( outputtabs, 1 )

        for outpair in range(nb_pys_out/2):
            tab = QWidget( outputtabs )
            tablayout = QHBoxLayout( tab )
            
            grpMonitor = QGroupBox(tab)
            tablayout.addWidget(grpMonitor)
    
            grpPlayback = QGroupBox(tab)
            tablayout.addWidget(grpPlayback)
    
            grpOutput = QGroupBox(tab)
            tablayout.addWidget(grpOutput)
    
            grpMonitor.setTitle("Monitor")
            grpPlayback.setTitle("Playback")
            grpOutput.setTitle("Output")
            
            # monitor controls
            grpMonitorLayout = QHBoxLayout()
            grpMonitor.setLayout(grpMonitorLayout);
            output_id = outpair * 2
            for inpair in range(nb_pys_in/2):
                # create GUI elements
                strip = AfMonitorWidget( grpMonitor )
                grpMonitorLayout.addWidget( strip, 1 )
                input_id = inpair*2
                strip.lblName.setText("IN %d/%d" % (input_id+1, input_id+2))

                # add the elements to the control structure

                self.MatrixButtonControls[strip.btnMute0] = ['/Mixer/MonitorMute', input_id, output_id]
                self.MatrixButtonControls[strip.btnMute1] = ['/Mixer/MonitorMute', input_id + 1, output_id + 1]
                self.MatrixButtonControls[strip.btnSolo0] = ['/Mixer/MonitorSolo', input_id, output_id]
                self.MatrixButtonControls[strip.btnSolo1] = ['/Mixer/MonitorSolo', input_id + 1, output_id + 1]
                self.MatrixRotaryControls[strip.rotPan0]  = ['/Mixer/MonitorPan', input_id, output_id]
                self.MatrixRotaryControls[strip.rotPan1]  = ['/Mixer/MonitorPan', input_id + 1, output_id + 1]
                self.MatrixVolumeControls[strip.sldGain0]  = ['/Mixer/MonitorGain', input_id, output_id]
                self.MatrixVolumeControls[strip.sldGain1]  = ['/Mixer/MonitorGain', input_id + 1, output_id + 1]

            # playback
            grpPlaybackLayout = QHBoxLayout()
            grpPlayback.setLayout(grpPlaybackLayout);
            strip = AfMonitorWidget( grpPlayback )
            grpPlaybackLayout.addWidget(strip, 1)
            strip.lblName.setText("Playback %d/%d" % (output_id+1, output_id+2))

            self.VolumeControls[strip.sldGain0] = ["/Mixer/PC%dGain" % (output_id)]
            self.VolumeControls[strip.sldGain1] = ["/Mixer/PC%dGain" % (output_id+1)]
            self.SelectorControls[strip.btnMute0] = ["/Mixer/PC%dMute" % (output_id)]
            self.SelectorControls[strip.btnMute1] = ["/Mixer/PC%dMute" % (output_id+1)]
            self.SelectorControls[strip.btnSolo0] = ["/Mixer/PC%dSolo" % (output_id)]
            self.SelectorControls[strip.btnSolo1] = ["/Mixer/PC%dSolo" % (output_id+1)]

            # fix up mixer strip gui
            strip.rotPan0.hide()
            strip.rotPan1.hide()

            # output
            grpOutputLayout = QHBoxLayout()
            grpOutput.setLayout(grpOutputLayout);
            strip = AfMonitorWidget( grpOutput )
            grpOutputLayout.addWidget(strip, 1)
            strip.lblName.setText("Output %d/%d" % (output_id+1, output_id+2))

            self.VolumeControls[strip.sldGain0] = ["/Mixer/OUT%dGain" % (output_id)]
            self.VolumeControls[strip.sldGain1] = ["/Mixer/OUT%dGain" % (output_id+1)]
            self.SelectorControls[strip.btnMute0] = ["/Mixer/OUT%dMute" % (output_id)]
            self.SelectorControls[strip.btnMute1] = ["/Mixer/OUT%dMute" % (output_id+1)]
            self.SelectorControls[strip.btnSolo0] = ["/Mixer/OUT%dNominal" % (output_id)]
            self.SelectorControls[strip.btnSolo1] = ["/Mixer/OUT%dNominal" % (output_id+1)]

            # fix up mixer strip gui
            strip.btnSolo0.setText("Pad")
            strip.btnSolo1.setText("Pad")
            strip.rotPan0.hide()
            strip.rotPan1.hide()

            # add the tab
            outputtabs.addTab( tab, "OUT %d/%d" % (output_id+1, output_id+2))

        # add an input config tab
        tab = QWidget( outputtabs )
        tablayout = QHBoxLayout( tab )

        for inpair in range(nb_pys_in):
            # create GUI elements
            log.debug("strip")
            grpInput = QGroupBox(tab)
            tablayout.addWidget(grpInput)
            grpInput.setTitle("IN %d" % (inpair+1))

            grpInputLayout = QVBoxLayout()
            grpInput.setLayout(grpInputLayout);

            label = QLabel( grpInput )
            grpInputLayout.addWidget( label )
            label.setText("frienlyname %d" % (inpair+1))
            label.setAlignment(Qt.AlignCenter)

            btn = QPushButton( grpInput )
            grpInputLayout.addWidget( btn )
            btn.setText("Pad")
            btn.setCheckable(True)
            self.SelectorControls[btn] = ["/Mixer/IN%dNominal" % (inpair)]

            spacer = QSpacerItem(1,1,QSizePolicy.Minimum,QSizePolicy.Expanding)
            grpInputLayout.addItem(spacer)

        outputtabs.addTab( tab, "INPUT")

        # add an settings tab
        tab = QWidget( outputtabs )
        tablayout = QHBoxLayout( tab )
        outputtabs.addTab( tab, "SETTINGS")
        settings = AfSettingsWidget( tab )

        has_sw_phantom = self.hw.getDiscrete("/HwInfo/PhantomPower")
        if has_sw_phantom:
            self.SelectorControls[settings.btnPhantom] = ["/PhantomPower"]
        else:
            settings.btnPhantom.hide()

        self.TriggerControls[settings.btnSaveSettings] = ["/SaveSettings"]
        self.TriggerControls[settings.btnIdentify] = ["/Identify"]

        self.SPDIFmodeControls[settings.radioConsumer] = ["/SpdifMode", 0]
        self.SPDIFmodeControls[settings.radioProfessional] = ["/SpdifMode", 1]

    def initValues(self):
        log.debug("Init values")

        for ctrl, info in self.MatrixVolumeControls.iteritems():
            vol = self.hw.getMatrixMixerValue(self.MatrixVolumeControls[ctrl][0],
                                                self.MatrixVolumeControls[ctrl][1],
                                                self.MatrixVolumeControls[ctrl][2])

            #vol = 0x01000000-vol
            log.debug("%s volume is %d" % (ctrl.objectName() , vol))
            ctrl.setValue(vol)

            # connect the UI element
            QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateMatrixVolume)

        for ctrl, info in self.MatrixButtonControls.iteritems():
            state = self.hw.getMatrixMixerValue(self.MatrixButtonControls[ctrl][0],
                                                self.MatrixButtonControls[ctrl][1],
                                                self.MatrixButtonControls[ctrl][2])

            log.debug("%s state is %d" % (ctrl.objectName() , state))
            if state:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)

            # connect the UI element
            QObject.connect(ctrl,SIGNAL('clicked(bool)'),self.updateMatrixButton)

        for ctrl, info in self.MatrixRotaryControls.iteritems():
            vol = self.hw.getMatrixMixerValue(self.MatrixRotaryControls[ctrl][0],
                                                self.MatrixRotaryControls[ctrl][1],
                                                self.MatrixRotaryControls[ctrl][2])

            log.debug("%s value is %d" % (ctrl.objectName(), vol))
            ctrl.setValue(vol)

            # connect the UI element
            QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateMatrixRotary)

        for ctrl, info in self.VolumeControls.iteritems():
            vol = self.hw.getContignuous(self.VolumeControls[ctrl][0])

            #vol = 0x01000000-vol
            log.debug("%s volume is %d" % (ctrl.objectName() , vol))
            ctrl.setValue(vol)

            # connect the UI element
            QObject.connect(ctrl,SIGNAL('valueChanged(int)'),self.updateVolume)

        for ctrl, info in self.SelectorControls.iteritems():
            state = self.hw.getDiscrete(self.SelectorControls[ctrl][0])
            log.debug("%s state is %d" % (ctrl.objectName() , state))
            if state:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)

            # connect the UI element
            QObject.connect(ctrl,SIGNAL('clicked(bool)'),self.updateSelector)

        for ctrl, info in self.TriggerControls.iteritems():
            # connect the UI element
            QObject.connect(ctrl,SIGNAL('clicked()'),self.updateTrigger)

        for ctrl, info in self.SPDIFmodeControls.iteritems():
            state = self.hw.getDiscrete(self.SPDIFmodeControls[ctrl][0])
            log.debug("%s state is %d" % (ctrl.objectName() , state))
            if state == self.SPDIFmodeControls[ctrl][1]:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)

            # connect the UI element
            QObject.connect(ctrl,SIGNAL('toggled(bool)'),self.updateSPDIFmodeControl)

# vim: et
