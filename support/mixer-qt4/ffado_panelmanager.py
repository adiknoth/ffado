#!/usr/bin/python
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
from ffadomixer_config import FFADO_VERSION, FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH

from PyQt4.QtGui import QFrame, QWidget, QTabWidget, QVBoxLayout
from PyQt4.QtCore import QTimer

from ffado_panelmanagerstatusui import Ui_PanelManagerStatusUI

from ffado_dbus_util import *
from ffado_registration import *

#from mixer_phase88 import *
#from mixer_phase24 import *
from mixer_saffire import SaffireMixer
from mixer_saffirepro import SaffireProMixer
from mixer_audiofire import AudioFireMixer
#from mixer_bcoaudio5 import *
#from mixer_edirolfa66 import *
#from mixer_mackie_generic import *
#from mixer_quatafire import *
#from mixer_motu import *
from mixer_dummy import *
from mixer_global import GlobalMixer

use_generic = False
try:
    from mixer_generic import *
    print "The generic mixer is found, seems to be a developer using ffadomixer..."
except ImportError:
    pass
else:
    use_generic = True

SupportedDevices=[
    #[(0x000aac, 0x00000003),'Phase88Control'],
    #[(0x000aac, 0x00000004),'Phase24Control'],
    #[(0x000aac, 0x00000007),'Phase24Control'],
    [(0x00130e, 0x00000003),'SaffireProMixer'],
    [(0x00130e, 0x00000006),'SaffireProMixer'],
    [(0x00130e, 0x00000000),'SaffireMixer'],
    [(0x001486, 0x00000af2),'AudioFireMixer'],
    [(0x001486, 0x00000af4),'AudioFireMixer'],
    [(0x001486, 0x00000af8),'AudioFireMixer'],
    [(0x001486, 0x0000af12),'AudioFireMixer'],
    #[(0x0007f5, 0x00010049),'BCoAudio5Control'],
    #[(0x0040AB, 0x00010049),'EdirolFa66Control'],
    #[(0x00000f, 0x00010067),'MackieGenericControl'],
    #[(0x000f1b, 0x00010064),'QuataFireMixer'],
    #[(0x0001f2, 0x00000000),'MotuMixer'],
    ]

# pseudo-guid
GUID_GENERIC_MIXER = 0


class HLine( QFrame ):
    def __init__( self, parent ):
        QFrame.__init__( self, parent )
        self.setFrameShape( QFrame.HLine )
        self.setLineWidth( 2 )
        self.setMinimumHeight( 10 )

class PanelManagerStatus(QWidget, Ui_PanelManagerStatusUI):
    def __init__(self, parent):
        QWidget.__init__(self,parent)
        self.setupUi(self)

class PanelManager(QWidget):
    def __init__(self, parent, devmgr):
        QWidget.__init__(self, parent)
        self.devmgr = devmgr
        self.devmgr.registerPreUpdateCallback(self.devlistPreUpdate)
        self.devmgr.registerPostUpdateCallback(self.devlistPostUpdate)
        self.devmgr.registerUpdateCallback(self.devlistUpdate)
        self.devmgr.registerDestroyedCallback(self.devmgrDestroyed)

        # maps a device GUID to a QT panel
        self.panels = {}

        # a layout for ourselves
        self.layout = QVBoxLayout( self )

        # the tabs
        self.tabs = QTabWidget(self)
        self.tabs.hide()
        self.layout.addWidget(self.tabs)

        # a dialog that is shown during update
        self.status = PanelManagerStatus(self)
        self.status.lblMessage.setText("Initializing...")
        self.layout.addWidget(self.status)
        self.status.show()

        # live check timer
        self.alivetimer = QTimer()
        QObject.connect( self.alivetimer, SIGNAL('timeout()'), self.commCheck )
        self.alivetimer.start( 2000 )

    def count(self):
        return self.tabs.count()

    def pollPanels(self):
        # only when not modifying the tabs
        if self.tabs.isEnabled():
            for guid in self.panels.keys():
                w = self.panels[guid]
                if 'polledUpdate' in dir(w):
                    try:
                        w.polledUpdate()
                    except:
                        print "error in polled update"

    def devlistPreUpdate(self):
        print "devlistPreUpdate"
        self.tabs.setEnabled(False)
        self.tabs.hide()
        self.status.lblMessage.setText("Bus reconfiguration in progress, please wait...")
        self.status.show()

    def devlistPostUpdate(self):
        print "devlistPostUpdate"
        self.updatePanels()

    def devlistUpdate(self):
        print "devlistUpdate"

    def devmgrDestroyed(self):
        print "devmgrDestroyed"
        self.alivetimer.stop()
        self.tabs.setEnabled(False)
        self.tabs.hide()
        self.status.lblMessage.setText("DBUS server was shut down, please restart it and restart ffadomixer...")
        self.status.show()

    def commCheck(self):
        try:
            nbDevices = self.devmgr.getNbDevices()
        except:
            print "comms lost"
            self.tabs.setEnabled(False)
            self.tabs.hide()
            self.status.lblMessage.setText("Failed to communicate with DBUS server. Please restart it and restart ffadomixer...")
            self.status.show()
            self.alivetimer.stop()

    def updatePanels(self):
        nbDevices = self.devmgr.getNbDevices()

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
                print "going to remove %s" % str(guid)
            else:
                print "going to keep %s" % str(guid)

        # figure out what to add
        to_add = []
        for guid in guids_present:
            if not guid in guids_with_tabs:
                to_add.append(guid)
                print "going to add %s" % str(guid)

        # update the widget
        for guid in to_remove:
            w = self.panels[guid]
            del self.panels[guid] # remove from the list
            idx = self.tabs.indexOf(w)
            self.tabs.removeTab(idx)
            del w # GC might also take care of that

        for guid in to_add:
            # retrieve the device manager index
            idx = guid_indexes[guid]
            path = self.devmgr.getDeviceName(idx)
            print "Adding device %d: %s" % (idx, path)

            cfgrom = ConfigRomInterface(FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+'/DeviceManager/'+path)
            vendorId = cfgrom.getVendorId()
            modelId = cfgrom.getModelId()
            unitVersion = cfgrom.getUnitVersion()
            guid = cfgrom.getGUID()
            vendorName = cfgrom.getVendorName()
            modelName = cfgrom.getModelName()
            print " Found (%s, %X, %X) %s %s" % (str(guid), vendorId, modelId, vendorName, modelName)

            # check whether this has already been registered at ffado.org
            reg = ffado_registration(FFADO_VERSION, int(guid, 16),
                                     vendorId, modelId,
                                     vendorName, modelName)
            reg.check_for_registration()

            thisdev = (vendorId, modelId);
            # The MOTU devices use unitVersion to differentiate models.  For the
            # moment thought we don't need to know precisely which model we're
            # using.
            if vendorId == 0x1f2:
                thisdev = (vendorId, 0x00000000)

            dev = None
            for d in SupportedDevices:
                if d[0] == thisdev:
                    dev = d

            w = QWidget( self.tabs )
            l = QVBoxLayout( w )

            # create a control object
            hw = ControlInterface(FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+'/DeviceManager/'+path)
            clockselect = ClockSelectInterface( FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+"/DeviceManager/"+path )
            samplerateselect = SamplerateSelectInterface( FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+"/DeviceManager/"+path )
            nickname = TextInterface( FFADO_DBUS_SERVER, FFADO_DBUS_BASEPATH+"/DeviceManager/"+path+"/Generic/Nickname" )

            #
            # Generic elements for all mixers follow here:
            #
            tmp = GlobalMixer( w )
            tmp.configrom = cfgrom
            tmp.clockselect = clockselect
            tmp.samplerateselect = samplerateselect
            tmp.nickname = nickname
            tmp.hw = hw
            tmp.initValues()
            l.addWidget( tmp, 1 )

            #
            # Line to separate
            #
            l.addWidget( HLine( w ) )

            #
            # Specific (or dummy) mixer widgets get loaded in the following
            #
            if dev != None:
                mixerapp = dev[1]
                exec( "mixerwidget = "+mixerapp+"( w )" )
            else:
                mixerwidget = DummyMixer( w )
                mixerapp = modelName+" (Dummy)"

            #
            # The same for all mixers
            #
            l.addWidget( mixerwidget, 10 )
            mixerwidget.configrom = cfgrom
            mixerwidget.clockselect = clockselect
            mixerwidget.samplerateselect = samplerateselect
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

            self.tabs.addTab( w, title )
            self.panels[guid] = w

        # if there is no panel, add the no-device message
        # else make sure it is not present
        if self.count() == 0:
            self.tabs.hide()
            self.tabs.setEnabled(False)
            self.status.lblMessage.setText("No supported device found.")
            self.status.show()
        else:
            self.tabs.show()
            self.tabs.setEnabled(True)
            self.status.hide()
            if use_generic:
                #
                # Show the generic (development) mixer if it is available
                #
                w = GenericMixer( devmgr.bus, FFADO_DBUS_SERVER, mw )
                self.tabs.addTab( w, "Generic Mixer" )
                self.panels[GUID_GENERIC_MIXER] = w
