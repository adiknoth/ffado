#
# Copyright (C) 2009, 2011 by Jonathan Woithe
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

from PyQt4 import QtGui

from PyQt4.QtCore import SIGNAL, SLOT, QObject, Qt, QTimer
from PyQt4.QtGui import QWidget, QApplication
from ffado.config import *

from ffado.widgets.matrixmixer import MatrixMixer

import logging
log = logging.getLogger('rme')

# Model defines.  These must agree with what is used in rme_avdevice.h.
RME_MODEL_NONE      = 0x0000
RME_MODEL_FF800     = 0x0001
RME_MODEL_FF400     = 0x0002

class Rme(QWidget):
    def __init__(self,parent = None):
        QWidget.__init__(self,parent)
        uicLoad("ffado/mixer/rme", self)

        self.init()

    def init(self):

        self.PhantomSwitches={
            self.phantom_0: ['/Control/Phantom', 0],
            self.phantom_1: ['/Control/Phantom', 1],
            self.phantom_2: ['/Control/Phantom', 2],
            self.phantom_3: ['/Control/Phantom', 3],
        }

        self.Switches={
            self.ff400_chan3_opt_instr: ['/Control/Chan3_opt_instr'],
            self.ff400_chan3_opt_pad:   ['/Control/Chan3_opt_pad'],
            self.ff400_chan4_opt_instr: ['/Control/Chan4_opt_instr'],
            self.ff400_chan4_opt_pad:   ['/Control/Chan4_opt_pad'],

            self.spdif_output_optical:  ['/Control/SPDIF_output_optical', 0],
            self.spdif_output_emphasis: ['/Control/SPDIF_output_emphasis', 0],
            self.spdif_output_pro:      ['/Control/SPDIF_output_pro', 0],
            self.spdif_output_nonaudio: ['/Control/SPDIF_output_nonaudio', 0],
        }

        self.Radiobuttons={
            self.level_in_lo_gain: ['/Control/Input_level', 1],
            self.level_in_p4dBu:   ['/Control/Input_level', 2],
            self.level_in_m10dBV:  ['/Control/Input_level', 3],

            self.level_out_hi_gain: ['/Control/Output_level', 1],
            self.level_out_p4dBu:   ['/Control/Output_level', 2],
            self.level_out_m10dBV:  ['/Control/Output_level', 3],

            self.spdif_input_coax:    ['/Control/SPDIF_input_mode', 0],
            self.spdif_input_optical: ['/Control/SPDIF_input_mode', 1],

            self.phones_hi_gain: ['/Control/Phones_level', 1],
            self.phones_p4dBu:   ['/Control/Phones_level', 2],
            self.phones_m10dBV:  ['/Control/Phones_level', 3],

            self.clock_mode_autosync: ['/Control/Clock_mode', 1],
            self.clock_mode_master: ['/Control/Clock_mode', 0],

            self.sync_ref_wordclk: ['/Control/Sync_ref', 0],
            self.sync_ref_adat1: ['/Control/Sync_ref', 1],
            self.sync_ref_adat2: ['/Control/Sync_ref', 2],
            self.sync_ref_spdif: ['/Control/Sync_ref', 3],
            self.sync_ref_tco: ['/Control/Sync_ref', 4],
        }

        self.Checkboxes={
            self.ch1_instr_fuzz: ['/Control/Chan1_instr_opts', 0x04],
            self.ch1_instr_limiter: ['/Control/Chan1_instr_opts', 0x08],
            self.ch1_instr_filter: ['/Control/Chan1_instr_opts', 0x02],
        }

        self.Gains={
            self.gain_mic1: ['/Control/Gains', 0],
            self.gain_mic2: ['/Control/Gains', 1],
            self.gain_input3: ['/Control/Gains', 2],
            self.gain_input4: ['/Control/Gains', 3],
        }

        self.Combos={
            self.ff800_ch1_src: ['/Control/Chan1_source'],
            self.ff800_ch7_src: ['/Control/Chan7_source'],
            self.ff800_ch8_src: ['/Control/Chan8_source'],
        }

        # Other mixer variables
        self.is_streaming = 0
        self.sample_rate = 0
        self.model = 0
        self.tco_present = 0

    # Public slot: update phantom power hardware switches
    def updatePhantomSwitch(self, a0):
        sender = self.sender()
        # Value is the phantom switch value, with a corresponding enable
        # bit in the high 16 bit word
        val = (a0 << self.PhantomSwitches[sender][1]) | (0x00010000 << self.PhantomSwitches[sender][1])
        log.debug("phantom switch %d set to %d" % (self.PhantomSwitches[sender][1], a0))
        self.hw.setDiscrete(self.PhantomSwitches[sender][0], val)

    # Public slot: update generic switches
    def updateSwitch(self, a0):
        sender = self.sender()
        log.debug("switch %s set to %d" % (self.Switches[sender][0], a0))
        self.hw.setDiscrete(self.Switches[sender][0], a0)

    # Public slot: update generic radiobuttons
    def updateRadiobutton(self, a0):
        sender = self.sender()
        if (a0 != 0):
            # Only change the control state on a button being "checked"
            log.debug("radiobutton group %s set to %d" % (self.Radiobuttons[sender][0], self.Radiobuttons[sender][1]))
            self.hw.setDiscrete(self.Radiobuttons[sender][0], self.Radiobuttons[sender][1])

    def updateCheckboxes(self, a0):
        sender = self.sender()
        val = self.hw.getDiscrete(self.Checkboxes[sender][0]);
        if (a0 != 0):
            val = val | self.Checkboxes[sender][1]
        else:
            val = val & ~self.Checkboxes[sender][1]
        log.debug("checkbox group %s set to %d" % (self.Checkboxes[sender][0], val));
        self.hw.setDiscrete(self.Checkboxes[sender][0], val)

    # Public slot: update gains
    def updateGain(self, a0):
        sender = self.sender()
        log.debug("gain %s[%d] set to %d" % (self.Gains[sender][0], self.Gains[sender][1], a0))
        self.hw.setMatrixMixerValue(self.Gains[sender][0], 0, self.Gains[sender][1], a0)

    def updateBandwidthLimit(self, a0):
        # Account for the "No ADAT-2" item which will not be present on
        # a FF400.
        if (self.model==RME_MODEL_FF400 and a0>0):
            a0 = a0 + 1
        # log.debug("limit update: %d" % (a0));
        self.hw.setDiscrete('/Control/Bandwidth_limit', a0);

    def updateCombo(self, a0):
        sender = self.sender()
        log.debug("combo %s set to %d" % (self.Combos[sender][0], a0))
        self.hw.setDiscrete(self.Combos[sender][0], a0)

    def updateStreamingState(self):
        ss = self.streamingstatus.selected()
        ss_txt = self.streamingstatus.getEnumLabel(ss)
        if ss_txt != 'Idle':
            self.is_streaming = True
        else:
            self.is_streaming = False
        if (self.last_streaming_state != self.is_streaming):
            self.bandwidth_limit.setEnabled(not(self.is_streaming));
        self.last_streaming_state = self.is_streaming

    def status_update(self):
        # log.debug("timer event")
        self.updateStreamingState()
        clk_mode = ['Master', 'Slave']
        src_str = ['None', 'ADAT 1', 'ADAT 2', 'SPDIF', 'Wordclock', 'TCO']
        sync_stat = ['No lock', 'Locked', 'Synced']
        sysclock_mode = self.hw.getDiscrete('/Control/sysclock_mode')
        sysclock_freq = self.hw.getDiscrete('/Control/sysclock_freq')
        autosync_freq = self.hw.getDiscrete('/Control/autosync_freq')
        autosync_src = self.hw.getDiscrete('/Control/autosync_src')
        sync_status = self.hw.getDiscrete('/Control/sync_status')
        spdif_freq = self.hw.getDiscrete('/Control/spdif_freq')
        self.sysclock_freq.setText("%d Hz" % (sysclock_freq))
        self.sysclock_mode.setText(clk_mode[sysclock_mode])
        self.autosync_freq.setText("%d Hz" % (autosync_freq))
        self.autosync_src.setText(src_str[autosync_src])
        self.sync_check_adat1_status.setText(sync_stat[sync_status & 0x03])
        self.sync_check_adat2_status.setText(sync_stat[(sync_status >> 2) & 0x03])
        self.sync_check_spdif_status.setText(sync_stat[(sync_status >> 4) & 0x03])
        self.sync_check_wclk_status.setText(sync_stat[(sync_status >> 6) & 0x03])
        self.sync_check_tco_status.setText(sync_stat[(sync_status >> 8) & 0x03])
        self.spdif_freq.setText("%d Hz" % (spdif_freq))

    # Hide and disable a control
    def disable_hide(self,widget):
        widget.hide()
        widget.setEnabled(False)

    def initValues(self):

        # print self.hw.servername
        # print self.hw.basepath
        self.inputmatrix = MatrixMixer(self.hw.servername, self.hw.basepath+"/Mixer/InputFaders", self, 0x8000, self.hw.basepath+"/Mixer/InputMutes", self.hw.basepath+"/Mixer/InputInverts")
        layout = QtGui.QVBoxLayout()
        scrollarea = QtGui.QScrollArea()
        scrollarea.setWidgetResizable(True)
        scrollarea.setWidget(self.inputmatrix)
        layout.addWidget(scrollarea)
        self.mixer.setLayout(layout)

        self.playbackmatrix = MatrixMixer(self.hw.servername, self.hw.basepath+"/Mixer/PlaybackFaders", self, 0x8000, self.hw.basepath+"/Mixer/PlaybackMutes", self.hw.basepath+"/Mixer/PlaybackInverts")
        layout = QtGui.QVBoxLayout()
        scrollarea = QtGui.QScrollArea()
        scrollarea.setWidgetResizable(True)
        scrollarea.setWidget(self.playbackmatrix)
        layout.addWidget(scrollarea)
        self.playbackmixer.setLayout(layout)

        self.outputmatrix = MatrixMixer(self.hw.servername, self.hw.basepath+"/Mixer/OutputFaders", self, 0x8000, self.hw.basepath+"/Mixer/OutputMutes")
        layout = QtGui.QVBoxLayout()
        scrollarea = QtGui.QScrollArea()
        scrollarea.setWidgetResizable(True)
        scrollarea.setWidget(self.outputmatrix)
        layout.addWidget(scrollarea)
        self.outputmixer.setLayout(layout)

        self.is_streaming = False
        self.last_streaming_state = False

        # Retrieve other device settings as needed and customise the UI
        # based on these options.
        self.model = self.hw.getDiscrete('/Control/Model')
        log.debug("device model identifier: %d" % (self.model))
        self.tco_present = self.hw.getDiscrete('/Control/TCO_present')
        log.debug("device has TCO: %d" % (self.tco_present))
        #self.sample_rate = self.hw.getDiscrete('/Mixer/Info/SampleRate')
        #log.debug("device sample rate: %d" % (self.sample_rate))

        # The Fireface-400 only has 2 phantom-capable channels
        if (self.model == RME_MODEL_FF400):
            self.disable_hide(self.phantom_2)
            self.disable_hide(self.phantom_3)
        else:
            self.phantom_0.setText("Mic 7")
            self.phantom_1.setText("Mic 8")
            self.phantom_2.setText("Mic 9")
            self.phantom_3.setText("Mic 10")

        # Instrument options, input jack selection controls and an ADAT2
        # input are applicable only to the FF800
        if (self.model != RME_MODEL_FF800):
            self.instrument_options_group.setEnabled(False)
            self.input_plug_select_group.setEnabled(False)
            self.sync_ref_adat2.setEnabled(False)
            self.sync_check_adat2_label.setEnabled(False)
            self.sync_check_adat2_status.setEnabled(False)

        for ctrl, info in self.Combos.iteritems():
            if (not(ctrl.isEnabled())):
                continue;
            val = self.hw.getDiscrete(info[0])
            log.debug("combo %s is %d" % (info[0], val));
            ctrl.setCurrentIndex(val);
            QObject.connect(ctrl, SIGNAL('currentIndexChanged(int)'), self.updateCombo)

        if (not(self.tco_present)):
            self.sync_check_tco_label.setEnabled(False)
            self.sync_check_tco_status.setEnabled(False)
            self.sync_ref_tco.setEnabled(False)

        # Only the FF400 has specific channel 3/4 options, input gain
        # controls and switchable phones level
        if (self.model != RME_MODEL_FF400):
            self.disable_hide(self.input_gains_group)
            self.disable_hide(self.channel_3_4_options_group)
            self.phones_level_group.setEnabled(False)

        # Add the "No ADAT-2" item to the bandwidth limit control if the
        # device is not a FF400.  Set the control to reflect the current
        # device setting and connect an update signal.
        if (self.model != RME_MODEL_FF400):
            self.bandwidth_limit.insertItem(1, "No ADAT-2")
        val = self.hw.getDiscrete('/Control/Bandwidth_limit')
        if (self.model==RME_MODEL_FF400 and val>1):
            val = val - 1
        self.bandwidth_limit.setCurrentIndex(val);
        QObject.connect(self.bandwidth_limit, SIGNAL('currentIndexChanged(int)'), self.updateBandwidthLimit)

        # Get current hardware values and connect GUI element signals to 
        # their respective slots
        for ctrl, info in self.PhantomSwitches.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = (self.hw.getDiscrete(info[0]) >> info[1]) & 0x01
            log.debug("phantom switch %d is %d" % (info[1], val))
            if val:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updatePhantomSwitch)

        for ctrl, info in self.Switches.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = self.hw.getDiscrete(info[0])
            log.debug("switch %s is %d" % (info[0], val))
            if val:
                ctrl.setChecked(True)
            else:
                ctrl.setChecked(False)
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateSwitch)

        for ctrl, info in self.Radiobuttons.iteritems():
            if (not(ctrl.isEnabled())):
                continue;
            # This is a touch wasteful since it means we retrieve the control
            # value once per radio button rather than once per radio button
            # group.  In time we might introduce radiobutton groupings in the
            # self.* datastructures to avoid this, but for the moment this is
            # easy and it works.
            val = self.hw.getDiscrete(info[0])
            if (val == info[1]):
                val = 1
            else:
                val = 0
            ctrl.setChecked(val)
            log.debug("Radiobutton %s[%d] is %d" % (info[0], info[1], val))
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateRadiobutton)

        for ctrl, info in self.Checkboxes.iteritems():
            if (not(ctrl.isEnabled())):
                continue;
            # This is a touch wasteful since it means we retrieve the control
            # value once per checkbox button rather than once per checkbox
            # group.  In time we might introduce checkbox groupings in the
            # self.* datastructures to avoid this, but for the moment this is
            # easy and it works.
            val = self.hw.getDiscrete(info[0])
            if (val & info[1]):
                val = 1
            else:
                val = 0
            ctrl.setChecked(val)
            log.debug("Checkbox %s[%d] is %d" % (info[0], info[1], val))
            QObject.connect(ctrl, SIGNAL('toggled(bool)'), self.updateCheckboxes)

        for ctrl, info in self.Gains.iteritems():
            if (not(ctrl.isEnabled())):
                continue
            val = self.hw.getMatrixMixerValue(info[0], 0, info[1])
            log.debug("gain %s[%d] is %d" % (info[0], info[1], val))
            ctrl.setValue(val);
            QObject.connect(ctrl, SIGNAL('valueChanged(int)'), self.updateGain)

        self.updateStreamingState()
        #log.debug("device streaming flag: %d" % (self.is_streaming))

        self.update_timer = QTimer(self)
        QObject.connect(self.update_timer, SIGNAL('timeout()'), self.status_update)
        self.update_timer.start(1000)

# vim: et
