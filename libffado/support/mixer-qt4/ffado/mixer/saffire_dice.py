#
# Copyright (C) 2009 by Arnold Krille
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

class MonitoringModel(QtCore.QAbstractTableModel):
    def __init__(self, hw, parent):
        QtCore.QAbstractTableModel.__init__(self, parent)

        self.hw = hw

        self.mutestates = ("Global", "True", "False")

        self.globaldims = []
        self.globalmutes = []
        self.perchannelmutes = []
        self.globalvolumes = []
        self.perchannelvolumes = []
        for i in range(10):
            self.globaldims.append(BooleanControl(self.hw, self.hw.basepath + ("/EAP/Monitoring/GlobalDim/AffectsCh%i" % i)))
            self.globalmutes.append(BooleanControl(self.hw, self.hw.basepath + ("/EAP/Monitoring/GlobalMute/AffectsCh%i" % i)))
            self.perchannelmutes.append(BooleanControl(self.hw, self.hw.basepath + ("/EAP/Monitoring/PerChannel/Mute%i" % i)))
            self.globalvolumes.append(BooleanControl(self.hw, self.hw.basepath + ("/EAP/Monitoring/GlobalVolume/AffectsCh%i" % i)))
            self.perchannelvolumes.append(DiscreteControl(self.hw, self.hw.basepath + ("/EAP/Monitoring/PerChannel/Volume%i" % i)))

    def rowCount(self, parent):
        return 3

    def columnCount(self, parent):
        return 10

    def headerData(self, section, orientation, role=Qt.Qt.DisplayRole):
        if role != Qt.Qt.DisplayRole:
            #print "headerData will return nothing"
            return None
        #print "headerData() Will return a label"
        if orientation == Qt.Qt.Horizontal:
            return "Ch. %i" % section
        if orientation == Qt.Qt.Vertical:
            return ("Mute", "Dim", "Volume")[section]
        return None

    def data(self, index, role):
        if not role in (Qt.Qt.DisplayRole, Qt.Qt.EditRole):
            return None
        row = index.row()
        col = index.column()
        if role == Qt.Qt.DisplayRole:
            if row is 0:
                if self.perchannelmutes[col].selected():
                    return self.mutestates[1]
                else:
                    if self.globalmutes[col].selected():
                        return self.mutestates[0]
                return self.mutestates[2]
            if row is 1:
                if self.globaldims[col].selected():
                    return "Enabled"
                else:
                    return "Disabled"
            if row is 2:
                if self.globalvolumes[col].selected():
                    return "Global"
                else:
                    return self.perchannelvolumes[col].getvalue()
        if role == Qt.Qt.EditRole:
            if row is 0:
                return QtCore.QStringList(self.mutestates)
            if row is 1:
                return QtCore.QStringList(("Enabled","Disabled"))
            if row is 2:
                if self.globalvolumes[col].selected():
                    return 1
                return self.perchannelvolumes[col].getvalue()
        return "%i,%i" % (row,col)

    def setData(self, index, value, role=Qt.Qt.EditRole):
        col = index.column()
        row = index.row()
        if row == 0:
            if value == "Global":
                self.globalmutes[col].select(True)
                self.perchannelmutes[col].select(False)
            else:
                self.globalmutes[col].select(False)
                if value == "True": value = True
                if value == "False": value = False
                self.perchannelmutes[col].select(value)
            return True
        if row == 1:
            if value == "Enabled": value = True
            if value == "Disabled": value = False
            return self.globaldims[col].select(value)
        if row == 2:
            #print "setData() value=%s" % value.toString()
            v = int(value.toString())
            #print " integer is %i" % v
            if v > 0:
                return self.globalvolumes[col].select(True)
            else:
                self.globalvolumes[col].select(False)
                self.perchannelvolumes[col].setvalue(v)
        return False

    def flags(self, index):
        ret = QtCore.QAbstractTableModel.flags(self, index)
        if index.row() in (0,1,2):
            ret |= Qt.Qt.ItemIsEditable
        return ret



class MonitoringDelegate(QtGui.QStyledItemDelegate):
    def __init__(self, parent):
        QtGui.QStyledItemDelegate.__init__(self, parent)

    def createEditor(self, parent, option, index):
        if index.data(Qt.Qt.EditRole).type() == QtCore.QVariant.StringList:
            combo = QtGui.QComboBox(parent)
            self.connect(combo, QtCore.SIGNAL("activated(int)"), self.currentChanged)
            return combo
        else:
            return QtGui.QStyledItemDelegate.createEditor(self, parent, option, index)

    def setEditorData(self, editor, index):
        if isinstance(editor, QtGui.QComboBox):
            list = index.data(Qt.Qt.EditRole).toStringList()
            editor.addItems(list)
            editor.setCurrentIndex(list.indexOf(index.data(Qt.Qt.DisplayRole).toString()))
        else:
            QtGui.QStyledItemDelegate.setEditorData(self, editor, index)

    def setModelData(self, editor, model, index):
        if isinstance(editor, QtGui.QComboBox):
            model.setData(index, editor.currentText(), Qt.Qt.EditRole)
        else:
            QtGui.QStyledItemDelegate.setModelData(self, editor, model, index)

    def currentChanged(self):
        #print "currentChanged() sender=%s" % (str(self.sender()))
        editor = self.sender()
        self.emit(QtCore.SIGNAL("commitData(QWidget*)"), editor)
        self.emit(QtCore.SIGNAL("closeEditor(QWidget*)"), editor)

class Saffire_Dice(QtGui.QWidget):
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)
        self.tabs = QtGui.QTabWidget(self)
        self.layout.addWidget(self.tabs)

    def buildMixer(self):
        #print self.hw
        #print self.hw.getText("/Generic/Nickname")
        self.matrix = MatrixMixer(self.hw.servername, self.hw.basepath+"/EAP/MatrixMixer", self)
        scrollarea = QtGui.QScrollArea(self.tabs)
        scrollarea.setWidgetResizable(True)
        scrollarea.setWidget(self.matrix)
        self.tabs.addTab(scrollarea, "Matrix")

        self.router = CrossbarRouter(self.hw.servername, self.hw.basepath+"/EAP/Router", self)
        scrollarea = QtGui.QScrollArea(self.tabs)
        scrollarea.setWidgetResizable(True)
        scrollarea.setWidget(self.router)
        self.tabs.addTab(scrollarea, "Routing")

        model = MonitoringModel(self.hw, self)

        widget = QtGui.QWidget()
        uicLoad("ffado/mixer/saffire_dice_monitoring.ui", widget)
        widget.monitoringView.setModel(model)
        widget.monitoringView.setItemDelegate(MonitoringDelegate(self))
        self.tabs.addTab(widget, "Monitoring")

        self.muteInterface = BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/GlobalMute/State")
        widget.btnMute.setChecked(self.muteInterface.selected())
        self.connect(widget.btnMute, QtCore.SIGNAL("toggled(bool)"), self.muteToggle)
        self.dimInterface = BooleanControl(self.hw, self.hw.basepath+"/EAP/Monitoring/GlobalDim/State")
        widget.btnDim.setChecked(self.dimInterface.selected())
        self.connect(widget.btnDim, QtCore.SIGNAL("toggled(bool)"), self.dimToggle)

        self.dimLevelInterface = DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/GlobalDim/Level")
        widget.dimLevel.setValue(self.dimLevelInterface.getvalue())
        self.connect(widget.dimLevel, QtCore.SIGNAL("valueChanged(int)"), self.dimLevelChanged)
        self.volumeInterface = DiscreteControl(self.hw, self.hw.basepath+"/EAP/Monitoring/GlobalVolume/Level")
        widget.volumeKnob.setValue(self.volumeInterface.getvalue())
        self.connect(widget.volumeKnob, QtCore.SIGNAL("valueChanged(int)"), self.volumeChanged)

        if self.configrom.getModelName() == "SAFFIRE_PRO_24":
            self.ch1inst = BooleanControl(self.hw, self.hw.basepath + "/EAP/Ch1LineInst")
            widget.chkInst1.setChecked(self.ch1inst.selected())
            self.connect(widget.chkInst1, QtCore.SIGNAL("toggled(bool)"), self.ch1inst.select)
            self.ch2inst = BooleanControl(self.hw, self.hw.basepath + "/EAP/Ch2LineInst")
            widget.chkInst2.setChecked(self.ch2inst.selected())
            self.connect(widget.chkInst2, QtCore.SIGNAL("toggled(bool)"), self.ch2inst.select)
            self.ch3level = BooleanControl(self.hw, self.hw.basepath + "/EAP/Ch3Level")
            widget.chkLevel3.setChecked(self.ch3level.selected())
            self.connect(widget.chkLevel3, QtCore.SIGNAL("toggled(bool)"), self.ch3level.select)
            self.ch4level = BooleanControl(self.hw, self.hw.basepath + "/EAP/Ch4Level")
            widget.chkLevel4.setChecked(self.ch4level.selected())
            self.connect(widget.chkLevel4, QtCore.SIGNAL("toggled(bool)"), self.ch4level.select)
        else:
            widget.chkInst1.deleteLater()
            widget.chkInst2.deleteLater()
            widget.chkLevel3.deleteLater()
            widget.chkLevel4.deleteLater()


    def muteToggle(self, mute):
        self.muteInterface.select(mute)
    def dimToggle(self, mute):
        self.dimInterface.select(mute)

    def dimLevelChanged(self, value):
        self.dimLevelInterface.setvalue(value)
    def volumeChanged(self, value):
        self.volumeInterface.setvalue(value)

    def getDisplayTitle(self):
        return "Saffire PRO40/PRO24 Mixer"

#
# vim: et ts=4 sw=4
