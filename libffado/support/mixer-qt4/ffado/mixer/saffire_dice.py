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

class Saffire_Dice(Generic_Dice_EAP):
    def __init__(self, parent=None):
        Generic_Dice_EAP.__init__(self, parent)

    def buildMixer(self):
        #print self.hw
        #print self.hw.getText("/Generic/Nickname")
        Generic_Dice_EAP.buildMixer(self)

        widget = QtGui.QWidget()

        ModelName = self.configrom.getModelName()
        if  ModelName == "SAFFIRE_PRO_14":
            uicLoad("ffado/mixer/Saffire_Pro14_monitoring.ui", widget)
        elif ModelName == "SAFFIRE_PRO_24" or self.configrom.getModelName() == "SAFFIRE_PRO_24DSP":
            uicLoad("ffado/mixer/Saffire_Pro24_monitoring.ui", widget)
        elif ModelName == "SAFFIRE_PRO_40":
            uicLoad("ffado/mixer/Saffire_Pro40_monitoring.ui", widget)

        # Add Monitoring to ffado-mixer panels
        scrollarea = QtGui.QScrollArea(self.tabs)
        scrollarea.setWidgetResizable(False)
        scrollarea.setWidget(widget)
        self.tabs.addTab(scrollarea, "Monitoring")

        # Global settings
        self.muteInterface = BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/GlobalMute/State")
        widget.GlobalMute.setChecked(self.muteInterface.selected())
        self.connect(widget.GlobalMute, QtCore.SIGNAL("toggled(bool)"), self.muteToggle)

        self.dimInterface = BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/GlobalDim/State")
        widget.GlobalDim.setChecked(self.dimInterface.selected())
        self.connect(widget.GlobalDim, QtCore.SIGNAL("toggled(bool)"), self.dimToggle)
        self.dimLevelInterface = DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/GlobalDim/Level")
        widget.DimLevel.setValue(self.dimLevelInterface.getvalue())
        self.connect(widget.DimLevel, QtCore.SIGNAL("valueChanged(int)"), self.dimLevelChanged)
        self.DimLevel = widget.DimLevel
        widget.DimLevel.setEnabled(self.dimInterface.selected())

        # Per line monitoring
        from collections import namedtuple
        LineInfo = namedtuple('LineInfo', ['widget','Interface'])
        self.nbLines = 4

        # Mono/Stereo Switch
        self.LineMonos = []
        p = LineInfo(widget.Mono_12, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/Mono/Line1Line2"))
        self.LineMonos.append(p)
        p = LineInfo(widget.Mono_34, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/Mono/Line3Line4"))
        self.LineMonos.append(p)

        # Volume Unactivating
        self.LineUnActivates = []
        p = LineInfo(widget.LineUnActivate_1, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate1"))
        self.LineUnActivates.append(p)
        p = LineInfo(widget.LineUnActivate_2, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate2"))
        self.LineUnActivates.append(p)
        p = LineInfo(widget.LineUnActivate_3, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate3"))
        self.LineUnActivates.append(p)
        p = LineInfo(widget.LineUnActivate_4, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate4"))
        self.LineUnActivates.append(p)

        # Mute
        self.LineMutes = []
        p = LineInfo(widget.LineMute_1, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute1"))
        self.LineMutes.append(p)
        p = LineInfo(widget.LineMute_2, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute2"))
        self.LineMutes.append(p)
        p = LineInfo(widget.LineMute_3, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute3"))
        self.LineMutes.append(p)
        p = LineInfo(widget.LineMute_4, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute4"))
        self.LineMutes.append(p)

        # per line Global Mute switch
        self.LineGMutes = []
        p = LineInfo(widget.LineGMute_1, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute1"))
        self.LineGMutes.append(p)
        p = LineInfo(widget.LineGMute_2, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute2"))
        self.LineGMutes.append(p)
        p = LineInfo(widget.LineGMute_3, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute3"))
        self.LineGMutes.append(p)
        p = LineInfo(widget.LineGMute_4, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute4"))
        self.LineGMutes.append(p)

        # per line Global Dim switch
        self.LineGDims = []
        p = LineInfo(widget.LineGDim_1, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim1"))
        self.LineGDims.append(p)
        p = LineInfo(widget.LineGDim_2, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim2"))
        self.LineGDims.append(p)
        p = LineInfo(widget.LineGDim_3, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim3"))
        self.LineGDims.append(p)
        p = LineInfo(widget.LineGDim_4, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim4"))
        self.LineGDims.append(p)

        # Volume
        self.LineVolumes = []
        p = LineInfo(widget.LineVolume_1, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume1"))
        self.LineVolumes.append(p)
        p = LineInfo(widget.LineVolume_2, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume2"))
        self.LineVolumes.append(p)
        p = LineInfo(widget.LineVolume_3, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume3"))
        self.LineVolumes.append(p)
        p = LineInfo(widget.LineVolume_4, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume4"))
        self.LineVolumes.append(p)
  
        if ModelName != "SAFFIRE_PRO_14":
            self.nbLines = 6
            # Mono/Stereo Switch
            p = LineInfo(widget.Mono_56, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/Mono/Line5Line6"))
            self.LineMonos.append(p)
            # Volume Unactivating
            p = LineInfo(widget.LineUnActivate_5, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate5"))
            self.LineUnActivates.append(p)
            p = LineInfo(widget.LineUnActivate_6, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate6"))
            self.LineUnActivates.append(p)
            # Mute
            p = LineInfo(widget.LineMute_5, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute5"))
            self.LineMutes.append(p)
            p = LineInfo(widget.LineMute_6, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute6"))
            self.LineMutes.append(p)
            # per line Global Mute switch
            p = LineInfo(widget.LineGMute_5, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute5"))
            self.LineGMutes.append(p)
            p = LineInfo(widget.LineGMute_6, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute6"))
            self.LineGMutes.append(p)
            # per line Global Dim switch
            p = LineInfo(widget.LineGDim_5, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim5"))
            self.LineGDims.append(p)
            p = LineInfo(widget.LineGDim_6, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim6"))
            self.LineGDims.append(p)
            # Volume
            p = LineInfo(widget.LineVolume_5, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume5"))
            self.LineVolumes.append(p)
            p = LineInfo(widget.LineVolume_6, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume6"))
            self.LineVolumes.append(p)

        if ModelName == "SAFFIRE_PRO_40":
            self.nbLines = 10
            # Mono/Stereo Switch
            p = LineInfo(widget.Mono_78, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/Mono/Line7Line8"))
            self.LineMonos.append(p)
            p = LineInfo(widget.Mono_910, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/Mono/Line9Line10"))
            self.LineMonos.append(p)
            # Volume Unactivating
            p = LineInfo(widget.LineUnActivate_7, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate7"))
            self.LineUnActivates.append(p)
            p = LineInfo(widget.LineUnActivate_8, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate8"))
            self.LineUnActivates.append(p)
            p = LineInfo(widget.LineUnActivate_9, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate9"))
            self.LineUnActivates.append(p)
            p = LineInfo(widget.LineUnActivate_10, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/UnActivate10"))
            self.LineUnActivates.append(p)
            # Mute
            p = LineInfo(widget.LineMute_7, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute7"))
            self.LineMutes.append(p)
            p = LineInfo(widget.LineMute_8, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute8"))
            self.LineMutes.append(p)
            p = LineInfo(widget.LineMute_9, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute9"))
            self.LineMutes.append(p)
            p = LineInfo(widget.LineMute_10, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Mute10"))
            self.LineMutes.append(p)
            # per line Global Mute switch
            p = LineInfo(widget.LineGMute_7, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute7"))
            self.LineGMutes.append(p)
            p = LineInfo(widget.LineGMute_8, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute8"))
            self.LineGMutes.append(p)
            p = LineInfo(widget.LineGMute_9, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute9"))
            self.LineGMutes.append(p)
            p = LineInfo(widget.LineGMute_10, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GMute10"))
            self.LineGMutes.append(p)
            # per line Global Dim switch
            p = LineInfo(widget.LineGDim_7, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim7"))
            self.LineGDims.append(p)
            p = LineInfo(widget.LineGDim_8, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim8"))
            self.LineGDims.append(p)
            p = LineInfo(widget.LineGDim_9, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim9"))
            self.LineGDims.append(p)
            p = LineInfo(widget.LineGDim_10, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/GDim10"))
            self.LineGDims.append(p)
            # Volume
            p = LineInfo(widget.LineVolume_7, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume7"))
            self.LineVolumes.append(p)
            p = LineInfo(widget.LineVolume_8, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume8"))
            self.LineVolumes.append(p)
            p = LineInfo(widget.LineVolume_9, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume9"))
            self.LineVolumes.append(p)
            p = LineInfo(widget.LineVolume_10, DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineOut/Volume10"))
            self.LineVolumes.append(p)

            # No Line/Inst switch control from interface for Pro40
            widget.LineInSwitches.setVisible(False)

        else:
            # Line/Inst and Hi/Lo switches for Pro14 and 24
            widget.LineInSwitches.setVisible(True)
            self.LineInSwitches = []
            p = LineInfo(widget.LineInSwitchInst_1, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineInstGain/LineInst1"))
            self.LineInSwitches.append(p)
            p = LineInfo(widget.LineInSwitchInst_2, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineInstGain/LineInst2"))
            self.LineInSwitches.append(p)
            p = LineInfo(widget.LineInSwitchHi_3, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineInstGain/LineGain3"))
            self.LineInSwitches.append(p)
            p = LineInfo(widget.LineInSwitchHi_4, BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/LineInstGain/LineGain4"))
            self.LineInSwitches.append(p)
            for i in range(4):
                self.LineInSwitches[i].widget.setChecked(self.LineInSwitches[i].Interface.selected())
                self.connect(self.LineInSwitches[i].widget, QtCore.SIGNAL("toggled(bool)"), self.LineInSwitches[i].Interface.select)
            widget.LineInSwitchLine_1.setChecked(not self.LineInSwitches[0].Interface.selected())
            widget.LineInSwitchLine_2.setChecked(not self.LineInSwitches[1].Interface.selected())
            widget.LineInSwitchLo_3.setChecked(not self.LineInSwitches[2].Interface.selected())
            widget.LineInSwitchLo_4.setChecked(not self.LineInSwitches[3].Interface.selected())
            
        # Mono/Stereo Switch
        for i in range(self.nbLines/2):
            self.LineMonos[i].widget.setChecked(self.LineMonos[i].Interface.selected())
            self.connect(self.LineMonos[i].widget, QtCore.SIGNAL("toggled(bool)"), self.LineMonos[i].Interface.select)



        for i in range(self.nbLines):
            self.LineUnActivates[i].widget.setChecked(self.LineUnActivates[i].Interface.selected())
            self.connect(self.LineUnActivates[i].widget, QtCore.SIGNAL("toggled(bool)"), self.LineUnActivates[i].Interface.select)
            self.LineMutes[i].widget.setChecked(self.LineMutes[i].Interface.selected())
            self.connect(self.LineMutes[i].widget, QtCore.SIGNAL("toggled(bool)"), self.LineMutes[i].Interface.select)
            self.LineGMutes[i].widget.setChecked(self.LineGMutes[i].Interface.selected())
            self.connect(self.LineGMutes[i].widget, QtCore.SIGNAL("toggled(bool)"), self.LineGMutes[i].Interface.select)
            self.LineGDims[i].widget.setChecked(self.LineGDims[i].Interface.selected())
            self.connect(self.LineGDims[i].widget, QtCore.SIGNAL("toggled(bool)"), self.LineGDims[i].Interface.select)
            self.LineVolumes[i].widget.setValue(self.LineVolumes[i].Interface.getvalue())
            self.connect(self.LineVolumes[i].widget, QtCore.SIGNAL("valueChanged(int)"), self.LineVolumes[i].Interface.setvalue)
 
        # HW switch controls the possibility of monitoring each output separatly 
        widget.HWSwitch.setChecked(self.HWselected())
        self.connect(widget.HWSwitch, QtCore.SIGNAL("toggled(bool)"), self.HWToggle)

        # Line Out monitoring enabling depends on H/W switch
        self.LineOut = widget.LineOut
        self.LineOut.setEnabled(self.HWselected())

    def muteToggle(self, mute):
        self.muteInterface.select(mute)
    def dimToggle(self, dim):
        self.dimInterface.select(dim)
        self.DimLevel.setEnabled(dim)

    def HWToggle(self, HW):
        for i in range(self.nbLines):
            self.LineUnActivates[i].widget.setChecked(not HW)
            self.LineMutes[i].widget.setChecked(False)
            self.LineGMutes[i].widget.setChecked(True)
            self.LineGDims[i].widget.setChecked(True)
            self.LineVolumes[i].widget.setValue(0)
        for i in range(self.nbLines/2):
            self.LineMonos[i].widget.setChecked(False)
        self.LineOut.setEnabled(HW)

    def HWselected(self):
        selected = False
        for i in range(self.nbLines):
            if (not self.LineUnActivates[i].Interface.selected()):
                 selected = True
            if (self.LineMutes[i].Interface.selected()):
                 selected = True
        return selected

    def dimLevelChanged(self, value):
        self.dimLevelInterface.setvalue(value)
    def volumeChanged(self, value):
        self.volumeInterface.setvalue(value)

    def getDisplayTitle(self):
        return "Saffire PRO40/PRO24/PRO14 Mixer"

#
# vim: et ts=4 sw=4
