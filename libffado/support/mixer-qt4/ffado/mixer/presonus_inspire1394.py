#
# presonus_inspire1394.py - Qt4/FFADO application for Presonus Inspire1394
# Copyright (C) 2014 Takashi Sakamoto <o-takashi@sakamocchi.jp>
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
from PyQt4.QtGui import QGroupBox, QLabel, QSizePolicy, QSlider, QDial, QComboBox, QToolButton
from math import log10
from ffado.config import *

class Presonus_Inspire1394(QtGui.QWidget):
    # feature_id/name
    inputs = [[1, "Analog in 1/2"],
              [2, "Analog in 3/4"]]

    # feature_id/name
    mixer_src = [[3, "Analog in 1/2"],
                 [4, "Analog in 3/4"],
                 [5, "Stream in 1/2"]]

    # feature id/name
    outputs = [[6, "Analog out 1/2"],
               [7, "HP out 1/2"]]

    # selector_id/sources
    out_src = [1, ["Mixer out 1/2", "Stream in 1/2"]]

    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)

    def getDisplayTitle(self):
        return 'Inspire1394'

    def buildMixer(self):
        self.Selectors = {}
        self.Volumes = {}
        self.Preamps = {}
        self.Pannings = {}
        self.Mutes = {}

        plain_layout = QHBoxLayout(self)

        left = QGroupBox(self)
        plain_layout.addWidget(left)
        self.addAnalogInputs(left)

        center = QGroupBox(self)
        plain_layout.addWidget(center)
        self.addInternalMixer(center)

        right = QGroupBox(self)
        plain_layout.addWidget(right)
        self.addAnalogOutputs(right)

    def addAnalogInputs(self, box):
        box_layout = QVBoxLayout()
        box.setLayout(box_layout)
        box.setTitle("Analog Inputs")

        grid_layout = QGridLayout()
        box_layout.addLayout(grid_layout)
    
        self.addVolumes(self.inputs, 0, box, grid_layout)

    def addAnalogOutputs(self, box):
        box_layout = QVBoxLayout()
        box.setLayout(box_layout)
        box.setTitle("Analog Outputs")

        grid_layout = QGridLayout()
        box_layout.addLayout(grid_layout)

        self.addVolumes(self.outputs, 2, box, grid_layout)

    def addInternalMixer(self, box):
        box_layout = QGridLayout()
        box.setLayout(box_layout)
        box.setTitle("Hardware Mixer")

        self.addVolumes(self.mixer_src, 1, box, box_layout)

    def addVolumes(self, elms, kind, parent, layout):
        def addPreampParam(label, ch, path, layout):
            button = self.getThinButton(parent, label)
            layout.addWidget(button)
            self.Preamps[button] = ["/Preamp/%s" % path, ch]
            return

        for col in range(len(elms)):
            label = QLabel(parent)
            label.setText(elms[col][1])
            layout.addWidget(label, 0, col * 2, 1, 2, Qt.AlignHCenter | Qt.AlignTop)

            if kind == 0:
                if col == 0:
                    for ch in range(2):
                        box_layout = QVBoxLayout()
                        layout.addLayout(box_layout, 1, col * 2 + ch, Qt.AlignHCenter | Qt.AlignBottom)
                        if col == 0:
                            addPreampParam("+48V", ch + 1, "PhantomPower", box_layout)
                            addPreampParam("Boost", ch + 1, "MicBoost", box_layout)
                            addPreampParam("Limit", ch + 1, "MicLimit", box_layout)
                else:
                    box_layout = QVBoxLayout()
                    addPreampParam("Phono", 0, "PhonoSwitch", box_layout)
                    layout.addLayout(box_layout, 1, col * 2, 1, 2, Qt.AlignHCenter | Qt.AlignBottom)
            elif kind == 1:
                l_dial = self.getDial(parent)
                r_dial = self.getDial(parent)

                layout.addWidget(l_dial, 1, col * 2, Qt.AlignHCenter | Qt.AlignBottom)
                layout.addWidget(r_dial, 1, col * 2 + 1, Qt.AlignHCenter | Qt.AlignBottom)

                path = "/Mixer/Feature_LRBalance_%d" % elms[col][0]
                self.Pannings[l_dial] = [path, 1]
                self.Pannings[r_dial] = [path, 2]

                if col == 2:
                    l_dial.setDisabled(True)
                    r_dial.setDisabled(True)

            elif col == 0:
                cmb = QComboBox(parent)
                layout.addWidget(cmb, 1, col * 2, 1, 4, Qt.AlignHCenter | Qt.AlignBottom)
                for i in range(len(self.out_src[1])):
                    cmb.addItem(self.out_src[1][i], i)
                    self.Selectors[cmb] = ["/Mixer/Selector_%d" % self.out_src[0]]

            l_sld = self.getSlider(parent)
            r_sld = self.getSlider(parent)
            layout.addWidget(l_sld, 2, col * 2, Qt.AlignHCenter)
            layout.addWidget(r_sld, 2, col * 2 + 1, Qt.AlignHCenter)

            l_mute = self.getThinButton(parent, "Mute")
            r_mute = self.getThinButton(parent, "Mute")
            layout.addWidget(l_mute, 3, col * 2, Qt.AlignHCenter)
            layout.addWidget(r_mute, 3, col * 2 + 1, Qt.AlignHCenter)

            link = self.getWideButton(parent, "Link")
            layout.addWidget(link, 4, col * 2, 1, 2, Qt.AlignHCenter)

            path = "/Mixer/Feature_Volume_%d" % elms[col][0]
            self.Volumes[l_sld] = [path, 1, r_sld, l_mute, link]
            self.Volumes[r_sld] = [path, 2, l_sld, r_mute, link]

            self.Mutes[l_mute] = [r_mute, l_sld]
            self.Mutes[r_mute] = [l_mute, r_sld]

    # widget helper functions
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
        dial.setNotchesVisible(True)
        dial.setNotchTarget(25.0)
        dial.setMaximumHeight(40)
        return dial;
 
    def getThinButton(self, parent, text):
        button = QToolButton(parent)
        button.setText(text)
        button.setCheckable(True)
        button.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        return button

    def getWideButton(self, parent, label):
        button = QToolButton(parent)
        button.setText(label)
        button.setCheckable(True)
        button.setMinimumWidth(100)
        button.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        return button

    def initValues(self):
        for ctl, params in self.Selectors.items():
           path = params[0]
           state = self.hw.getDiscrete(path)
           ctl.setCurrentIndex(state)
           QObject.connect(ctl, SIGNAL('activated(int)'), self.updateSelector)

        for ctl, params in self.Volumes.items():
           path = params[0]
           idx  = params[1]
           pair = params[2]
           mute = params[3]
           link = params[4]

           db = self.hw.getContignuous(path, idx)
           vol = self.db2vol(db)
           ctl.setValue(vol)
           QObject.connect(ctl, SIGNAL('valueChanged(int)'), self.updateVolume)

           if vol == 0:
               mute.setChecked(True)

           if idx == 2:
               pair_db = self.hw.getContignuous(path, 1)
               if pair_db == db:
                   link.setChecked(True)

        for ctl, params in self.Preamps.items():
            path = params[0]
            idx = params[1]

            vol = self.hw.getDiscrete(path, idx)
            if vol > 0:
                ctl.setChecked(True)

            QObject.connect(ctl, SIGNAL('clicked(bool)'), self.updatePreamps)

        #       Right - Center - Left
        # 0x8000 - 0x0000 - 0x0001 - 0x7FFE
        #        ..., -1, 0, +1, ...
        for ctl, params in self.Pannings.items():
            path = params[0]
            idx = params[1]

            val = self.hw.getContignuous(path, idx)
            state = -(val / 0x7FFE) * 50 + 50
            ctl.setValue(state)
            QObject.connect(ctl, SIGNAL('valueChanged(int)'), self.updatePanning)

        for ctl, params in self.Mutes.items():
            QObject.connect(ctl, SIGNAL('clicked(bool)'), self.updateMute)

    # helper functions
    def vol2db(self, vol):
        return (log10(vol + 1) - 2) * 16384

    def db2vol(self, db):
        return pow(10, db / 16384 + 2) - 1

    def updateSelector(self, state):
        sender = self.sender()
        path   = self.Selectors[sender][0]
        self.hw.setDiscrete(path, state)

    def updatePreamps(self, state):
        sender = self.sender()
        path = self.Preamps[sender][0]
        idx = self.Preamps[sender][1]
        self.hw.setDiscrete(path, idx, state)

    def updateVolume(self, vol):
        sender = self.sender()
        path   = self.Volumes[sender][0]
        idx    = self.Volumes[sender][1]
        pair   = self.Volumes[sender][2]
        mute   = self.Volumes[sender][3]
        link   = self.Volumes[sender][4]

        if mute.isChecked():
                return

        db = self.vol2db(vol)
        self.hw.setContignuous(path, db, idx)

        if link.isChecked():
           pair.setValue(vol)

    def updatePanning(self, state):
        sender = self.sender()
        path = self.Pannings[sender][0]
        idx = self.Pannings[sender][1]
        val  = (state - 50) * 0x7FFE / -50
        self.hw.setContignuous(path, idx, val)

    def updateMute(self, state):
        sender = self.sender()
        pair = self.Mutes[sender][0]
        sld = self.Mutes[sender][1]

        path = self.Volumes[sld][0]
        idx = self.Volumes[sld][1]
        pair_sld = self.Volumes[sld][2]
        link = self.Volumes[sld][4]

        if state:
            db = 0x8000
            vol = 0
        else:
            db = 0x0000
            vol = 99

        self.hw.setContignuous(path, db, idx)
        sld.setValue(vol)
        sld.setDisabled(state)

        if link.isChecked():
            if idx == 1:
               idx = 2
            else:
               idx = 1
            self.hw.setContignuous(path, db, idx)
            pair.setChecked(state)
            pair_sld.setValue(vol)
            pair_sld.setDisabled(state)
