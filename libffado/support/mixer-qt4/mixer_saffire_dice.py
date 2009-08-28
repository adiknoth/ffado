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

from PyQt4 import QtGui, QtCore
import dbus

class MatrixNode(QtGui.QFrame):
    def __init__(self,row,col,parent):
        QtGui.QFrame.__init__(self,parent)
        self.row = row
        self.column = col

        self.setLineWidth(1)
        self.setFrameStyle(QtGui.QFrame.Panel|QtGui.QFrame.Raised)
        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)

        self.lbl = QtGui.QLabel("(%i,%i)" % (row,col), self)
        self.layout.addWidget(self.lbl, 0, 0)

        self.dial = QtGui.QDial(self)
        self.dial.setRange(0,pow(2,16-1))
        self.connect(self.dial, QtCore.SIGNAL("valueChanged(int)"), self.valueChanged)
        self.layout.addWidget(self.dial, 1, 0)

    def valueChanged(self, n):
        self.emit(QtCore.SIGNAL("valueChanged"), self.row, self.column, n)

class MatrixMixer(QtGui.QWidget):
    def __init__(self,servername,basepath,parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.bus = dbus.SessionBus()
        self.dev = self.bus.get_object(servername, basepath)
        self.interface = dbus.Interface(self.dev, dbus_interface="org.ffado.Control.Element.MatrixMixer")

        rows = min(8, self.interface.getRowCount())
        cols = min(8, self.interface.getColCount())

        layout = QtGui.QGridLayout(self)
        self.setLayout(layout)

        for i in range(rows):
            for j in range(cols):
                node = MatrixNode(i, j, self)
                node.dial.setValue(self.interface.getValue(i, j))
                self.connect(node, QtCore.SIGNAL("valueChanged"), self.valueChanged)
                layout.addWidget(node, i, j)

    def valueChanged(self, row, column, n):
        #print "MatrixNode.valueChanged( %i, %i, %i )" % (row,column,n)
        self.interface.setValue(row, column, n)

class Saffire_Dice(QtGui.QWidget):
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)

    def buildMixer(self):
        print self.hw
        print self.hw.getText("/Generic/Nickname")
        self.matrix = MatrixMixer(self.hw.servername, self.hw.basepath+"/EAP/MatrixMixer", self)
        self.layout.addWidget(self.matrix)

    def getDisplayTitle(self):
        return "Experimental EAP Mixer"


#
# vim: et ts=4 sw=4
