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

class OutputSwitcher(QtGui.QFrame):
    """
The name is a bit misleading. This widget selectes sources for a specified
destination.

In mixer-usage this widget is at the top of the input-channel. Because the input
of the mixer is an available output from the routers point.
"""
    def __init__(self, interface, outname, parent):
        QtGui.QFrame.__init__(self, parent)
        self.interface = interface
        self.outname = outname

        self.setLineWidth(1)

        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)

        self.lbl = QtGui.QLabel(self.outname, self)
        self.layout.addWidget(self.lbl, 0, 0, 1, 2)

        self.btn = QtGui.QPushButton("Sel.", self)
        self.layout.addWidget(self.btn, 1, 0)

        self.exclusiveGroup = QtGui.QActionGroup(self)

        sources = self.interface.getSourceNames()
        self.ingroups = {}
        for ch in sources:
            tmp = str(ch).split(":")[0]
            if tmp not in self.ingroups:
                self.ingroups[tmp] = 0
            self.ingroups[tmp] += 1
        #print "Detected ingroups: %s" % str(self.ingroups)

        self.menu = QtGui.QMenu(self)
        self.btn.setMenu(self.menu)

        for group in self.ingroups:
            submenu = InGroupMenu(self.interface, self.outname, group, self.ingroups[group], self, self.exclusiveGroup)
            self.menu.addMenu(submenu)

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

        destinations = self.interface.getDestinationNames()
        self.outgroups = []
        for ch in destinations:
            tmp = str(ch).split(":")[0]
            if not tmp in self.outgroups:
                self.outgroups.append(tmp)

        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)

        for out in destinations:
            btn = OutputSwitcher(self.interface, out, self)
            self.layout.addWidget(btn, int(out.split(":")[-1]), self.outgroups.index(out.split(":")[0]))

#
# vim: sw=4 ts=4 et
