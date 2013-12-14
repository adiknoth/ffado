#
# presonus_firebox.py - Qt4/FFADO application for Presonus FIREBOX
# Copyright (C) 2013 Takashi Sakamoto <o-takashi@sakamocchi.jp>
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from PyQt4 import QtGui, QtCore
from PyQt4.QtCore import QObject, Qt, SIGNAL
from PyQt4.QtGui import QHBoxLayout, QVBoxLayout, QGridLayout
from PyQt4.QtGui import QWidget, QTabWidget, QGroupBox, QLabel, QSizePolicy, QToolButton, QSlider, QComboBox, QSpacerItem, QDial
from math import log10
from ffado.config import *

class Presonus_Firebox(QtGui.QWidget):
    #name/feature_id/+12dB_id
    inputs = [["Analog in 1/2",  5, 10],
              ["Analog in 3/4",  6, 11],
              ["Digital in 1/2", 7,  0],
              ["Stream in",      8,  0]]

    # name/id for mixer sorce selector
    streams = [["Stream in 1/2", 0],
               ["Stream in 3/4", 1],
               ["Stream in 5/6", 2],
               ["Stream in 7/8", 3]]

    # name/feature id
    outputs = [["Analog out 1/2",    1],
               ["Analog out 3/4",    2],
               ["Analog out 5/6",    3],
               ["Digital out 1/2",   0],
               ["Headphone out 1/2", 4]]

    # selector_id/sources
    out_src = [[1, ["Stream in 1/2", "Mixer out 1/2"]],
               [2, ["Stream in 3/4", "Mixer out 1/2"]],
               [3, ["Stream in 5/6", "Mixer out 1/2"]],
               [5, ["Stream in 7/8", "Mixer out 1/2"]],
               [4, ["Stream in 1/2", "Stream in 3/4",
                    "Stream in 5/6", "Stream in 7/8", "Mixer out 1/2"]]]

    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)

    def getDisplayTitle(self):
        return 'Firebox'

    def buildMixer(self):
        self.Selectors = {}
        self.MicBoosts = {}
        self.Volumes = {}
        self.Mutes = {}
        self.Balances = {}

        tabs = QTabWidget(self)
        tabs_layout = QHBoxLayout(self)
        tabs_layout.addWidget(tabs)

        self.addMixer(tabs)
        self.addOutput(tabs)

    def addMixer(self, tabs):
        tab_mixer = QWidget(tabs)
        tabs.addTab(tab_mixer, "Mixer")
        tab_mixer_layout = QGridLayout()
        tab_mixer.setLayout(tab_mixer_layout)

        for i in range(len(self.inputs)):
            col = 2 * i;
            label = QLabel(tab_mixer)
            tab_mixer_layout.addWidget(label, 0, col, 1, 2, Qt.AlignHCenter)

            label.setText(self.inputs[i][0])
            label.setAlignment(Qt.AlignCenter)
            label.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Minimum)

            link = self.getLink(tab_mixer)
            tab_mixer_layout.addWidget(link, 3, col, 1, 2, Qt.AlignHCenter)

            if self.inputs[i][2] > 0:
                for j in range(0, 2):
                    button = QToolButton(tab_mixer)
                    tab_mixer_layout.addWidget(button, 1, col + j, Qt.AlignHCenter)
                    button.setText('+12dB')
                    button.setCheckable(True)
                    button.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
                    path = "/Mixer/Feature_Volume_%d" % self.inputs[i][2]
                    self.MicBoosts[button] = [path, j]

            elif self.inputs[i][0].find('Stream') >= 0:
                cmb = QComboBox(tab_mixer)
                tab_mixer_layout.addWidget(cmb, 1, col, 1, 2, Qt.AlignHCenter)
                for i in range(len(self.streams)):
                    cmb.addItem(self.streams[i][0], self.streams[i][1])
                self.Selectors[cmb] = ["/Mixer/Selector_6"]

            l_sld = self.getSlider(tab_mixer)
            r_sld = self.getSlider(tab_mixer)
            tab_mixer_layout.addWidget(l_sld, 2, col, Qt.AlignHCenter)
            tab_mixer_layout.addWidget(r_sld, 2, col + 1, Qt.AlignHCenter)

            mute = self.getMute(tab_mixer)
            tab_mixer_layout.addWidget(mute, 4, col, 1, 2, Qt.AlignHCenter)

            path = "/Mixer/Feature_Volume_%d" % self.inputs[i][1]
            self.Volumes[l_sld] = [path, 1, r_sld, link, mute]
            self.Volumes[r_sld] = [path, 2, l_sld, link, mute]
            self.Mutes[mute] = [path, l_sld, r_sld]

            for j in range(0, 2):
                dial = self.getDial(tab_mixer)
                tab_mixer_layout.addWidget(dial, 5, col + j, Qt.AlignHCenter)
                if self.inputs[i][2] > 0:
                    path = "/Mixer/Feature_LRBalance_%d" % self.inputs[i][1]
                    self.Balances[dial] = [path, j + 1]
                # to keep width
                else:
                    dial.setDisabled(True)

    def addOutput(self, tabs):
        tab_out = QWidget(self)
        tabs.addTab(tab_out, "Output")
        tab_out_layout = QGridLayout()
        tab_out.setLayout(tab_out_layout)

        for i in range(len(self.outputs)):
            col = 2 * i

            label = QLabel(tab_out)
            tab_out_layout.addWidget(label, 0, col, 1, 2, Qt.AlignCenter)
            label.setText(self.outputs[i][0])

            cmb = QComboBox(tab_out)
            tab_out_layout.addWidget(cmb, 1, col, 1, 2, Qt.AlignHCenter)
            for j in range(len(self.out_src[i][1])):
                cmb.addItem(self.out_src[i][1][j])
            self.Selectors[cmb] = ["/Mixer/Selector_%d" % self.out_src[i][0]]

            if self.outputs[i][1] == 0:
                continue

            l_sld = self.getSlider(tab_out)
            r_sld = self.getSlider(tab_out)
            tab_out_layout.addWidget(l_sld, 2, col, Qt.AlignHCenter)
            tab_out_layout.addWidget(r_sld, 2, col + 1, Qt.AlignHCenter)

            link = self.getLink(tab_out)
            tab_out_layout.addWidget(link, 3, col, 1, 2, Qt.AlignHCenter)

            mute = self.getMute(tab_out)
            tab_out_layout.addWidget(mute, 4, col, 1, 2, Qt.AlignHCenter)

            path = "/Mixer/Feature_Volume_%d" % self.outputs[i][1]
            self.Volumes[l_sld] = [path, 1, r_sld, link, mute]
            self.Volumes[r_sld] = [path, 2, l_sld, link, mute]
            self.Mutes[mute] = [path, l_sld, r_sld]

    def getSlider(self, parent):
        sld = QSlider(parent)
        sld.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Minimum)
        sld.setMinimum(0)
        sld.setMaximum(99)
        sld.setPageStep(10)
        sld.setPageStep(10)
        sld.setMinimumHeight(50)
        sld.setTickInterval(25)
        sld.setTickPosition(QSlider.TicksBothSides)
        return sld

    def getDial(self, parent):
        dial = QDial(parent)
        dial.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Minimum)
        dial.setMinimumSize(50, 50)
        dial.setMaximumHeight(50)
        dial.setNotchTarget(25)
        dial.setNotchesVisible(True)
        dial.setMinimum(0)
        dial.setMaximum(99)
        dial.setPageStep(10)
        dial.setPageStep(10)
        return dial

    def getLink(self, parent):
        link = QToolButton(parent)
        link.setText("link")
        link.setCheckable(True)
        link.setMinimumWidth(100)
        link.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        return link

    def getMute(self, parent):
        mute = QToolButton(parent)
        mute.setText("mute")
        mute.setCheckable(True)
        mute.setMinimumWidth(100)
        mute.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        return mute

    def initValues(self):
        for elm, params in self.Selectors.items():
            path = params[0]
            state = self.hw.getDiscrete(path)
            elm.setCurrentIndex(state)
            QObject.connect(elm, SIGNAL('activated(int)'), self.updateSelector)

        for elm, params in self.MicBoosts.items():
            path = params[0]
            idx  = params[1]
            value = self.hw.getContignuous(path, idx)
            elm.setChecked(not value == 0)
            QObject.connect(elm, SIGNAL('clicked(bool)'), self.updateMicBoost)

        for elm, params in self.Volumes.items():
            path = params[0]
            idx  = params[1]
            db   = self.hw.getContignuous(path, idx)
            vol  = self.db2vol(db)
            elm.setValue(vol)
            QObject.connect(elm, SIGNAL('valueChanged(int)'), self.updateVolume)

            if idx == 0:
                continue

            p_idx = 0
            pair = params[2]
            link = params[3]
            mute = params[4]

            p_db = self.hw.getContignuous(path, p_idx)
            if db == p_db:
                link.setChecked(True)

        for elm, params in self.Mutes.items():
            path  = params[0]
            l_elm = params[1]
            r_elm = params[2]

            l_db  = self.hw.getContignuous(path, 1)
            r_db  = self.hw.getContignuous(path, 2)
            l_vol = self.db2vol(l_db)
            r_vol = self.db2vol(r_db)

            if l_vol == r_vol and l_vol == 0:
                elm.setChecked(True)
                l_elm.setDisabled(True)
                r_elm.setDisabled(True)
            QObject.connect(elm, SIGNAL('clicked(bool)'), self.updateMute)

        for elm, params in self.Balances.items():
            path = params[0]
            idx  = params[1]
            pan = self.hw.getContignuous(path, idx)
            val = self.pan2val(pan)
            elm.setValue(val)
            QObject.connect(elm, SIGNAL('valueChanged(int)'), self.updateBalance)

    # helper functions
    def vol2db(self, vol):
        return (log10(vol + 1) - 2) * 16384
    def db2vol(self, db):
        return pow(10, db / 16384 + 2) - 1

    #       Right - Center - Left
    # 0x8000 - 0x0000 - 0x0001 - 0x7FFE
    #        ..., -1, 0, +1, ...
    def val2pan(self, val):
        return (val - 50) * 0x7ffe / -50
    def pan2val(self, pan):
        return -(pan/ 0x7ffe) * 50 + 50

    def updateSelector(self, state):
        sender = self.sender()
        path   = self.Selectors[sender][0]
        self.hw.setDiscrete(path, state)

    def updateVolume(self, vol):
        sender = self.sender()
        path   = self.Volumes[sender][0]
        idx    = self.Volumes[sender][1]
        pair   = self.Volumes[sender][2]
        link   = self.Volumes[sender][3]
        mute   = self.Volumes[sender][4]
        db = self.vol2db(vol)
        self.hw.setContignuous(path, db, idx)

        if idx == 1:
            p_idx = 2
        else:
            p_idx = 1
        if link.isChecked():
            self.hw.setContignuous(path, db, p_idx)
            pair.setValue(vol)

    def updateMicBoost(self, state):
        sender = self.sender()
        path   = self.MicBoosts[sender][0]
        idx    = self.MicBoosts[sender][1]
        if state:
            value = 0x7ffe
        else:
            value = 0x0000
        self.hw.setContignuous(path, value, idx)

    def updateBalance(self, val):
        sender = self.sender()
        path   = self.Balances[sender][0]
        idx    = self.Balances[sender][1]
        pan    = self.val2pan(val)
        self.hw.setContignuous(path, pan, idx)

    def updateMute(self, state):
        sender = self.sender()
        path   = self.Mutes[sender][0]
        l_elm  = self.Mutes[sender][1]
        r_elm  = self.Mutes[sender][2]

        if state:
            # mute emulation
            db = self.vol2db(10)
            l_elm.setValue(0)
            l_elm.setDisabled(True)
            r_elm.setValue(0)
            r_elm.setDisabled(True)
        else:
            db = self.vol2db(99)
            l_elm.setDisabled(False)
            l_elm.setValue(99)
            r_elm.setDisabled(False)
            r_elm.setValue(99)

        self.hw.setContignuous(path, db, 1)
        self.hw.setContignuous(path, db, 2)
