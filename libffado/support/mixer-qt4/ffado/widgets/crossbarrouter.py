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

class InGroupMenu(QtGui.QMenu):
    def __init__(self, interface, outname, inname, insize, parent, exclusiveGroup = None):
        QtGui.QMenu.__init__(self, inname, parent)
        self.interface = interface
        self.outname = str(outname)
        self.inname = str(inname)
        self.insize = insize
        self.connect(self, QtCore.SIGNAL("aboutToShow()"), self.showMenu)

        self.lock = False
        if exclusiveGroup:
            self.exclusiveGroup = exclusiveGroup
        else:
            self.exclusiveGroup = QtGui.QActionGroup(self)

    def showMenu(self):
        #print "About to show the menu"
        if len(self.actions()) < self.insize:
            # Do a lazy init of the sub-items.
            for i in range(self.insize):
                action = QtGui.QAction("%s:%02i" % (self.inname,i), self)
                action.setCheckable(True)
                self.connect(action, QtCore.SIGNAL("toggled(bool)"), self.connectionSwitched)
                self.exclusiveGroup.addAction(action)
                self.addAction(action)
                idx = self.interface.getDestinationIndex(self.outname)
                sourceidx = self.interface.getSourceForDestination(idx)
                #print self.interface.getConnectionState(sourceidx, idx)
                source = self.interface.getSourceName(sourceidx)
                self.lock = True
                for action in self.actions():
                    action.setChecked(action.text() == source)
                self.lock = False

    def connectionSwitched(self, checked):
        if self.lock: return
        print "connectionSwitched( %s ) sender: %s" % (str(checked),str(self.sender()))
        inname = str(self.sender().text())
        print " source=%s destination=%s  possible? %s" % (inname, self.outname, self.interface.canConnectNamed(inname, self.outname))
        print " connectionState is now %s" % self.interface.setConnectionStateNamed(inname, self.outname, checked)


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
            if True: #tmp[0] == "M" or tmp[0] == "I":
                if not tmp in self.ingroups:
                    self.ingroups[tmp] = 0
                    self.innames.append(tmp)
                self.ingroups[tmp] = self.ingroups[tmp] + 1
        print self.ingroups
        self.outnames = []
        self.outgroups = {}
        for ch in self.destinations:
            tmp = ch.split(":")[0]
            if True: #tmp == "MixerIn":
                if not tmp in self.outgroups:
                    self.outgroups[tmp] = 0
                    self.outnames.append(tmp)
                self.outgroups[tmp] = self.outgroups[tmp] + 1
        print self.outgroups

        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)

        for group in self.outgroups.keys():
            for i in range(self.outgroups[group]):
                outname = "%s:%02i" % (group,i)
                #print "Creating buttons for %s" % outname
                btn = QtGui.QPushButton("%s" % outname, self)
                outidx = self.destinations.index(outname)
                self.layout.addWidget(btn, i, self.outnames.index(group))
                menu = QtGui.QMenu(self)
                btn.setMenu(menu)
                exclusiveGroup = QtGui.QActionGroup(btn)
                for x in self.ingroups:
                    submenu = InGroupMenu(self.interface, outname, x, self.ingroups[x], self, exclusiveGroup)
                    menu.addMenu(submenu)

#
# vim: sw=4 ts=4 et
