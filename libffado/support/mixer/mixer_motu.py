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

from qt import *
from mixer_motuui import *

class MotuMixer(MotuMixerUI):
    def __init__(self,parent = None,name = None,modal = 0,fl = 0):
        MotuMixerUI.__init__(self,parent,name,modal,fl)

    # public slot: channel/mix faders
    def updateFader(self, a0):
        sender = self.sender()
        vol = 128-a0
        print "setting %s channel/mix fader to %d" % (self.ChannelFaders[sender][0], vol)
        self.hw.setDiscrete(self.ChannelFaders[sender][0], vol)

    # public slot: channel pan
    def updatePan(self, a0):
        sender = self.sender()
        pan = a0
        print "setting %s channel pan to %d" % (self.ChannelPans[sender][0], pan)
        self.hw.setDiscrete(self.ChannelPans[sender][0], pan)

    # public slot: generic binary switch
    def updateBinarySwitch(self, a0):
        sender = self.sender()
        val=a0
        print "setting %s switch to %d" % (self.BinarySwitches[sender][0], val)
        self.hw.setDiscrete(self.BinarySwitches[sender][0], val)

    # public slot: mix destination control
    def updateMixDest(self, a0):
        sender = self.sender()
        dest=a0
        print "setting %s mix destination to %d" % (self.MixDests[sender][0], dest)
        self.hw.setDiscrete(self.MixDests[sender][0], dest)

    # public slots: mix output controls
    def set_mix1_dest(self,a0):
        self.setMixDest('mix1', a0)

    def setSelector(self,a0,a1):
        name=a0
        state = a1
        print "setting %s state to %d" % (name, state)
        self.hw.setDiscrete(self.SelectorControls[name][0], state)

    def init(self):
        print "Init MOTU mixer window"

        self.ChannelFaders={
            self.mix1ana1_fader: ['/Mixer/Mix1/Ana1_fader'],
            self.mix1ana2_fader: ['/Mixer/Mix1/Ana2_fader'],
            self.mix1ana3_fader: ['/Mixer/Mix1/Ana3_fader'],
            self.mix1ana4_fader: ['/Mixer/Mix1/Ana4_fader'],
            self.mix1ana5_fader: ['/Mixer/Mix1/Ana5_fader'],
            self.mix1ana6_fader: ['/Mixer/Mix1/Ana6_fader'],
            self.mix1ana7_fader: ['/Mixer/Mix1/Ana7_fader'],
            self.mix1ana8_fader: ['/Mixer/Mix1/Ana8_fader'],
            self.mix1_fader: ['/Mixer/Mix1/Mix_fader'],
            }

        self.ChannelPans={
            self.mix1ana1_pan:   ['/Mixer/Mix1/Ana1_pan'],
            self.mix1ana2_pan:   ['/Mixer/Mix1/Ana2_pan'],
            self.mix1ana3_pan:   ['/Mixer/Mix1/Ana3_pan'],
            self.mix1ana4_pan:   ['/Mixer/Mix1/Ana4_pan'],
            self.mix1ana5_pan:   ['/Mixer/Mix1/Ana5_pan'],
            self.mix1ana6_pan:   ['/Mixer/Mix1/Ana6_pan'],
            self.mix1ana7_pan:   ['/Mixer/Mix1/Ana7_pan'],
            self.mix1ana8_pan:   ['/Mixer/Mix1/Ana8_pan'],
        }

        self.BinarySwitches={
            self.mix1ana1_mute:  ['/Mixer/Mix1/Ana1_mute'],
            self.mix1ana2_mute:  ['/Mixer/Mix1/Ana2_mute'],
            self.mix1ana3_mute:  ['/Mixer/Mix1/Ana3_mute'],
            self.mix1ana4_mute:  ['/Mixer/Mix1/Ana4_mute'],
            self.mix1ana5_mute:  ['/Mixer/Mix1/Ana5_mute'],
            self.mix1ana6_mute:  ['/Mixer/Mix1/Ana6_mute'],
            self.mix1ana7_mute:  ['/Mixer/Mix1/Ana7_mute'],
            self.mix1ana8_mute:  ['/Mixer/Mix1/Ana8_mute'],
            self.mix1_mute:  ['/Mixer/Mix1/Mix_mute'],
            self.mix1ana1_solo:  ['/Mixer/Mix1/Ana1_solo'],
            self.mix1ana2_solo:  ['/Mixer/Mix1/Ana2_solo'],
            self.mix1ana3_solo:  ['/Mixer/Mix1/Ana3_solo'],
            self.mix1ana4_solo:  ['/Mixer/Mix1/Ana4_solo'],
            self.mix1ana5_solo:  ['/Mixer/Mix1/Ana5_solo'],
            self.mix1ana6_solo:  ['/Mixer/Mix1/Ana6_solo'],
            self.mix1ana7_solo:  ['/Mixer/Mix1/Ana7_solo'],
            self.mix1ana8_solo:  ['/Mixer/Mix1/Ana8_solo'],
            self.ana5_level:     ['/Mixer/Control/Ana5_level'],
            self.ana6_level:     ['/Mixer/Control/Ana6_level'],
            self.ana7_level:     ['/Mixer/Control/Ana7_level'],
            self.ana8_level:     ['/Mixer/Control/Ana8_level'],
            self.ana5_boost:     ['/Mixer/Control/Ana5_boost'],
            self.ana6_boost:     ['/Mixer/Control/Ana6_boost'],
            self.ana7_boost:     ['/Mixer/Control/Ana7_boost'],
            self.ana8_boost:     ['/Mixer/Control/Ana8_boost'],
        }

        self.MixDests={
            self.mix1_dest:      ['/Mixer/Mix1/Mix_dest'],
        }

        self.SelectorControls={

        }

        # Other mixer variables
        self.is_streaming = 0
        self.sample_rate = 0

    def initValues(self):
        # Is the device streaming?
        self.is_streaming = self.hw.getDiscrete('/Mixer/Info/IsStreaming')
        print "device streaming flag: %d" % (self.is_streaming)

        # Retrieve other device settings as needed
        self.sample_rate = self.hw.getDiscrete('/Mixer/Info/SampleRate')
        print "device sample rate: %d" % (self.sample_rate)
        self.has_mic_inputs = self.hw.getDiscrete('/Mixer/Info/HasMicInputs')
        print "device has mic inputs: %d" % (self.has_mic_inputs)
        self.has_aesebu_inputs = self.hw.getDiscrete('/Mixer/Info/HasAESEBUInputs')
        print "device has AES/EBU inputs: %d" % (self.has_aesebu_inputs)
        self.has_spdif_inputs = self.hw.getDiscrete('/Mixer/Info/HasSPDIFInputs')
        print "device has SPDIF inputs: %d" % (self.has_spdif_inputs)

        # Customise the UI based on device options retrieved
        if (self.has_mic_inputs):
            # Mic input controls displace AES/EBU since no current device
            # has both.
            self.mix1_aes_group.setTitle("Mic inputs")
            # FIXME: when implmented, will mic channels just reuse the AES/EBU
            # dbus path?  If not we'll have to reset the respective values in
            # the control arrays (self.ChannelFaders etc).
        else:
            if (not(self.has_aesebu_inputs)):
                self.mix1_aes_group.setEnabled(False)
        if (not(self.has_spdif_inputs)):
            self.mix1_spdif_group.setEnabled(False)

        # Some controls must be disabled if the device is streaming
        if (self.is_streaming):
            print "Disabling controls which require inactive streaming"
            self.optical_in_mode.setEnabled(False)
            self.optical_out_mode.setEnabled(False)

        # Some channels aren't available at higher sampling rates
        if (self.sample_rate > 96000):
            print "Disabling controls not present above 96 kHz"
            self.mix1_adat_group.setEnabled(False)
            self.mix1_aes_group.setEnabled(False)
            self.mix1_spdif_group.setEnabled(False)
        if (self.sample_rate > 48000):
            print "Disabling controls not present above 48 kHz"
            self.mix1_adat58_group.setEnabled(False)

        # Now fetch the current values into the respective controls.  Don't
        # bother fetching controls which are disabled.
        for ctrl, info in self.ChannelFaders.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            vol = 128-self.hw.getDiscrete(info[0])
            print "%s channel fader is %d" % (info[0] , vol)
            ctrl.setValue(vol)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateFader)

        for ctrl, info in self.ChannelPans.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            pan = self.hw.getDiscrete(info[0])
            print "%s channel pan is %d" % (info[0] , pan)
            ctrl.setValue(pan)
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updatePan)

        for ctrl, info in self.BinarySwitches.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = self.hw.getDiscrete(info[0])
            print "%s switch is %d" % (info[0] , val)
            ctrl.setChecked(val)
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateBinarySwitch)

        for ctrl, info in self.MixDests.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            dest = self.hw.getDiscrete(info[0])
            print "%s mix destination is %d" % (info[0] , dest)
            ctrl.setCurrentItem(dest)
            QObject.connect(ctrl, SIGNAL('activated(int)'), self.updateMixDest)

        for name, ctrl in self.SelectorControls.iteritems():
            state = self.hw.getDiscrete(ctrl[0])
            print "%s state is %d" % (name , state)
            ctrl[1].setCurrentItem(state)    

        # FIXME: If optical mode is not ADAT, disable ADAT controls here. 
        # It can't be done earlier because we need the current values of the
        # ADAT channel controls in case the user goes ahead and enables the
        # ADAT optical mode.
