# coding=utf8
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
import dbus, math, decimal

import logging
log = logging.getLogger("matrixmixer")

class ColorForNumber:
    def __init__(self):
        self.colors = dict()

    def addColor(self, n, color):
        self.colors[n] = color

    def getColor(self, n):
        #print "ColorForNumber.getColor( %g )" % (n)
        keys = self.colors.keys()
        keys.sort()
        low = keys[-1]
        high = keys[-1]
        for i in range(len(keys)-1):
            if keys[i] <= n and keys[i+1] > n:
                low = keys[i]
                high = keys[i+1]
        #print "%g is between %g and %g" % (n, low, high)
        f = 0
        if high != low:
            f = (n-low) / (high-low)
        lc = self.colors[low]
        hc = self.colors[high]
        return QtGui.QColor(
                (1-f)*lc.red()   + f*hc.red(),
                (1-f)*lc.green() + f*hc.green(),
                (1-f)*lc.blue()  + f*hc.blue() )

class MixerNode(QtGui.QAbstractSlider):
    def __init__(self, input, output, value, parent):
        QtGui.QAbstractSlider.__init__(self, parent)
        #log.debug("MixerNode.__init__( %i, %i, %i, %s )" % (input, output, value, str(parent)) )

        self.pos = QtCore.QPointF(0, 0)
        self.input = input
        self.output = output
        self.setOrientation(Qt.Qt.Vertical)
        self.setRange(0, pow(2, 16)-1)
        self.setValue(value)
        self.connect(self, QtCore.SIGNAL("valueChanged(int)"), self.internalValueChanged)

        self.setSmall(False)

        self.bgcolors = ColorForNumber()
        self.bgcolors.addColor(             0.0, QtGui.QColor(  0,   0,   0))
        self.bgcolors.addColor(             1.0, QtGui.QColor(  0,   0, 128))
        self.bgcolors.addColor(   math.pow(2,6), QtGui.QColor(  0, 255,   0))
        self.bgcolors.addColor(  math.pow(2,14), QtGui.QColor(255, 255,   0))
        self.bgcolors.addColor(math.pow(2,16)-1, QtGui.QColor(255,   0,   0))

        self.setContextMenuPolicy(Qt.Qt.ActionsContextMenu)
        self.mapper = QtCore.QSignalMapper(self)
        self.connect(self.mapper, QtCore.SIGNAL("mapped(const QString&)"), self.directValues)

        self.spinbox = QtGui.QDoubleSpinBox(self)
        self.spinbox.setRange(-40, 12)
        self.spinbox.setSuffix(" dB")
        self.connect(self.spinbox, QtCore.SIGNAL("valueChanged(const QString&)"), self.directValues)
        action = QtGui.QWidgetAction(self)
        action.setDefaultWidget(self.spinbox)
        self.addAction(action)

        for text in ["3 dB", "0 dB", "-3 dB", "-20 dB", "-inf dB"]:
            action = QtGui.QAction(text, self)
            self.connect(action, QtCore.SIGNAL("triggered()"), self.mapper, QtCore.SLOT("map()"))
            self.mapper.setMapping(action, text)
            self.addAction(action)

    def directValues(self,text):
        #log.debug("MixerNode.directValues( '%s' )" % text)
        text = text.split(" ")[0].replace(",",".")
        n = pow(10, (float(text)/20)) * pow(2,14)
        #log.debug("%g" % n)
        self.setValue(n)

    def mousePressEvent(self, ev):
        if ev.buttons() & Qt.Qt.LeftButton:
            self.pos = ev.posF()
            self.tmpvalue = self.value()
            ev.accept()
            #log.debug("MixerNode.mousePressEvent() %s" % str(self.pos))

    def mouseMoveEvent(self, ev):
        if hasattr(self, "tmpvalue") and self.pos is not QtCore.QPointF(0, 0):
            newpos = ev.posF()
            change = newpos.y() - self.pos.y()
            #log.debug("MixerNode.mouseReleaseEvent() change %s" % (str(change)))
            self.setValue( self.tmpvalue - math.copysign(pow(abs(change), 2), change) )
            ev.accept()

    def mouseReleaseEvent(self, ev):
        if hasattr(self, "tmpvalue") and self.pos is not QtCore.QPointF(0, 0):
            newpos = ev.posF()
            change = newpos.y() - self.pos.y()
            #log.debug("MixerNode.mouseReleaseEvent() change %s" % (str(change)))
            self.setValue( self.tmpvalue - math.copysign(pow(abs(change), 2), change) )
            self.pos = QtCore.QPointF(0, 0)
            del self.tmpvalue
            ev.accept()

    def paintEvent(self, ev):
        p = QtGui.QPainter(self)
        rect = self.rect()
        v = self.value()
        color = self.bgcolors.getColor(v)
        p.fillRect(rect, color)

        if self.small:
            return

        if color.valueF() < 0.6:
            p.setPen(QtGui.QColor(255, 255, 255))
        else:
            p.setPen(QtGui.QColor(0, 0, 0))
        lv=decimal.Decimal('-Infinity')
        if v != 0:
            lv = 20 * math.log10(v * 1.0 / math.pow(2,14))
            #log.debug("new value is %g dB" % lv)
        text = "%.2g dB" % lv
        if v == 0:
            text = "-ꝏ dB"
        p.drawText(rect, Qt.Qt.AlignCenter, QtCore.QString.fromUtf8(text))

    def internalValueChanged(self, value):
        #log.debug("MixerNode.internalValueChanged( %i )" % value)
        if value is not 0:
            dB = 20 * math.log10(value / math.pow(2,14))
            if self.spinbox.value() is not dB:
                self.spinbox.setValue(dB)
        self.emit(QtCore.SIGNAL("valueChanged"), (self.input, self.output, value) )
        self.update()

    def setSmall(self, small):
        self.small = small
        if small:
            self.setMinimumSize(10, 10)
        else:
            fontmetrics = self.fontMetrics()
            self.setMinimumSize(fontmetrics.boundingRect("-0.0 dB").size()*1.1)
        self.update()


class MixerChannel(QtGui.QWidget):
    def __init__(self, number, parent=None, name=""):
        QtGui.QWidget.__init__(self, parent)
        layout = QtGui.QGridLayout(self)
        self.number = number
        if name != "":
            name = "\n(%s)" % name
        self.name = name
        self.lbl = QtGui.QLabel(self)
        self.lbl.setAlignment(Qt.Qt.AlignCenter)
        layout.addWidget(self.lbl, 0, 0, 1, 2)
        self.hideChannel(False)

        self.setContextMenuPolicy(Qt.Qt.ActionsContextMenu)

        action = QtGui.QAction("Make this channel small", self)
        action.setCheckable(True)
        self.connect(action, QtCore.SIGNAL("triggered(bool)"), self.hideChannel)
        self.addAction(action)

    def hideChannel(self, hide):
        if hide:
            self.lbl.setText("%i" % self.number);
        else:
            self.lbl.setText("Ch. %i%s" % (self.number, self.name))
        self.emit(QtCore.SIGNAL("hide"), self.number, hide)
        self.update()



class MatrixMixer(QtGui.QWidget):
    def __init__(self, servername, basepath, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.bus = dbus.SessionBus()
        self.dev = self.bus.get_object(servername, basepath)
        self.interface = dbus.Interface(self.dev, dbus_interface="org.ffado.Control.Element.MatrixMixer")

        #palette = self.palette()
        #palette.setColor(QtGui.QPalette.Window, palette.color(QtGui.QPalette.Window).darker());
        #self.setPalette(palette)

        rows = self.interface.getColCount()
        cols = self.interface.getRowCount()
        log.debug("Mixer has %i rows and %i columns" % (rows, cols))

        layout = QtGui.QGridLayout(self)
        self.setLayout(layout)

        self.rowHeaders = []
        self.columnHeaders = []
        self.items = []

        # Add row/column headers
        for i in range(cols):
            ch = MixerChannel(i, self, self.interface.getColName(i))
            self.connect(ch, QtCore.SIGNAL("hide"), self.hideColumn)
            layout.addWidget(ch, 0, i+1)
            self.columnHeaders.append( ch )
        for i in range(rows):
            ch = MixerChannel(i, self, self.interface.getRowName(i))
            self.connect(ch, QtCore.SIGNAL("hide"), self.hideRow)
            layout.addWidget(ch, i+1, 0)
            self.rowHeaders.append( ch )

        # Add node-widgets
        for i in range(rows):
            self.items.append([])
            for j in range(cols):
                node = MixerNode(j, i, self.interface.getValue(i,j), self)
                self.connect(node, QtCore.SIGNAL("valueChanged"), self.valueChanged)
                layout.addWidget(node, i+1, j+1)
                self.items[i].append(node)

        self.hiddenRows = []
        self.hiddenCols = []


    def checkVisibilities(self):
        for x in range(len(self.items)):
            for y in range(len(self.items[x])):
                self.items[x][y].setSmall(
                        (x in self.hiddenRows)
                        | (y in self.hiddenCols)
                        )


    def hideColumn(self, column, hide):
        if hide:
            self.hiddenCols.append(column)
        else:
            self.hiddenCols.remove(column)
        self.checkVisibilities()
    def hideRow(self, row, hide):
        if hide:
            self.hiddenRows.append(row)
        else:
            self.hiddenRows.remove(row)
        self.checkVisibilities()

    def valueChanged(self, n):
        #log.debug("MatrixNode.valueChanged( %s )" % str(n))
        self.interface.setValue(n[1], n[0], n[2])


#
# vim: et ts=4 sw=4 fileencoding=utf8
