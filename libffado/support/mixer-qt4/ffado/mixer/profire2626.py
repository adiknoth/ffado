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
from ffado.mixer.generic_dice_eap import *

from ffado.config import *

class BooleanControl:
    def __init__(self, hw, path):
        self.iface = dbus.Interface(
                hw.bus.get_object(hw.servername, path),
                dbus_interface="org.ffado.Control.Element.Boolean")
        self.value = self.iface.selected()

    def selected(self):
        return self.value

    def select(self, n):
        if self.iface.select(n):
            self.value = n
            return True
        return False

class DiscreteControl:
    def __init__(self, hw, path):
        self.iface = dbus.Interface(
                hw.bus.get_object(hw.servername, path),
                dbus_interface="org.ffado.Control.Element.Discrete")
        self.value = self.iface.getValue()

    def getvalue(self):
        return self.value

    def setvalue(self, v):
        if v != self.value:
            self.iface.setValue(v)
            self.value = v

class Profire2626(Generic_Dice_EAP):
    def __init__(self, parent=None):
        Generic_Dice_EAP.__init__(self, parent)

    def buildMixer(self):
        #print self.hw
        #print self.hw.getText("/Generic/Nickname")
        Generic_Dice_EAP.buildMixer(self)

        widget = QtGui.QWidget()
        uicLoad("ffado/mixer/profire2626_settings.ui", widget)

        # Add Settings to ffado-mixer panels
        scrollarea = QtGui.QScrollArea(self.tabs)
        scrollarea.setWidgetResizable(False)
        scrollarea.setWidget(widget)
        self.tabs.addTab(scrollarea, "Settings")

        # Master volume knob
        from collections import namedtuple
        LineInfo = namedtuple('LineInfo', ['widget','Interface'])

        # Volume Unactivating
        self.LineUnActivates = []
        p = LineInfo(widget.line1line2, BooleanControl(self.hw, self.hw.basepath+"/EAP/Settings/VolumeKnob/Line1Line2"))
        self.LineUnActivates.append(p)
        p = LineInfo(widget.line3line4, BooleanControl(self.hw, self.hw.basepath+"/EAP/Settings/VolumeKnob/Line3Line4"))
        self.LineUnActivates.append(p)
        p = LineInfo(widget.line5line6, BooleanControl(self.hw, self.hw.basepath+"/EAP/Settings/VolumeKnob/Line5Line6"))
        self.LineUnActivates.append(p)
        p = LineInfo(widget.line7line8, BooleanControl(self.hw, self.hw.basepath+"/EAP/Settings/VolumeKnob/Line7Line8"))
        self.LineUnActivates.append(p)

        for l in self.LineUnActivates:
            l.widget.setChecked(l.Interface.selected())
            self.connect(l.widget, QtCore.SIGNAL("toggled(bool)"), l.Interface.select)

    def getDisplayTitle(self):
        return "M-Audio Profire 2626 Mixer"

#
# vim: et ts=4 sw=4
