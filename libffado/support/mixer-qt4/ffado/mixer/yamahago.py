#
# Copyright (c) 2013      by Takashi Sakamoto
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

from PyQt4.QtCore import SIGNAL, SLOT, QObject
from PyQt4.QtGui import QWidget, QMessageBox
from math import log10
from ffado.config import *

import logging
log = logging.getLogger('YamahaGo')

class YamahaGo(QWidget):
    def __init__(self, parent=None):
        QWidget.__init__(self, parent)

    def getDisplayTitle(self):
        if self.configrom.getModelId() == 0x0010000B:
            return "GO44"
        else:
            return "GO46"

    # startup
    def initValues(self):
        uicLoad("ffado/mixer/yamahago", self)

        self.modelId = self.configrom.getModelId()
        # GO44
        if self.modelId == 0x0010000B:
            self.setWindowTitle("Yamaha GO44 Control")
            self.box_ana_out.hide()
            self.is46 = False
        # GO46
        elif self.modelId == 0x0010000C:
            self.setWindowTitle("Yamaha GO46 Control")
            self.box_ana_in_12_level.hide()
            self.is46 = True

        # common
        self.VolumeControls = {
            self.sld_mixer_strm_in_1:    ['/Mixer/Feature_Volume_3', 1, self.sld_mixer_strm_in_2, 2, self.link_mixer_strm_in_12],
            self.sld_mixer_strm_in_2:    ['/Mixer/Feature_Volume_3', 2, self.sld_mixer_strm_in_1, 1, self.link_mixer_strm_in_12],
            self.sld_mixer_strm_in_3:    ['/Mixer/Feature_Volume_4', 1, self.sld_mixer_strm_in_4, 2, self.link_mixer_strm_in_34],
            self.sld_mixer_strm_in_4:    ['/Mixer/Feature_Volume_4', 2, self.sld_mixer_strm_in_3, 1, self.link_mixer_strm_in_34],
            self.sld_mixer_strm_in_5:    ['/Mixer/Feature_Volume_5', 1, self.sld_mixer_strm_in_6, 2, self.link_mixer_strm_in_56],
            self.sld_mixer_strm_in_6:    ['/Mixer/Feature_Volume_5', 2, self.sld_mixer_strm_in_5, 1, self.link_mixer_strm_in_56],
            self.sld_mixer_ana_in_1:    ['/Mixer/Feature_Volume_6', 1, self.sld_mixer_ana_in_2,  2, self.link_mixer_ana_in_12],
            self.sld_mixer_ana_in_2:    ['/Mixer/Feature_Volume_6', 2, self.sld_mixer_ana_in_1,  1, self.link_mixer_ana_in_12],
            self.sld_mixer_dig_in_1:    ['/Mixer/Feature_Volume_7', 1, self.sld_mixer_dig_in_2,  2, self.link_mixer_dig_in_12],
            self.sld_mixer_dig_in_2:    ['/Mixer/Feature_Volume_7', 2, self.sld_mixer_dig_in_1,  1, self.link_mixer_dig_in_12]
        }
        self.JackSourceSelectors = {
            self.cmb_ana_out_12_route:    '/Mixer/Selector_1',
            self.cmb_ana_out_34_route:    '/Mixer/Selector_2',
            self.cmb_dig_out_12_route:    '/Mixer/Selector_3'
            }

        if not self.is46:
            # volume for mixer output
            self.VolumeControls[self.sld_mixer_out_1] = ['/Mixer/Feature_Volume_1', 1, self.sld_mixer_out_2, 2, self.link_mixer_out_12]
            self.VolumeControls[self.sld_mixer_out_2] = ['/Mixer/Feature_Volume_1', 2, self.sld_mixer_out_1, 1, self.link_mixer_out_12]
            # analog out 3/4 is headphone out 1/2
            self.label_ana_out_34_route.setText("Headphone out 1/2")
        else:
            # volume for mixer output
            self.VolumeControls[self.sld_mixer_out_1] = ['/Mixer/Feature_Volume_2', 1, self.sld_mixer_out_2, 2, self.link_mixer_out_12]
            self.VolumeControls[self.sld_mixer_out_2] = ['/Mixer/Feature_Volume_2', 2, self.sld_mixer_out_1, 1, self.link_mixer_out_12]
            # volume for analog output
            self.VolumeControls[self.sld_ana_out_1] = ['/Mixer/Feature_Volume_1', 1, self.sld_ana_out_2, 2, self.link_ana_out_12]
            self.VolumeControls[self.sld_ana_out_2] = ['/Mixer/Feature_Volume_1', 2, self.sld_ana_out_1, 1, self.link_ana_out_12]
            self.VolumeControls[self.sld_ana_out_3] = ['/Mixer/Feature_Volume_1', 3, self.sld_ana_out_4, 4, self.link_ana_out_34]
            self.VolumeControls[self.sld_ana_out_4] = ['/Mixer/Feature_Volume_1', 4, self.sld_ana_out_3, 3, self.link_ana_out_34]

        # gain control
        for ctl, params in self.VolumeControls.items():
            path = params[0]
            idx = params[1]

            db = self.hw.getContignuous(path, idx)
            vol = self.db2vol(db)
            ctl.setValue(vol)

            link = params[4]

            pvol = self.db2vol(db)

            if pvol == vol:
                link.setChecked(True)

            QObject.connect(ctl, SIGNAL('valueChanged(int)'), self.updateVolume)

        # source selector for jack output
        for ctl, param in self.JackSourceSelectors.items():
            state = self.hw.getDiscrete(param)
            ctl.setCurrentIndex(state)

            QObject.connect(ctl, SIGNAL('activated(int)'), self.updateSelector)

        if not self.is46:
            QObject.connect(self.cmb_ana_in_12_level, SIGNAL('activated(int)'), self.updateMicLevel)

    # helper functions
    def vol2db(self, vol):
        return (log10(vol + 1) - 2) * 16384
    def db2vol(self, db):
        return pow(10, db / 16384 + 2) - 1

    def updateMicLevel(self, state):
        if state == 0:
            level = 0
        elif state == 1:
            level = -1535
        else:
            level = -3583
        self.hw.setContignuous('/Mixer/Feature_Volume_2', level)

    def updateVolume(self, vol):
        sender = self.sender()
        path = self.VolumeControls[sender][0]
        idx = self.VolumeControls[sender][1]
        pair = self.VolumeControls[sender][2]
        link = self.VolumeControls[sender][4]

        db = self.vol2db(vol)
        self.hw.setContignuous(path, db, idx)

        if link.isChecked():
            pair.setValue(vol)

    def updateSelector(self, state):
        sender = self.sender()
        path = self.JackSourceSelectors[sender]
        self.hw.setDiscrete(path, state)
