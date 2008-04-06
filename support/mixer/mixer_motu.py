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

    def setChannelFader(self,a0,a1):
            name=a0
            vol = 128-a1
            print "setting %s channel fader to %d" % (name, vol)
            self.hw.setDiscrete(self.ChannelFaders[name][0], vol)

    def setChannelPan(self,a0,a1):
            name=a0
            pan = a1
            print "setting %s channel pan to %d" % (name, pan)
            self.hw.setDiscrete(self.ChannelPans[name][0], pan)

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

            for name, ctrl in self.SelectorControls.iteritems():
                state = self.hw.getDiscrete(ctrl[0])
                print "%s state is %d" % (name , state)
                ctrl[1].setCurrentItem(state)    
