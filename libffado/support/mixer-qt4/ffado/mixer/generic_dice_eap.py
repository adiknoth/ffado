#
# Copyright (C) 2009-2010 by Arnold Krille
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
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

from PyQt4 import QtGui, QtCore, Qt
import dbus

from ffado.widgets.matrixmixer import MatrixMixer
from ffado.widgets.crossbarrouter import *

from ffado.config import *

class Generic_Dice_EAP(QtGui.QWidget):
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)
        self.tabs = QtGui.QTabWidget(self)
        self.layout.addWidget(self.tabs)

    def buildMixer(self):
        #print self.hw
        #print self.hw.getText("/Generic/Nickname")
        self.mixer = MatrixMixer(self.hw.servername, self.hw.basepath+"/EAP/MatrixMixer", self, "Columns_are_outputs")
        self.tabs.addTab(self.mixer, "Mixer")

        self.router = CrossbarRouter(self.hw.servername, self.hw.basepath+"/EAP/Router", self)
        scrollarea = QtGui.QScrollArea(self.tabs)
        scrollarea.setWidgetResizable(True)
        scrollarea.setWidget(self.router)
        self.tabs.addTab(scrollarea, "Crossbar Router")


    #def getDisplayTitle(self):
    #    return "Saffire PRO40/PRO24 Mixer"

#
# vim: et ts=4 sw=4
