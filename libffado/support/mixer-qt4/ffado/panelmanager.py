#
# Copyright (C) 2005-2008 by Pieter Palmers
#               2007-2008 by Arnold Krille
#               2013 by Philippe Carriere
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

from PyQt4.QtGui import QFrame, QWidget, QTabWidget, QVBoxLayout, QMainWindow, QIcon, QAction, qApp, QStyleOptionTabWidgetFrame, QFileDialog
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

FILE_VERSION = '0.1'

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
        self.parent = parent

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

    def removePanel(self, guid):
        print "Removing widget for device" + guid
        w = self.panels[guid]
        del self.panels[guid] # remove from the list
        idx = self.tabs.indexOf(w)
        self.tabs.removeTab(idx)
        w.deleteLater()

    def addPanel(self, idx):
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
        # using beyond it being a pre-mark3 (modelId=0) or mark3 (modelId=1) 
        # device.
        if vendorId == 0x1f2:
            # All MOTU devices with a unit version of 0x15 or greater are
            # mark3 devices
            if (unitVersion >= 0x15):
                modelId = 0x00000001
            else:
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

        if 'onSamplerateChange' in dir(mixerwidget):
          log.debug("Updating Mixer on samplerate change required")
          globalmixer.onSamplerateChange = mixerwidget.onSamplerateChange

        w.gmixSaveSetgs = globalmixer.saveSettings
        w.gmixReadSetgs = globalmixer.readSettings
        if 'saveSettings' in dir(mixerwidget):
          w.smixSaveSetgs = mixerwidget.saveSettings
          self.parent.saveaction.setEnabled(True)

        if 'readSettings' in dir(mixerwidget):
          w.smixReadSetgs = mixerwidget.readSettings
          self.parent.openaction.setEnabled(True)

    def displayPanels(self):
        # if there is no panel, add the no-device message
        # else make sure it is not present
        if self.count() == 0:
            self.tabs.hide()
            self.tabs.setEnabled(False)
            self.status.lblMessage.setText("No supported device found.")
            self.status.show()
            #self.statusBar().showMessage("No supported device found.", 5000)
        else:
            # Hide the status widget before showing the panel tab to prevent
            # the panel manager's vertical size including that of the status
            # widget.  For some reason, hiding the status after showing the
            # tabs does not cause a recalculation of the panel manager's size,
            # and the window ends up being larger than it needs to be.
            self.status.hide()
            self.tabs.show()
            self.tabs.setEnabled(True)
            #self.statusBar().showMessage("Configured the mixer for %i devices." % self.tabs.count())
            if use_generic:
                #
                # Show the generic (development) mixer if it is available
                #
                w = GenericMixer( devmgr.bus, FFADO_DBUS_SERVER, mw )
                self.tabs.addTab( w, "Generic Mixer" )
                self.panels[GUID_GENERIC_MIXER] = w
    
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
            self.removePanel(guid)

        for guid in to_add:
            # retrieve the device manager index
            idx = guid_indexes[guid]
            self.addPanel(idx)

        self.displayPanels()

    def refreshPanels(self):
        log.debug("PanelManager::refreshPanels()")
        nbDevices = self.devmgr.getNbDevices()
        #self.statusBar().showMessage("Reconfiguring the mixer panels...")

        # list of panels present
        guids_with_tabs = self.panels.keys()

        # build list of guids on the bus now
        guid_indexes = {}
        for idx in range(nbDevices):
            path = self.devmgr.getDeviceName(idx)
            cfgrom = ConfigRomInterface(FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+'/DeviceManager/'+path)
            guid = cfgrom.getGUID()
            guid_indexes[guid] = idx

        # remove/create the widget
        for guid in guids_with_tabs:
            self.removePanel(guid)
            idx = guid_indexes[guid]
            self.addPanel(idx)

        self.displayPanels()

    def saveSettings(self):
        saveString = []
        saveString.append('<?xml version="1.0" encoding="UTF-8"?>\n')
        saveString.append('<fileversion>\n')
        saveString.append('  <major>\n')
        saveString.append('    ' + str(FILE_VERSION).split('.')[0] + '\n')
        saveString.append('  </major>\n')
        saveString.append('  <minor>\n')
        saveString.append('    ' + str(FILE_VERSION).split('.')[1] + '\n')
        saveString.append('  </minor>\n')
        saveString.append('</fileversion>\n')
        saveString.append('<ffadoversion>\n')
        saveString.append('  <major>\n')
        saveString.append('    ' + str(str(FFADO_VERSION).split('-')[0]).split('.')[0] + '\n')
        saveString.append('  </major>\n')
        saveString.append('  <minor>\n')
        saveString.append('    ' + str(str(FFADO_VERSION).split('-')[0]).split('.')[1] + '\n')
        saveString.append('  </minor>\n')
        saveString.append('</ffadoversion>\n')
        for guid in self.panels.keys():
          saveString.append('<device>\n')
          saveString.append('  <guid>\n')
          saveString.append('    ' + str(guid) + '\n')
          saveString.append('  </guid>\n')        
          w = self.panels[guid]
          indent = "  "
          saveString.extend(w.gmixSaveSetgs(indent))
          if 'smixSaveSetgs' in dir(w):
              saveString.extend(w.smixSaveSetgs(indent))
          saveString.append('</device>\n')
        # file saving
        savefilename = QFileDialog.getSaveFileName(self, 'Save File', os.getenv('HOME'))
        try:
          f = open(savefilename, 'w')
        except IOError:
          print "Unable to open save file"
          return
        for s in saveString:
          f.write(s)
        f.close()

    def readSettings(self):
        readfilename = QFileDialog.getOpenFileName(self, 'Open File', os.getenv('HOME'))
        try:
          f = open(readfilename, 'r')
        except IOError:
          print "Unable to open file"
          return
        log.debug("Opening file %s" % readfilename)
        # discard useless whitespace characters
        readString = []
        for line in f:
          readString.append(" ".join(str(line).split()))
        f.close()
        # Check it is a compatible "FFADO" file
        # It must start with the <?xml ... tag as the first string
        if readString[0].find("<?xml") == -1:
            print "Not an xml data file"
            return
        # Then there must be a file version tag somewhere in the file
        try:
            idx = readString.index('<fileversion>')
        except Exception:
            print "Data file should contain the version tag"
            return
        if readString[idx+1].find("<major>") == -1:
            print "Incompatible versioning of the file"
        if readString[idx+3].find("</major>") == -1:
            print "Not a valid xml file"
        if readString[idx+4].find("<minor>") == -1:
            print "Incompatible versioning of the file"
        if readString[idx+6].find("</minor>") == -1:
            print "Not a valid xml file"
        version_major = readString[idx+2]
        version =  version_major + '.' + readString[idx+5]
        log.debug("File version: %s" % version)
        # File version newer than present
        if int(version_major) > int(str(FILE_VERSION).split('.')[0]):
            print "File version is too recent: you should upgrade your FFADO installation"
            return
        # FIXME At a time it will be necessary to detect if an older major version is detected
        #
        # It looks like useless to check for the FFADO version
        # Add something here if you would like so
        #
        # Now search for devices
        nd = readString.count('<device>');
        n  = readString.count('</device>');
        if n != nd:
            print "Not a regular xml file: opening device tag must match closing ones"
            return
        while nd > 0:
          idxb = readString.index('<device>')
          idxe = readString.index('</device>')
          if idxe < idxb+1:
            print "Not a regular xml file: data must be enclosed between a <device> and </device> tag"
            return
          stringDev = []
          for s in readString[idxb:idxe]:
            stringDev.append(s)
          # determine the device guid
          try:
              idx = stringDev.index('<guid>')
          except Exception:
              print "Device guid not found"
              return
          guid = stringDev[idx+1]
          log.debug("Device %s found" % guid)

          if guid in self.panels:
              w = self.panels[guid]
              w.gmixReadSetgs(stringDev)
              if 'smixReadSetgs' in dir(w):
                w.smixReadSetgs(stringDev)
              log.debug("Settings changed for device %s" % guid)
          else:
              log.debug("Device %s not present; settings ignored" % guid)

          del stringDev[:]
          del readString[idxb:idxe]
          nd -= 1
   
# vim: et
