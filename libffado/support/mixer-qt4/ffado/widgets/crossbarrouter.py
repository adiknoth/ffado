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

        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)

        #for i in range(len(self.sources)):
        #    lbl = QtGui.QLabel(self.sources[i], self)
        #    self.layout.addWidget(lbl, 0, i+1)
        #for i in range(len(self.destinations)):
        #    lbl = QtGui.QLabel(self.destinations[i], self)
        #    self.layout.addWidget(lbl, i+1, 0)

        for i in range(0, len(self.sources)):
            print "Checking connections for source %s" % self.sources[i]
            for j in range(0, len(self.destinations)):
                if self.interface.canConnect(i, j):
                    checkbox = QtGui.QPushButton("%s->%s" % (self.sources[i], self.destinations[j]), self)
                    checkbox.setCheckable(True)
                    checkbox.setChecked(self.interface.getConnectionState(i, j))
                    self.layout.addWidget(checkbox, j, i)
                    #self.layout.addWidget(checkbox, j+1, i+1)
#
# vim: sw=4 ts=4 et
