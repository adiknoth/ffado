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

from PyQt4.QtCore import SIGNAL, SLOT, QObject
from PyQt4.QtGui import QDialog
from ffado_regdialogui import Ui_ffadoRegDialogUI

REGISTRATION_MESSAGE = """
<html><head><meta name="qrichtext" content="1" />
<style type="text/css">\np, li { white-space: pre-wrap; }\n</style></head>
<body style=" font-family:'Sans Serif'; font-size:9pt; font-weight:400; font-style:normal;">\n
<p style=" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">
You are running this version of FFADO for the first time with this device. </p>
<p style=" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">
In order to collect usage statistics we would like to send some information about your system to ffado.org.
It is very important for us to have good usage statistics. This is to convince vendors that Linux users 
do exist and is where you as a user can help the project. </p>
<p style=" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">
The information collected is intended only for usage monitoring. The email address is optional, and will 
be used for FFADO related announcements only. If you provide one, please provide a valid one. </p>
<p style=" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">
Note: This registration can also be performed on-line at 
<a href="http://www.ffado.org/?q=usage">http://www.ffado.org/?q=usage</a>.</p></body></html>
"""

class ffadoRegDialog(QDialog, Ui_ffadoRegDialogUI):
    def __init__(self, vendor_name, vendor_id, model_name, model_id,
                 guid, version, email="(optional)",
                 parent = None):
        QDialog.__init__(self,parent)
        self.setupUi(self)
        self.txtVendorName.setText(vendor_name)
        self.txtVendorId.setText(vendor_id)
        self.txtModelName.setText(model_name)
        self.txtModelId.setText(model_id)
        self.txtGUID.setText(guid)
        self.txtVersion.setText(version)
        self.txtEmail.setText(email)
        self.txtMessage.setHtml(REGISTRATION_MESSAGE)

        self.choice = "nosend"
        QObject.connect(self.btnSend,SIGNAL('clicked()'),self.buttonPressed)
        QObject.connect(self.btnNoSend,SIGNAL('clicked()'),self.buttonPressed)
        QObject.connect(self.btnNeverSend,SIGNAL('clicked()'),self.buttonPressed)

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
