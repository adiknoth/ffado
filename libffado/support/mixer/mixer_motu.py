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

    # public slots: mix1 faders
    def set_mix1ana1_fader(self,a0):
        self.setChannelFader('mix1ana1', a0)
    def set_mix1ana2_fader(self,a0):
        self.setChannelFader('mix1ana2', a0)
    def set_mix1ana3_fader(self,a0):
        self.setChannelFader('mix1ana3', a0)
    def set_mix1ana4_fader(self,a0):
        self.setChannelFader('mix1ana4', a0)
    def set_mix1ana5_fader(self,a0):
        self.setChannelFader('mix1ana5', a0)
    def set_mix1ana6_fader(self,a0):
        self.setChannelFader('mix1ana6', a0)
    def set_mix1ana7_fader(self,a0):
        self.setChannelFader('mix1ana7', a0)
    def set_mix1ana8_fader(self,a0):
        self.setChannelFader('mix1ana8', a0)

    # public slots: mix1 pan
    def set_mix1ana1_pan(self,a0):
        self.setChannelPan('mix1ana1', a0)
    def set_mix1ana2_pan(self,a0):
        self.setChannelPan('mix1ana2', a0)
    def set_mix1ana3_pan(self,a0):
        self.setChannelPan('mix1ana3', a0)
    def set_mix1ana4_pan(self,a0):
        self.setChannelPan('mix1ana4', a0)
    def set_mix1ana5_pan(self,a0):
        self.setChannelPan('mix1ana5', a0)
    def set_mix1ana6_pan(self,a0):
        self.setChannelPan('mix1ana6', a0)
    def set_mix1ana7_pan(self,a0):
        self.setChannelPan('mix1ana7', a0)
    def set_mix1ana8_pan(self,a0):
        self.setChannelPan('mix1ana8', a0)

    # public slots: mix1 mute
    def set_mix1ana1_mute(self,a0):
        self.setChannelMute('mix1ana1', a0)
    def set_mix1ana2_mute(self,a0):
        self.setChannelMute('mix1ana2', a0)
    def set_mix1ana3_mute(self,a0):
        self.setChannelMute('mix1ana3', a0)
    def set_mix1ana4_mute(self,a0):
        self.setChannelMute('mix1ana4', a0)
    def set_mix1ana5_mute(self,a0):
        self.setChannelMute('mix1ana5', a0)
    def set_mix1ana6_mute(self,a0):
        self.setChannelMute('mix1ana6', a0)
    def set_mix1ana7_mute(self,a0):
        self.setChannelMute('mix1ana7', a0)
    def set_mix1ana8_mute(self,a0):
        self.setChannelMute('mix1ana8', a0)

    # public slots: mix1 solo
    def set_mix1ana1_solo(self,a0):
        self.setChannelSolo('mix1ana1', a0)
    def set_mix1ana2_solo(self,a0):
        self.setChannelSolo('mix1ana2', a0)
    def set_mix1ana3_solo(self,a0):
        self.setChannelSolo('mix1ana3', a0)
    def set_mix1ana4_solo(self,a0):
        self.setChannelSolo('mix1ana4', a0)
    def set_mix1ana5_solo(self,a0):
        self.setChannelSolo('mix1ana5', a0)
    def set_mix1ana6_solo(self,a0):
        self.setChannelSolo('mix1ana6', a0)
    def set_mix1ana7_solo(self,a0):
        self.setChannelSolo('mix1ana7', a0)
    def set_mix1ana8_solo(self,a0):
        self.setChannelSolo('mix1ana8', a0)

    # public slots: mix output controls
    def set_mix1_fader(self,a0):
        self.setChannelFader('mix1', a0)
    def set_mix1_mute(self,a0):
        self.setChannelMute('mix1', a0)
    def set_mix1_dest(self,a0):
        self.setMixDest('mix1', a0)

    def setChannelFader(self,a0,a1):
            name=a0
            vol = 128-a1
            print "setting %s channel/mix fader to %d" % (name, vol)
            self.hw.setDiscrete(self.ChannelFaders[name][0], vol)

    def setChannelPan(self,a0,a1):
            name=a0
            pan = a1
            print "setting %s channel pan to %d" % (name, pan)
            self.hw.setDiscrete(self.ChannelPans[name][0], pan)

    def setChannelMute(self,a0,a1):
            name=a0
            mute=a1
            print "setting %s channel/mix mute to %d" % (name, mute)
            self.hw.setDiscrete(self.ChannelMutes[name][0], mute)

    def setChannelSolo(self,a0,a1):
            name=a0
            solo=a1
            print "setting %s channel solo to %d" % (name, solo)
            self.hw.setDiscrete(self.ChannelSolos[name][0], solo)

    def setMixDest(self,a0,a1):
            name=a0
            dest=a1
            print "setting %s mix destination to %d" % (name, dest)
            self.hw.setDiscrete(self.MixDests[name][0], dest)

    def setSelector(self,a0,a1):
            name=a0
            state = a1
            print "setting %s state to %d" % (name, state)
            self.hw.setDiscrete(self.SelectorControls[name][0], state)

    def init(self):
            print "Init MOTU mixer window"

            self.ChannelFaders={
                'mix1ana1':    ['/Mixer/Mix1/Ana1_fader', self.mix1ana1_fader], 
                'mix1ana2':    ['/Mixer/Mix1/Ana2_fader', self.mix1ana2_fader], 
                'mix1ana3':    ['/Mixer/Mix1/Ana3_fader', self.mix1ana3_fader], 
                'mix1ana4':    ['/Mixer/Mix1/Ana4_fader', self.mix1ana4_fader], 
                'mix1ana5':    ['/Mixer/Mix1/Ana5_fader', self.mix1ana5_fader], 
                'mix1ana6':    ['/Mixer/Mix1/Ana6_fader', self.mix1ana6_fader], 
                'mix1ana7':    ['/Mixer/Mix1/Ana7_fader', self.mix1ana7_fader], 
                'mix1ana8':    ['/Mixer/Mix1/Ana8_fader', self.mix1ana8_fader], 
                'mix1':        ['/Mixer/Mix1/Mix_fader',  self.mix1_fader],
                }

            self.ChannelPans={
                'mix1ana1':    ['/Mixer/Mix1/Ana1_pan', self.mix1ana1_pan],
                'mix1ana2':    ['/Mixer/Mix1/Ana2_pan', self.mix1ana2_pan],
                'mix1ana3':    ['/Mixer/Mix1/Ana3_pan', self.mix1ana3_pan],
                'mix1ana4':    ['/Mixer/Mix1/Ana4_pan', self.mix1ana4_pan],
                'mix1ana5':    ['/Mixer/Mix1/Ana5_pan', self.mix1ana5_pan],
                'mix1ana6':    ['/Mixer/Mix1/Ana6_pan', self.mix1ana6_pan],
                'mix1ana7':    ['/Mixer/Mix1/Ana7_pan', self.mix1ana7_pan],
                'mix1ana8':    ['/Mixer/Mix1/Ana8_pan', self.mix1ana8_pan],
            }

            self.ChannelMutes={
                'mix1ana1':    ['/Mixer/Mix1/Ana1_mute', self.mix1ana1_mute],
                'mix1ana2':    ['/Mixer/Mix1/Ana2_mute', self.mix1ana2_mute],
                'mix1ana3':    ['/Mixer/Mix1/Ana3_mute', self.mix1ana3_mute],
                'mix1ana4':    ['/Mixer/Mix1/Ana4_mute', self.mix1ana4_mute],
                'mix1ana5':    ['/Mixer/Mix1/Ana5_mute', self.mix1ana5_mute],
                'mix1ana6':    ['/Mixer/Mix1/Ana6_mute', self.mix1ana6_mute],
                'mix1ana7':    ['/Mixer/Mix1/Ana7_mute', self.mix1ana7_mute],
                'mix1ana8':    ['/Mixer/Mix1/Ana8_mute', self.mix1ana8_mute],
                'mix1':        ['/Mixer/Mix1/Mix_mute',  self.mix1_mute],
            }

            self.ChannelSolos={
                'mix1ana1':    ['/Mixer/Mix1/Ana1_solo', self.mix1ana1_solo],
                'mix1ana2':    ['/Mixer/Mix1/Ana2_solo', self.mix1ana2_solo],
                'mix1ana3':    ['/Mixer/Mix1/Ana3_solo', self.mix1ana3_solo],
                'mix1ana4':    ['/Mixer/Mix1/Ana4_solo', self.mix1ana4_solo],
                'mix1ana5':    ['/Mixer/Mix1/Ana5_solo', self.mix1ana5_solo],
                'mix1ana6':    ['/Mixer/Mix1/Ana6_solo', self.mix1ana6_solo],
                'mix1ana7':    ['/Mixer/Mix1/Ana7_solo', self.mix1ana7_solo],
                'mix1ana8':    ['/Mixer/Mix1/Ana8_solo', self.mix1ana8_solo],
            }

            self.MixDests={
                'mix1':        ['/Mixer/Mix1/Mix_dest',  self.mix1_dest],
            }

            self.SelectorControls={

            }

    def initValues(self):
            for name, ctrl in self.ChannelFaders.iteritems():
                vol = 128-self.hw.getDiscrete(ctrl[0])
                print "%s channel fader is %d" % (name , vol)
                ctrl[1].setValue(vol)
            for name, ctrl in self.ChannelPans.iteritems():
                pan = self.hw.getDiscrete(ctrl[0])
                print "%s channel pan is %d" % (name , pan)
                ctrl[1].setValue(pan)
            for name, ctrl in self.ChannelMutes.iteritems():
                mute = self.hw.getDiscrete(ctrl[0])
                print "%s channel mute is %d" % (name , mute)
                ctrl[1].setChecked(mute)
            for name, ctrl in self.ChannelSolos.iteritems():
                solo = self.hw.getDiscrete(ctrl[0])
                print "%s channel solo is %d" % (name , solo)
                ctrl[1].setChecked(solo)
            for name, ctrl in self.MixDests.iteritems():
                dest = self.hw.getDiscrete(ctrl[0])
                print "%s mix destination is %d" % (name , dest)
                ctrl[1].setCurrentItem(dest)

            for name, ctrl in self.SelectorControls.iteritems():
                state = self.hw.getDiscrete(ctrl[0])
                print "%s state is %d" % (name , state)
                ctrl[1].setCurrentItem(state)    
