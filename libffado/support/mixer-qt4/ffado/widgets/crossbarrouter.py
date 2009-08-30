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


class CrossbarState(QtGui.QAbstractButton):
    def __init__(self, title, parent=None):
        QtGui.QAbstractButton.__init__(self, parent)
        self.setText(title)
        self.setToolTip(title)

        self.setMinimumSize(QtCore.QSize(10, 10))

    def paintEvent(self, event):
        p = QtGui.QPainter(self)
        color = QtGui.QColor(0, 255, 0)
        if self.isChecked():
            color = QtGui.QColor(255, 0, 0)
        p.fillRect(self.rect(), color)


class CrossbarRouter(QtGui.QWidget):
    def __init__(self, servername, basepath, parent=None):
        QtGui.QWidget.__init__(self, parent);
        self.bus = dbus.SessionBus()
        self.dev = self.bus.get_object(servername, basepath)
        self.interface = dbus.Interface(self.dev, dbus_interface="org.ffado.Control.Element.CrossbarRouter")

        sources = self.interface.getSourceNames()
        self.sources = []
        for source in sources:
            self.sources.append(str(source))
        destinations = self.interface.getDestinationNames()
        self.destinations = []
        for dest in destinations:
            self.destinations.append(str(dest))

        print "Available sources (%i=?%i) %s" % (self.interface.getNbSources(), len(self.sources), str(self.sources))
        print "Available destinations (%i=?%i) %s" % (self.interface.getNbDestinations(), len(self.destinations), str(self.destinations))

        self.innames = []
        self.ingroups = {}
        for ch in self.sources:
            tmp = ch.split(":")[0]
            if not tmp in self.ingroups:
                self.ingroups[tmp] = 0
                self.innames.append(tmp)
            self.ingroups[tmp] = self.ingroups[tmp] + 1
        print self.ingroups
        self.outnames = []
        self.outgroups = {}
        for ch in self.destinations:
            tmp = ch.split(":")[0]
            if not tmp in self.outgroups:
                self.outgroups[tmp] = 0
                self.outnames.append(tmp)
            self.outgroups[tmp] = self.outgroups[tmp] + 1
        print self.outgroups

        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)

        tmp = 1
        for group in self.innames:
            lbl = QtGui.QLabel(group, self)
            self.layout.addWidget(lbl, 0, tmp + self.innames.index(group), 1, self.ingroups[group])
            tmp += self.ingroups[group]
        tmp = 1
        for group in self.outnames:
            lbl = QtGui.QLabel(group, self)
            self.layout.addWidget(lbl, tmp + self.outnames.index(group), 0, self.outgroups[group], 1)
            tmp += self.outgroups[group]
        #for i in range(len(self.sources)):
        #    lbl = QtGui.QLabel(self.sources[i], self)
        #    self.layout.addWidget(lbl, 0, i+1)
        #for i in range(len(self.destinations)):
        #    lbl = QtGui.QLabel(self.destinations[i], self)
        #    self.layout.addWidget(lbl, i+1, 0)

        for i in range(0, min(600, len(self.sources))):
            print "Checking connections for source %s" % self.sources[i]
            for j in range(0, min(600, len(self.destinations))):
                if self.interface.canConnect(i, j) and self.sources[i].split(":")[0] != self.destinations[j].split(":")[0]:
                    #checkbox = QtGui.QPushButton("%s->%s" % (self.sources[i], self.destinations[j]), self)
                    checkbox = CrossbarState("%s->%s" % (self.sources[i], self.destinations[j]), self)
                    checkbox.setCheckable(True)
                    checkbox.setChecked(self.interface.getConnectionState(i, j))
                    #self.layout.addWidget(checkbox, j, i)
                    self.layout.addWidget(checkbox, j+1, i+1)
#
# vim: sw=4 ts=4 et
