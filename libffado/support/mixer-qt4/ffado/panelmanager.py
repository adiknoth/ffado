#
# Copyright (C) 2005-2008 by Pieter Palmers
#               2007-2008 by Arnold Krille
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

from ffado.config import * #FFADO_VERSION, FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH

from PyQt4.QtGui import QFrame, QWidget, QTabWidget, QVBoxLayout, QMainWindow, QIcon, QAction, qApp, QStyleOptionTabWidgetFrame
from PyQt4.QtCore import QTimer

from ffado.dbus_util import *
from ffado.registration import *

from ffado.configuration import *

from ffado.mixer.globalmixer import GlobalMixer
from ffado.mixer.dummy import Dummy

import time

import logging
log = logging.getLogger('panelmanager')

use_generic = False
try:
    from mixer_generic import *
    log.info("The generic mixer is found, seems to be a developer using ffadomixer...")
except ImportError:
    pass
else:
    use_generic = True

# pseudo-guid
GUID_GENERIC_MIXER = 0


class HLine( QFrame ):
    def __init__( self, parent ):
        QFrame.__init__( self, parent )
        self.setFrameShape( QFrame.HLine )
        self.setLineWidth( 2 )
        self.setMinimumHeight( 10 )

class PanelManagerStatus(QWidget):
    def __init__(self, parent):
        QWidget.__init__(self,parent)
        uicLoad("ffado/panelmanagerstatus", self)

class OwnTabWidget(QTabWidget):
    def __init__(self,parent):
        QTabWidget.__init__(self,parent)

    def tabInserted(self,index):
        self.checkTabBar()

    def tabRemoved(self,index):
        self.checkTabBar()

    def checkTabBar(self):
        if self.count()<2:
            self.tabBar().hide()
        else:
            self.tabBar().show()

class PanelManager(QWidget):
    def __init__(self, parent, devmgr=None):
        QWidget.__init__(self, parent)
        self.setObjectName("PanelManager")

        # maps a device GUID to a QT panel
        self.panels = {}

        # a layout for ourselves
        self.layout = QVBoxLayout(self)

        # the tabs
        self.tabs = OwnTabWidget(self)
        self.tabs.hide()
        self.layout.addWidget(self.tabs)

        # a dialog that is shown during update
        self.status = PanelManagerStatus(self)
        self.layout.addWidget(self.status)
        self.status.show()

        self.devices = DeviceList( SYSTEM_CONFIG_FILE )
        self.devices.updateFromFile( USER_CONFIG_FILE )

        if devmgr is not None:
            self.setManager(devmgr)

    def __del__(self):
        print("PanelManager.__del__()")
        self.polltimer.stop()

    def setManager(self,devmgr):
        self.devmgr = devmgr
        self.devmgr.registerPreUpdateCallback(self.devlistPreUpdate)
        self.devmgr.registerPostUpdateCallback(self.devlistPostUpdate)
        self.devmgr.registerUpdateCallback(self.devlistUpdate)
        self.devmgr.registerDestroyedCallback(self.devmgrDestroyed)
        # create a timer to poll the panels
        self.polltimer = QTimer()
        self.connect( self.polltimer, SIGNAL('timeout()'), self.pollPanels )
        self.polltimer.start( POLL_SLEEP_TIME_MSEC )

        # create a timer to initialize the panel after the main form is shown
        # since initialization can take a while
        QTimer.singleShot( POLL_SLEEP_TIME_MSEC, self.updatePanels )

        # live check timer
        self.alivetimer = QTimer()
        QObject.connect( self.alivetimer, SIGNAL('timeout()'), self.commCheck )
        self.alivetimer.start( 2000 )

    def count(self):
        return self.tabs.count()

    def pollPanels(self):
        #log.debug("PanelManager::pollPanels()")
        # only when not modifying the tabs
        try:
            if self.tabs.isEnabled():
                for guid in self.panels.keys():
                    w = self.panels[guid]
                    for child in w.children():
                        #log.debug("poll child %s,%s" % (guid,child))
                        if 'polledUpdate' in dir(child):
                            child.polledUpdate()
        except:
            log.error("error in pollPanels")
            self.commCheck()

    def devlistPreUpdate(self):
        log.debug("devlistPreUpdate")
        self.tabs.setEnabled(False)
        self.tabs.hide()
        self.status.lblMessage.setText("Bus reconfiguration in progress, please wait...")
        self.status.show()
        #self.statusBar().showMessage("bus reconfiguration in progress...", 5000)

    def devlistPostUpdate(self):
        log.debug("devlistPostUpdate")
        # this can fail if multiple busresets happen in fast succession
        ntries = 10
        while ntries > 0:
            try:
                self.updatePanels()
                return
            except:
                log.debug("devlistPostUpdate failed (%d)" % ntries)
                for guid in self.panels.keys():
                    w = self.panels[guid]
                    del self.panels[guid] # remove from the list
                    idx = self.tabs.indexOf(w)
                    self.tabs.removeTab(idx)
                    del w # GC might also take care of that

                ntries = ntries - 1
                time.sleep(2) # sleep a few seconds

        log.debug("devlistPostUpdate failed completely")
        self.tabs.setEnabled(False)
        self.tabs.hide()
        self.status.lblMessage.setText("Error while reconfiguring. Please restart ffadomixer.")
        self.status.show()


    def devlistUpdate(self):
        log.debug("devlistUpdate")

    def devmgrDestroyed(self):
        log.debug("devmgrDestroyed")
        self.alivetimer.stop()
        self.tabs.setEnabled(False)
        self.tabs.hide()
        self.status.lblMessage.setText("DBUS server was shut down, please restart it and restart ffadomixer...")
        self.status.show()

    def commCheck(self):
        try:
            nbDevices = self.devmgr.getNbDevices()
        except:
            log.error("The communication with ffado-dbus-server was lost.")
            self.tabs.setEnabled(False)
            self.polltimer.stop()
            self.alivetimer.stop()
            keys = self.panels.keys()
            for panel in keys:
                w = self.panels[panel]
                del self.panels[panel]
                w.deleteLater()
            self.emit(SIGNAL("connectionLost"))

    def updatePanels(self):
        log.debug("PanelManager::updatePanels()")
        nbDevices = self.devmgr.getNbDevices()
        #self.statusBar().showMessage("Reconfiguring the mixer panels...")

        # list of panels present
        guids_with_tabs = self.panels.keys()

        # build list of guids on the bus now
        guids_present = []
        guid_indexes = {}
        for idx in range(nbDevices):
            path = self.devmgr.getDeviceName(idx)
            cfgrom = ConfigRomInterface(FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+'/DeviceManager/'+path)
            guid = cfgrom.getGUID()
            guids_present.append(guid)
            guid_indexes[guid] = idx

        # figure out what to remove
        # the special panel (generic)
        # that has (pseudo-)GUID 0
        # is also automatically removed
        to_remove = []
        for guid in guids_with_tabs:
            if not guid in guids_present:
                to_remove.append(guid)
                log.debug("going to remove %s" % str(guid))
            else:
                log.debug("going to keep %s" % str(guid))

        # figure out what to add
        to_add = []
        for guid in guids_present:
            if not guid in guids_with_tabs:
                to_add.append(guid)
                log.debug("going to add %s" % str(guid))

        # update the widget
        for guid in to_remove:
            print "Removing widget for device" + guid
            w = self.panels[guid]
            del self.panels[guid] # remove from the list
            idx = self.tabs.indexOf(w)
            self.tabs.removeTab(idx)
            w.deleteLater()

        for guid in to_add:
            # retrieve the device manager index
            idx = guid_indexes[guid]
            path = self.devmgr.getDeviceName(idx)
            log.debug("Adding device %d: %s" % (idx, path))

            cfgrom = ConfigRomInterface(FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+'/DeviceManager/'+path)
            vendorId = cfgrom.getVendorId()
            modelId = cfgrom.getModelId()
            unitVersion = cfgrom.getUnitVersion()
            guid = cfgrom.getGUID()
            vendorName = cfgrom.getVendorName()
            modelName = cfgrom.getModelName()
            log.debug(" Found (%s, %X, %X) %s %s" % (str(guid), vendorId, modelId, vendorName, modelName))

            # check whether this has already been registered at ffado.org
            reg = ffado_registration(FFADO_VERSION, int(guid, 16),
                                     vendorId, modelId,
                                     vendorName, modelName)
            reg.check_for_registration()

            # The MOTU devices use unitVersion to differentiate models.  For the
            # moment though we don't need to know precisely which model we're
            # using.
            if vendorId == 0x1f2:
                modelId = 0x00000000

            # The RME devices use unitVersion to differentiate models. 
            # Therefore in the configuration file we use the config file's
            # modelid field to store the unit version.  As a result we must
            # override the modelId with the unit version here so the correct
            # configuration file entry (and hense mixer widget) is identified.
            if vendorId == 0xa35:
                modelId = unitVersion;

            dev = self.devices.getDeviceById( vendorId, modelId )

            w = QWidget( )
            l = QVBoxLayout( w )

            # create a control object
            hw = ControlInterface(FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+'/DeviceManager/'+path)
            clockselect = ClockSelectInterface( FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+"/DeviceManager/"+path )
            samplerateselect = SamplerateSelectInterface( FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+"/DeviceManager/"+path )
            streamingstatus = StreamingStatusInterface( FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+"/DeviceManager/"+path )
            nickname = TextInterface( FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+"/DeviceManager/"+path+"/Generic/Nickname" )

            #
            # Generic elements for all mixers follow here:
            #
            globalmixer = GlobalMixer( w )
            globalmixer.configrom = cfgrom
            globalmixer.clockselect = clockselect
            globalmixer.samplerateselect = samplerateselect
            globalmixer.streamingstatus = streamingstatus
            globalmixer.nickname = nickname
            globalmixer.hw = hw
            globalmixer.initValues()
            l.addWidget( globalmixer, 1 )

            #
            # Line to separate
            #
            l.addWidget( HLine( w ) )

            #
            # Specific (or dummy) mixer widgets get loaded in the following
            #
            if 'mixer' in dev and dev['mixer'] != None:
                mixerapp = dev['mixer']
                exec( """
import ffado.mixer.%s
mixerwidget = ffado.mixer.%s.%s( w )
""" % (mixerapp.lower(), mixerapp.lower(), mixerapp) )
            else:
                mixerwidget = Dummy( w )
                mixerapp = modelName+" (Dummy)"

            #
            # The same for all mixers
            #
            l.addWidget( mixerwidget, 10 )
            mixerwidget.configrom = cfgrom
            mixerwidget.clockselect = clockselect
            mixerwidget.samplerateselect = samplerateselect
            mixerwidget.streamingstatus = streamingstatus
            mixerwidget.nickname = nickname
            mixerwidget.hw = hw
            if 'buildMixer' in dir(mixerwidget):
                mixerwidget.buildMixer()
            if 'initValues' in dir(mixerwidget):
                mixerwidget.initValues()
            if 'getDisplayTitle' in dir(mixerwidget):
                title = mixerwidget.getDisplayTitle()
            else:
                title = mixerapp

            globalmixer.setName(title)
            self.tabs.addTab( w, title )
            self.panels[guid] = w

        # if there is no panel, add the no-device message
        # else make sure it is not present
        if self.count() == 0:
            self.tabs.hide()
            self.tabs.setEnabled(False)
            self.status.lblMessage.setText("No supported device found.")
            self.status.show()
            #self.statusBar().showMessage("No supported device found.", 5000)
        else:
            self.tabs.show()
            self.tabs.setEnabled(True)
            self.status.hide()
            #self.statusBar().showMessage("Configured the mixer for %i devices." % self.tabs.count())
            if use_generic:
                #
                # Show the generic (development) mixer if it is available
                #
                w = GenericMixer( devmgr.bus, FFADO_DBUS_SERVER, mw )
                self.tabs.addTab( w, "Generic Mixer" )
                self.panels[GUID_GENERIC_MIXER] = w

# vim: et
