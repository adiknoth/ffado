#
# Copyright (C) 2005-2008 by Pieter Palmers
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# FFADO is based upon FreeBoB.
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

from qt import *
from ffado_regdialogui import *

class ffadoRegDialog(ffadoRegDialogUI):
    def __init__(self, devtext, vendormodel,
                 guid, version, email="(optional)",
                 parent = None,name = None,modal = 1,fl = 0):
        ffadoRegDialogUI.__init__(self,parent,name,modal,fl)

        self.txtDevice.setText(devtext)
        self.txtVendorModel.setText(vendormodel)
        self.txtGUID.setText("%016X" % guid)
        self.txtVersion.setText(version)
        self.txtEmail.setText(email)
        self.choice = "nosend"

    def buttonPressed(self):
        sender = self.sender()
        if sender == self.btnSend:
            print "user chose to send"
            self.choice = "send"
        elif sender ==  self.btnNoSend:
            print "user chose not to send"
            self.choice = "nosend"
        elif sender ==  self.btnNeverSend:
            print "user chose to never send"
            self.choice = "neversend"
        self.close()

    def getEmail(self):
        return self.txtEmail.text()

    def init(self):
        print "Init ffado reg window"
        self.choice = "nosend"
        QObject.connect(self.btnSend,SIGNAL('clicked()'),self.buttonPressed)
        QObject.connect(self.btnNoSend,SIGNAL('clicked()'),self.buttonPressed)
        QObject.connect(self.btnNeverSend,SIGNAL('clicked()'),self.buttonPressed)
