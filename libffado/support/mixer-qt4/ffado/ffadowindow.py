#!/usr/bin/python
#
# Copyright (C) 2005-2008 by Pieter Palmers
#               2007-2009 by Arnold Krille
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# FFADO is based upon FreeBoB.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import os

from ffado.config import *

import subprocess

from PyQt4.QtCore import SIGNAL, SLOT, QObject, QTimer, Qt
from PyQt4.QtGui import *

from ffado.dbus_util import *

from ffado.panelmanager import PanelManager

from ffado.logginghandler import *

"""Just a small helper to ask the retry-question without a blocking messagebox"""
class StartDialog(QWidget):
    def __init__(self, parent):
        QWidget.__init__(self, parent)
        self.setObjectName("Restart Dialog")
        self.label = QLabel("<qt>Somehow the connection to the dbus-service of FFADO couldn't be established.<p>\nShall we take another try?</qt>",self)
        self.button = QPushButton("Retry", self)
        self.layout = QGridLayout(self)
        self.layout.setContentsMargins( 50, 10, 50, 10 )
        self.layout.addWidget(self.label, 0, 0, Qt.AlignHCenter|Qt.AlignBottom)
        self.layout.addWidget(self.button, 1, 0, Qt.AlignHCenter|Qt.AlignTop)

class FFADOWindow(QMainWindow):
    def __init__(self, parent):
        QMainWindow.__init__(self, parent)

        self.textlogger = QTextLogger(self)
        dock = QDockWidget("Log Messages",self)
        dock.setWidget(self.textlogger.textedit)
        logging.getLogger('').addHandler(self.textlogger)
        self.addDockWidget(Qt.BottomDockWidgetArea, dock)

        self.statuslogger = QStatusLogger(self, self.statusBar(), 20)
        logging.getLogger('').addHandler(self.statuslogger)

        self.manager = PanelManager(self)
        self.connect(self.manager, SIGNAL("connectionLost"), self.connectToDBUS)

        filemenu = self.menuBar().addMenu("File")
        quitaction = QAction("Quit", self)
        quitaction.setShortcut(self.tr("Ctrl+q"))
        self.connect(quitaction, SIGNAL("triggered()"), self, SLOT("close()"))
        filemenu.addAction(quitaction)

        editmenu = self.menuBar().addMenu("Edit")
        self.updateaction = QAction("Update Mixer Panels", self)
        self.updateaction.setEnabled(False)
        self.connect(self.updateaction, SIGNAL("triggered()"), self.manager.updatePanels)
        editmenu.addAction(self.updateaction)

        helpmenu = self.menuBar().addMenu( "Help" )
        aboutaction = QAction( "About FFADO", self )
        self.connect( aboutaction, SIGNAL( "triggered()" ), self.aboutFFADO )
        helpmenu.addAction( aboutaction )
        aboutqtaction = QAction( "About Qt", self )
        self.connect( aboutqtaction, SIGNAL( "triggered()" ), qApp, SLOT( "aboutQt()" ) )
        helpmenu.addAction( aboutqtaction )

        log.info( "Starting up" )

        QTimer.singleShot( 1, self.tryStartDBUSServer )

    def __del__(self):
        log.info("__del__")
        del self.manager
        log.info("__del__ finished")

    def closeEvent(self, event):
        log.info("closeEvent()")
        event.accept()

    def connectToDBUS(self):
        log.info("connectToDBUS")
        try:
            self.setupDeviceManager()
        except dbus.DBusException, ex:
            log.error("Could not communicate with the FFADO DBus service...")
            if not hasattr(self,"retry"):
                self.retry = StartDialog(self)
                self.connect(self.retry.button, SIGNAL("clicked()"), self.tryStartDBUSServer)
            if hasattr(self, "retry"):
                self.manager.setParent(None)
                self.setCentralWidget(self.retry)
                self.retry.setEnabled(True)

    def tryStartDBUSServer(self):
        try:
            self.setupDeviceManager()
        except dbus.DBusException, ex:
            if hasattr(self, "retry"):
                self.retry.setEnabled(False)
            subprocess.Popen(['ffado-dbus-server', '-v3']).pid
            QTimer.singleShot(5000, self.connectToDBUS)

    def setupDeviceManager(self):
        devmgr = DeviceManagerInterface(FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH)
        self.manager.setManager(devmgr)
        if hasattr(self, "retry"):
            self.retry.setParent(None)
        self.setCentralWidget(self.manager)
        self.updateaction.setEnabled(True)

    def aboutFFADO(self):
        QMessageBox.about( self, "About FFADO", """
<h1>ffado.org</h1>

<p>FFADO is the new approach to have firewire audio on linux.</p>

<p>&copy; 2006-2008 by the FFADO developers<br />ffado is licensed under the GPLv3, for the full license text see <a href="http://www.gnu.org/licenses/">www.gnu.org/licenses</a> or the LICENSE.* files shipped with ffado.</p>

<p>FFADO developers are:<ul>
<li>Pieter Palmers
<li>Daniel Wagner
<li>Jonathan Woithe
<li>Arnold Krille
</ul>
""" )


def ffadomain(args):
    #set up logging
    import logging
    logging.basicConfig( datefmt="%H:%M:%S", format="%(asctime)s %(name)-16s %(levelname)-8s %(message)s" )

    if DEBUG:
        debug_level = logging.DEBUG
    else:
        debug_level = logging.INFO

    # main loggers:
    logging.getLogger('main').setLevel(debug_level)
    logging.getLogger('dbus').setLevel(debug_level)
    logging.getLogger('registration').setLevel(debug_level)
    logging.getLogger('panelmanager').setLevel(debug_level)
    logging.getLogger('configparser').setLevel(logging.INFO)

    # widgets:
    logging.getLogger('matrixmixer').setLevel(debug_level)
    logging.getLogger('crossbarrouter').setLevel(debug_level)

    # mixers:
    logging.getLogger('audiofire').setLevel(debug_level)
    logging.getLogger('bridgeco').setLevel(debug_level)
    logging.getLogger('edirolfa101').setLevel(debug_level)
    logging.getLogger('edirolfa66').setLevel(debug_level)
    logging.getLogger('motu').setLevel(debug_level)
    logging.getLogger('rme').setLevel(debug_level)
    logging.getLogger('phase24').setLevel(debug_level)
    logging.getLogger('phase88').setLevel(debug_level)
    logging.getLogger('quatafire').setLevel(debug_level)
    logging.getLogger('saffirebase').setLevel(debug_level)
    logging.getLogger('saffire').setLevel(debug_level)
    logging.getLogger('saffirepro').setLevel(debug_level)

    logging.getLogger('global').setLevel(debug_level)

    log = logging.getLogger('main')

    app = QApplication(args)
    app.setWindowIcon( QIcon( SHAREDIR + "/icons/hi64-apps-ffado.png" ) )

    app.setOrganizationName("FFADO")
    app.setOrganizationDomain("ffado.org")
    app.setApplicationName("ffado-mixer")

    mainwindow = FFADOWindow(None)


    # rock & roll
    mainwindow.show()
    return app.exec_()

if __name__ == "__main__":
    import sys
    sys.exit(ffadomain(sys.argv))

#
# vim: ts=4 sw=4 et
