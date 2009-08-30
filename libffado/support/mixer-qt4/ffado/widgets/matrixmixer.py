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
    def __init__(self, row, col, parent):
        QtGui.QFrame.__init__(self,parent)
        self.row = row
        self.connect(self.row, QtCore.SIGNAL("hide"), self.setHidden)
        self.column = col
        self.connect(self.column, QtCore.SIGNAL("hide"), self.setHidden)

        self.setLineWidth(1)
        self.setFrameStyle(QtGui.QFrame.Panel|QtGui.QFrame.Raised)
        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)

        self.dial = QtGui.QDial(self)
        self.dial.setRange(0,pow(2,16-1))
        self.connect(self.dial, QtCore.SIGNAL("valueChanged(int)"), self.valueChanged)
        self.layout.addWidget(self.dial, 0, 0)

    def valueChanged(self, n):
        self.emit(QtCore.SIGNAL("valueChanged"), self.row.number, self.column.number, n)

    def setHidden(self, hide):
        if not hide:
            if self.row.hidden or self.column.hidden:
                return
        QtGui.QFrame.setHidden(self, hide)

class MatrixChannel(QtGui.QWidget):
    def __init__(self, number, parent=None):
        QtGui.QWidget.__init__(self, parent)
        layout = QtGui.QGridLayout(self)
        self.number = number
        self.btn = QtGui.QPushButton("%i" % self.number, self)
        self.btn.setCheckable(True)
        self.connect(self.btn, QtCore.SIGNAL("clicked(bool)"), self.hideChannel)
        layout.addWidget(self.btn, 0, 0)

        self.hidden = False

    def hideChannel(self, hide):
        #print "MatrixChannel.hideChannel( %s )" % str(hide)
        self.hidden = hide
        self.emit(QtCore.SIGNAL("hide"), hide)

class MatrixMixer(QtGui.QWidget):
    def __init__(self, servername, basepath, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.bus = dbus.SessionBus()
        self.dev = self.bus.get_object(servername, basepath)
        self.interface = dbus.Interface(self.dev, dbus_interface="org.ffado.Control.Element.MatrixMixer")

        #print self.palette().color( QtGui.QPalette.Window ).name()
        self.palette().setColor( QtGui.QPalette.Window, self.palette().color( QtGui.QPalette.Window ).darker() );
        #print self.palette().color( QtGui.QPalette.Window ).name()

        rows = self.interface.getRowCount()
        cols = self.interface.getColCount()

        layout = QtGui.QGridLayout(self)
        self.setLayout(layout)

        self.columns = []
        self.rows = []

        # Add row/column headers
        for i in range(cols):
            ch = MatrixChannel(i, self)
            layout.addWidget(ch, 0, i+1)
            self.columns.append( ch )
        for i in range(rows):
            ch = MatrixChannel(i, self)
            layout.addWidget(ch, i+1, 0)
            self.rows.append( ch )

        # Add node-widgets
        for i in range(rows):
            for j in range(cols):
                node = MatrixNode(self.rows[i], self.columns[j], self)
                node.dial.setValue(self.interface.getValue(i, j))
                self.connect(node, QtCore.SIGNAL("valueChanged"), self.valueChanged)
                layout.addWidget(node, i+1, j+1)

    def valueChanged(self, row, column, n):
        #print "MatrixNode.valueChanged( %i, %i, %i )" % (row,column,n)
        self.interface.setValue(row, column, n)

    def paintEvent(self, event):
        p = QtGui.QPainter(self)
        p.fillRect(event.rect(), self.palette().window())

#
# vim: et ts=4 sw=4
