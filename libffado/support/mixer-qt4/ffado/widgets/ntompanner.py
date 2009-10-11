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
import dbus, math, numpy

import logging
log = logging.getLogger("widgets")

percentage = 0.3
circlerect = QtCore.QRectF(-percentage/2, -percentage/2, percentage, percentage)

class PannerSource(QtGui.QGraphicsEllipseItem):
    def __init__(self, number, parent):
        QtGui.QGraphicsEllipseItem.__init__(self)
        self.setRect(circlerect)

        self.number = number
        self.parent = parent

        self.setBrush(QtGui.QColor(255, 0, 0))
        self.setPen(QtGui.QPen(Qt.Qt.NoPen))

        self.setFlags(QtGui.QGraphicsItem.ItemIsMovable | QtGui.QGraphicsItem.ItemIsFocusable)

    def focusInEvent(self, ev):
        QtGui.QGraphicsEllipseItem.focusInEvent(self, ev)

    def itemChange(self, change, value):
        if change == QtGui.QGraphicsItem.ItemPositionChange:
            value = self.parent.nearestSink(value.toPointF())
        if change == QtGui.QGraphicsItem.ItemPositionHasChanged:
            self.parent.sourcePositionChanged(self.number)
        return QtGui.QGraphicsEllipseItem.itemChange(self, change, value)

class N2MPanner(QtGui.QFrame):
    def __init__(self, parent):
        QtGui.QFrame.__init__(self, parent)

        layout = QtGui.QGridLayout(self)
        self.setLayout(layout)

        self.view = QtGui.QGraphicsView(self)
        layout.addWidget(self.view, 0, 0)

        self.scene = QtGui.QGraphicsScene(QtCore.QRectF(-1, -1, 2, 2), self)
        self.view.setScene(self.scene)

        self.scene.addEllipse( QtCore.QRectF(-1,-1,2,2), QtGui.QPen(self.palette().color(QtGui.QPalette.Window).darker() ) ).setZValue(-1)

        self.pen = QtGui.QPen(self.palette().color(QtGui.QPalette.Window).darker())
        #pen.setWidthF(0.01)

        self.sources = []
        self.sinks = []
        self.values = numpy.array(numpy.float)
        #print self.values.flags
        #print self.values

        #self.updateGeometry()
        QtCore.QTimer.singleShot(10, self.resizeEvent)

    def setNumberOfSinks(self, sinknr):
        while len(self.sinks) > sinknr:
            self.sinks[-1].setParentItem(None)
            del self.sinks[-1]
        while len(self.sinks) < sinknr:
            c = self.scene.addEllipse(circlerect, self.pen)
            c.setZValue(1)
            self.sinks.append(c)
        for i in range(sinknr):
            degree = 2*math.pi/sinknr
            self.sinks[i].setPos(math.cos(degree), math.sin(degree))
        self.update()
        self.values.resize((len(self.sources), len(self.sinks)))
        self.sourcePositionChanged(range(len(self.sources)))

    def setNumberOfSources(self, sourcenr):
        while len(self.sources) > sourcenr:
            self.sources[-1].setParentItem(None)
            del self.sources[-1]
        while len(self.sources) < sourcenr:
            s = PannerSource(len(self.sources), self)
            self.scene.addItem(s)
            self.sources.append(s)
        self.values.resize((len(self.sources), len(self.sinks)))
        self.sourcePositionChanged(range(len(self.sources)))

    def distance(self, p1, p2):
        return math.sqrt( math.pow(p1.x()-p2.x(), 2) + math.pow(p1.y()-p2.y(), 2) )

    def sourcePositionChanged(self, number):
        if not isinstance(number, list):
            number = [number]
        #print "N2MPanner.sourcePositionChanged( %s )" % str(number)
        for i in number:
            for sink in self.sinks:
                j = self.sinks.index(sink)
                self.values[i,j] = self.distance(self.sources[i].pos(), sink.pos())
                #print " distance to sink(%g, %g) is %g" % (sink.x(), sink.y(), self.distance(self.sources[number].pos(), sink.pos()))
        #print self.values
        self.emit(QtCore.SIGNAL("valuesChanged"))

    def nearestSink(self, point):
        for sink in self.sinks:
            if sink.contains(sink.mapFromParent(point)):
                return sink.pos()
        return point

    def resizeEvent(self, event=None):
        self.view.fitInView(QtCore.QRectF(-1.2, -1.2, 2.4, 2.4), Qt.Qt.KeepAspectRatio)


if __name__ == "__main__":

    import sys

    app = QtGui.QApplication(sys.argv)

    w = QtGui.QWidget(None)
    l = QtGui.QVBoxLayout(w)

    s1 = QtGui.QSpinBox(w)
    s1.setRange(1,10)
    l.addWidget(s1)

    s2 = QtGui.QSpinBox(w)
    s2.setRange(1,10)
    l.addWidget(s2)

    p = N2MPanner(w)
    p.setNumberOfSinks(1)
    p.setNumberOfSources(1)
    QtCore.QObject.connect(s1, QtCore.SIGNAL("valueChanged(int)"), p.setNumberOfSinks)
    QtCore.QObject.connect(s2, QtCore.SIGNAL("valueChanged(int)"), p.setNumberOfSources)
    l.addWidget(p)

    w.show()

    app.exec_()


#
# vim: et ts=4 sw=4
