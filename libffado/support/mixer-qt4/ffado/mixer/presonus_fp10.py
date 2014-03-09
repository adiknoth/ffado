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
from PyQt4.QtGui import QGridLayout
from PyQt4.QtGui import QWidget, QLabel, QSizePolicy, QToolButton, QSlider, QDial
from math import log10
from ffado.config import *

class Presonus_FP10(QtGui.QWidget):
    outputs = [["Analog Out 1/2", 1],
               ["Analog Out 3/4", 2],
               ["Analog Out 5/6", 3],
               ["Analog Out 7/8", 4]]

    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)

    def getDisplayTitle(self):
        return 'FP10'

    def buildMixer(self):
        self.Volumes = {}
        self.Mutes = {}
        self.Pannings = {}

        layout = QGridLayout(self)

        for i in range(len(self.outputs)):
            self.addVolume(self.outputs[i][0], self.outputs[i][1], i, layout)

    def addVolume(self, name, bid, col, layout):
        label = QLabel(self)
        label.setText(name)
        l_dial = self.getDial()
        r_dial = self.getDial()
        l_sld = self.getSlider()
        r_sld = self.getSlider()
        l_mute = self.getMute()
        r_mute = self.getMute()
        link = self.getLink()

        layout.addWidget(label,  1, col * 2, 1, 2, Qt.AlignHCenter)
        layout.addWidget(l_dial, 2, col * 2,       Qt.AlignHCenter)
        layout.addWidget(r_dial, 2, col * 2 + 1,   Qt.AlignHCenter)
        layout.addWidget(l_sld,  3, col * 2,       Qt.AlignHCenter)
        layout.addWidget(r_sld,  3, col * 2 + 1,   Qt.AlignHCenter)
        layout.addWidget(l_mute, 4, col * 2,       Qt.AlignHCenter)
        layout.addWidget(r_mute, 4, col * 2 + 1,   Qt.AlignHCenter)
        layout.addWidget(link,   5, col * 2, 1, 2, Qt.AlignHCenter)
        
        path = "/Mixer/Feature_Volume_%d" % bid
        self.Volumes[l_sld] = [path, 1, r_sld, l_mute, link]
        self.Volumes[r_sld] = [path, 2, l_sld, r_mute, link]
        self.Mutes[l_mute] = [r_mute, l_sld]
        self.Mutes[r_mute] = [l_mute, r_sld]

        path = "/Mixer/Feature_LRBalance_%d" % bid
        self.Pannings[l_dial] = [path, 1]
        self.Pannings[r_dial] = [path, 2]
        
    # widget helper functions
    def getSlider(self):
        sld = QSlider(self)
        sld.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Minimum)
        sld.setMinimum(0)
        sld.setMaximum(99)
        sld.setPageStep(10)
        sld.setPageStep(10)
        sld.setMinimumHeight(50)
        sld.setTickInterval(25)
        sld.setTickPosition(QSlider.TicksBothSides)
        return sld

    def getDial(self):
        dial = QDial(self)
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

    def getLink(self):
        link = QToolButton(self)
        link.setText("link")
        link.setCheckable(True)
        link.setMinimumWidth(100)
        link.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        return link

    def getMute(self):
        mute = QToolButton(self)
        mute.setText("mute")
        mute.setCheckable(True)
        mute.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        return mute

    def initValues(self):
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

        return

    # helper functions
    def vol2db(self, vol):
        return (log10(vol + 1) - 2) * 16384

    def db2vol(self, db):
        return pow(10, db / 16384 + 2) - 1

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

