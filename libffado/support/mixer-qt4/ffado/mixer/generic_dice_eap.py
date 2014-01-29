#
# Copyright (C) 2009-2010 by Arnold Krille
#               2013 by Philippe Carriere
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

class Generic_Dice_EAP(QtGui.QWidget):
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.layout = QtGui.QGridLayout(self)
        self.setLayout(self.layout)
        self.tabs = QtGui.QTabWidget(self)
        self.tabs.setTabPosition(QtGui.QTabWidget.West)
        self.layout.addWidget(self.tabs)

    def buildMixer(self):
        #print self.hw
        #print self.hw.getText("/Generic/Nickname")
        self.mixer = MatrixMixer(self.hw.servername, self.hw.basepath+"/EAP/MatrixMixer", self, "Columns_are_outputs", -1, None, None, False, QtGui.QTabWidget.North, QtGui.QTabWidget.Rounded)
        self.tabs.addTab(self.mixer, "Mixer")

        self.router_scrollarea = self.buildRouter(self.hw.servername, self.hw.basepath+"/EAP/Router")
        self.tabs.addTab(self.router_scrollarea, "Crossbar Router")

    def buildRouter(self, servername, path):
        self.router = CrossbarRouter(servername, path, self)
        self.connect(self.router, QtCore.SIGNAL("MixerRoutingChanged"), self.mixer.updateRouting)
        scrollarea = QtGui.QScrollArea(self.tabs)
        scrollarea.setWidgetResizable(True)
        scrollarea.setWidget(self.router)
        return scrollarea

    def onSamplerateChange(self):
        # Router configuration is samplerate dependent for DICE EAP devices
        # Destroy and redraw the crossbar router view when called
        n = self.tabs.indexOf(self.router_scrollarea)
        self.tabs.removeTab(n)
        self.router.destroy()
        self.router_scrollarea.destroy()

        self.router_scrollarea = self.buildRouter(self.hw.servername, self.hw.basepath+"/EAP/Router")
        self.tabs.insertTab(n, self.router_scrollarea, "Crossbar Router")
        self.tabs.setCurrentWidget(self.router_scrollarea)

        self.mixer.updateRouting()

    def saveSettings(self, indent):
        saveString = []
        idt = indent + "  "
        saveString.append('%s<mixer>\n' % indent)
        saveString.extend(self.mixer.saveSettings(idt))
        # Do not forget to mention the adopted rule for matrix columns mixer
        #  This might be useful for future import function
        saveString.append("%s  <col_rule>\n" % indent)
        saveString.append("%s    Columns_are_outputs\n" % indent)
        saveString.append("%s  </col_rule>\n" % indent)
        saveString.append('%s</mixer>\n' % indent)
        saveString.append('%s<router>\n' % indent)
        saveString.extend(self.router.saveSettings(idt))
        saveString.append('%s</router>\n' % indent)
        return saveString

    def readSettings(self, readString):
        try:
            idxb = readString.index('<mixer>')
            idxe = readString.index('</mixer>')
        except Exception:
            log.debug("No mixer settings found")
            idxb = -1
            idxe = -1
        if idxb >= 0:
            if idxe > idxb + 1:
                stringMixer = []
                for s in readString[idxb+1:idxe]:
                    stringMixer.append(s)
                # When trying to import from a different device, the rule for column interpretation is
                # not necessarily the same
                try:
                    idx = stringMixer.index('<col_rule>')
                except Exception:
                    log.debug('Do not know how to handle column versus input/output')
                    idx = -1
                transpose_coeff = False
                if idx >=0:
                    if stringMixer[idx+1].find("Columns_are_outputs") == -1:
                        log.debug('Transposing the matrix coefficient; you are importing, are not you ?')
                        transpose_coeff = True
                if self.mixer.readSettings(stringMixer, transpose_coeff):
                    log.debug("Mixer settings modified")
                del stringMixer
        try:
            idxb = readString.index('<router>')
            idxe = readString.index('</router>')
        except Exception:
            log.debug("No router settings found")
            idxb = -1
            idxe = -1
        if idxb >= 0:
            if idxe > idxb + 1:
                stringRouter = []
                for s in readString[idxb+1:idxe]:
                    stringRouter.append(s)
                if self.router.readSettings(stringRouter):
                    log.debug("Router settings modified")
                del stringRouter

    #def getDisplayTitle(self):
    #    return "Saffire PRO40/PRO24 Mixer"

#
# vim: et ts=4 sw=4
