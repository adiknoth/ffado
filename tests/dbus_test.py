#!/usr/bin/python
#
# Copyright (C) 2005-2007 by Pieter Palmers
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

import sys

import os
import time
import dbus

class ControlInterface:
    def __init__(self, servername, basepath):
        self.basepath=basepath
        self.servername=servername
        self.bus=dbus.SessionBus()

    def setContignuous(self, subpath, v, idx=None):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Continuous')
            if idx == None:
                dev_cont.setValue(v)
            else:
                dev_cont.setValueIdx(idx,v)
        except:
            print "Failed to set Continuous %s on server %s" % (path, self.servername)

    def getContignuous(self, subpath, idx=None):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Continuous')
            if idx == None:
                return dev_cont.getValue()
            else:
                return dev_cont.getValueIdx(idx)
        except:
            print "Failed to get Continuous %s on server %s" % (path, self.servername)
            return 0

    def setDiscrete(self, subpath, v):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Discrete')
            dev_cont.setValue(v)
        except:
            print "Failed to set Discrete %s on server %s" % (path, self.servername)

    def getDiscrete(self, subpath):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Discrete')
            return dev_cont.getValue()
        except:
            print "Failed to get Discrete %s on server %s" % (path, self.servername)
            return 0

    def setRegister(self, subpath, address, value):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Register')
            dev_cont.setValue(address, value)
        except:
            print "Failed to set Register %s on server %s" % (path, self.servername)

    def getRegister(self, subpath, address):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Register')
            return dev_cont.getValue(address)
        except:
            print "Failed to get Register %s on server %s" % (path, self.servername)
            return 0

    def setText(self, subpath, v):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Text')
            dev_cont.setValue(v)
        except:
            print "Failed to set Text %s on server %s" % (path, self.servername)

    def getText(self, subpath):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Text')
            return dev_cont.getValue()
        except:
            print "Failed to get Text %s on server %s" % (path, self.servername)
            return 0

    def setMatrixMixerValue(self, subpath, row, col, v):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.MatrixMixer')
            dev_cont.setValue(row, col, v)
        except:
            print "Failed to set MatrixMixer %s on server %s" % (path, self.servername)

    def getMatrixMixerValue(self, subpath, row, col):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.MatrixMixer')
            return dev_cont.getValue(row, col)
        except:
            print "Failed to get MatrixMixer %s on server %s" % (path, self.servername)
            return 0

    def enumSelect(self, subpath, v):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Enum')
            dev_cont.select(v)
        except:
            print "Failed to select %s on server %s" % (path, self.servername)

    def enumSelected(self, subpath):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Enum')
            return dev_cont.selected()
        except:
            print "Failed to get selected enum %s on server %s" % (path, self.servername)
            return 0

    def enumGetLabel(self, subpath, v):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Enum')
            return dev_cont.getEnumLabel(v)
        except:
            print "Failed to get enum label %s on server %s" % (path, self.servername)
            return 0

    def enumCount(self, subpath):
        try:
            path = self.basepath + subpath
            dev = self.bus.get_object(self.servername, path)
            dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Enum')
            return dev_cont.count()
        except:
            print "Failed to get enum count %s on server %s" % (path, self.servername)
            return 0

class DeviceManagerInterface:
    def __init__(self, servername, basepath):
        self.basepath=basepath + '/DeviceManager'
        self.servername=servername
        self.bus=dbus.SessionBus()
        self.dev = self.bus.get_object(self.servername, self.basepath)
        self.iface = dbus.Interface(self.dev, dbus_interface='org.ffado.Control.Element.Container')
        # signal reception does not work yet since we need a mainloop for that
        # and qt3 doesn't provide one for python/dbus
        #try:
            #self.dev.connect_to_signal("Updated", self.updateSignal, \
                                       #dbus_interface="org.ffado.Control.Element.Container", arg0=self)
        #except dbus.DBusException:
            #traceback.print_exc()

    #def updateSignal(self):
        #print ("Received update signal")

    def getNbDevices(self):
        return self.iface.getNbElements()
    def getDeviceName(self, idx):
        return self.iface.getElementName(idx)


class ConfigRomInterface:
    def __init__(self, servername, devicepath):
        self.basepath=devicepath + '/ConfigRom'
        self.servername=servername
        self.bus=dbus.SessionBus()
        self.dev = self.bus.get_object(self.servername, self.basepath)
        self.iface = dbus.Interface(self.dev, dbus_interface='org.ffado.Control.Element.ConfigRomX')
    def getGUID(self):
        return self.iface.getGUID()
    def getVendorName(self):
        return self.iface.getVendorName()
    def getModelName(self):
        return self.iface.getModelName()
    def getVendorId(self):
        return self.iface.getVendorId()
    def getModelId(self):
        return self.iface.getModelId()
    def getUnitVersion(self):
        return self.iface.getUnitVersion()

class ClockSelectInterface:
    def __init__(self, servername, devicepath):
        self.basepath=devicepath + '/Generic/ClockSelect'
        self.servername=servername
        self.bus=dbus.SessionBus()
        self.dev = self.bus.get_object(self.servername, self.basepath)
        self.iface = dbus.Interface(self.dev, dbus_interface='org.ffado.Control.Element.AttributeEnum')
    def count(self):
        return self.iface.count()
    def select(self, idx):
        return self.iface.select(idx)
    def selected(self):
        return self.iface.selected()
    def getEnumLabel(self, idx):
        return self.iface.getEnumLabel(idx)
    def attributeCount(self):
        return self.iface.attributeCount()
    def getAttributeValue(self, idx):
        return self.iface.getAttributeValue(idx)
    def getAttributeName(self, idx):
        return self.iface.getAttributeName(idx)

class TextInterface:
    def __init__(self, servername, basepath):
        self.basepath=basepath
        self.servername=servername
        self.bus=dbus.SessionBus()
        self.dev = self.bus.get_object( self.servername, self.basepath )
        self.iface = dbus.Interface( self.dev, dbus_interface="org.ffado.Control.Element.Text" )

    def text(self):
        return self.iface.getValue()

    def setText(self,text):
        self.iface.setValue(text)

if __name__ == "__main__":

    print """
===========================================================
FFADO DBUS TEST TOOL
===========================================================
"""
    
    server='org.ffado.Control'
    basepath='/org/ffado/Control'

    repeat = 1
    while repeat > 0:
        try:
            devmgr = DeviceManagerInterface(server, basepath)
            nbDevices = devmgr.getNbDevices()
            repeat -= 1
        except dbus.DBusException, ex:
            print "\n"
            print "==========================================================="
            print "ERROR: Could not communicate with the FFADO DBus service..."
            print "==========================================================="
            print "\n"
            sys.exit( -1 )

    if nbDevices == 0:
        print "No supported device found..."
        sys.exit( -1 )

    idx = 0

    path=devmgr.getDeviceName(idx)
    print "Found device %d: %s" % (idx, path)

    cfgrom = ConfigRomInterface(server, basepath+'/DeviceManager/'+path)
    vendorId = cfgrom.getVendorId()
    modelId = cfgrom.getModelId()
    unitVersion = cfgrom.getUnitVersion()
    GUID = cfgrom.getGUID()
    vendorName = cfgrom.getVendorName()
    modelName = cfgrom.getModelName()
    print " Found (%s, %X, %X) %s %s" % (str(GUID), vendorId, modelId, vendorName, modelName)

    # create a control objects
    hw = ControlInterface(server, basepath+'/DeviceManager/'+path)
    clockselect = ClockSelectInterface( server, basepath+"/DeviceManager/"+path )
    nickname = TextInterface( server, basepath+"/DeviceManager/"+path+"/Generic/Nickname" )

    import time
    register = range(60)
    # do whatever you have to do
    for i in range(60):
        for regid in range(60):
            newval = hw.getRegister("/Register", regid)
            oldval = register[regid]
            if newval != oldval:
                print "%04d: from %8d | %08X to %8d | %08X" % (regid, oldval, oldval, newval, newval)
                register[regid] = newval
            time.sleep(0.2)
        print "--- %d ---" % i
        #time.sleep(1)

    #regid=eval(sys.argv[1])
    #val=eval(sys.argv[2])
    #print hw.getRegister("/Register", regid)
    #hw.setRegister("/Register", regid, val)
    