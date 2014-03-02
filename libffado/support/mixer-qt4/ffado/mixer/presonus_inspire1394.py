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
from PyQt4.QtGui import QGroupBox, QLabel, QSizePolicy, QSlider, QComboBox, QToolButton
from math import log10
from ffado.config import *

class Presonus_Inspire1394(QtGui.QWidget):
    # feature_id/name
    inputs = [[1, "Analog in 1/2"],
              [2, "Analog in 3/4"]]

    # feature_id/name
    mixer_src = [[3, "Analog in 1/2"],
                 [4, "Analog in 3/4"],
                 [5, "Stream in 5/6"]]

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

        grid = QGroupBox(box)
        box_layout.addWidget(grid)
        grid_layout = QGridLayout()
        grid.setLayout(grid_layout)
    
        self.addVolumes(self.inputs, grid, grid_layout)

    def addAnalogOutputs(self, box):
        box_layout = QVBoxLayout()
        box.setLayout(box_layout)
        box.setTitle("Analog Outputs")

        cmb = QComboBox(box)
        box_layout.addWidget(cmb)
        for i in range(len(self.out_src[1])):
            cmb.addItem(self.out_src[1][i], i)
            self.Selectors[cmb] = ["/Mixer/Selector_%d" % self.out_src[0]]

        grid = QGroupBox(box)
        box_layout.addWidget(grid)
        grid_layout = QGridLayout()
        grid.setLayout(grid_layout)

        self.addVolumes(self.outputs, grid, grid_layout)

    def addInternalMixer(self, box):
        box_layout = QGridLayout()
        box.setLayout(box_layout)
        box.setTitle("Hardware Mixer")

        self.addVolumes(self.mixer_src, box, box_layout)

    def addVolumes(self, elms, parent, layout):
        for col in range(len(elms)):
            label = QLabel(parent)
            label.setText(elms[col][1])
            layout.addWidget(label, 0, col * 2, 1, 2, Qt.AlignHCenter)

            l_sld = self.getSlider(parent)
            r_sld = self.getSlider(parent)
            layout.addWidget(l_sld, 1, col * 2, Qt.AlignHCenter)
            layout.addWidget(r_sld, 1, col * 2 + 1, Qt.AlignHCenter)

            link = self.getLink(parent)
            layout.addWidget(link, 2, col * 2, 1, 2, Qt.AlignHCenter)

            path = "/Mixer/Feature_Volume_%d" % elms[col][0]
            self.Volumes[l_sld] = [path, 1, r_sld, link]
            self.Volumes[r_sld] = [path, 2, l_sld, link]

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

    def getLink(self, parent):
        link = QToolButton(parent)
        link.setText("link")
        link.setCheckable(True)
        link.setMinimumWidth(100)
        link.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        return link

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
           link = params[3]

           db = self.hw.getContignuous(path, idx)
           vol = self.db2vol(db)
           ctl.setValue(vol)
           QObject.connect(ctl, SIGNAL('valueChanged(int)'), self.updateVolume)

           if idx == 2:
               pair_db = self.hw.getContignuous(path, 1)
               if pair_db == db:
                   link.setChecked(True)

    # helper functions
    def vol2db(self, vol):
        return (log10(vol + 1) - 2) * 16384

    def db2vol(self, db):
        return pow(10, db / 16384 + 2) - 1

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

        db = self.vol2db(vol)
        self.hw.setContignuous(path, db, idx)

        if link.isChecked():
           pair.setValue(vol)
