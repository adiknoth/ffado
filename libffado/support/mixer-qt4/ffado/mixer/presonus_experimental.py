#
# Copyright (C) 2010 by Arnold Krille <arnold@arnoldarts.de>
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# version 3 of the License.
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

from ffado.config import *

class Presonus_Experimental(QtGui.QWidget):
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        uicLoad('ffado/mixer/presonus_experimental', self)

    def buildMixer(self):
        #print self.hw
        #print 'Servername = "%s"\nBasename = "%s"' % (self.hw.servername, self.hw.basepath)
        bus = self.hw.bus
        self.ifaces = list()
        for feature in ['Volume_1', 'Volume_2', 'Volume_3', 'Volume_4', 'LRBalance_1', 'LRBalance_2', 'LRBalance_3', 'LRBalance_4']:
            dev = bus.get_object(self.hw.servername, self.hw.basepath+'/Mixer/Feature_%s' % feature)
            #print dev
            iface = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Continuous')
            self.ifaces.append(iface)
            #print iface
            #print iface.getValue(), iface.getMinimum(), iface.getMaximum()
            obj = self.findChild(QtGui.QWidget, feature)
            #print obj
            obj.setMinimum(iface.getMinimum())
            obj.setMaximum(iface.getMaximum())
            obj.setValue(iface.getValue())
            self.connect(obj, QtCore.SIGNAL("valueChanged(int)"), iface.setValue)

#
# vim: et ts=4 sw=4
