#!/usr/bin/python
#

from qt import *
from mixer_phase88 import *
from mixer_phase24 import *
from mixer_saffirepro import *
from mixer_saffire import *
from mixer_af2 import *
import sys
import dbus

SupportedDevices=[
    [(0x000aac, 0x00000003),'Phase88Control'],
    [(0x000aac, 0x00000004),'PhaseX24Control'],
    [(0x000aac, 0x00000007),'PhaseX24Control'],
    [(0x00130e, 0x00000003),'SaffireProMixer'],
    [(0x00130e, 0x00000006),'SaffireProMixer'],
    [(0x00130e, 0x00000000),'SaffireMixer'],
    [(0x001486, 0x00000af2),'AudioFire2Mixer'],
    ]

class ControlInterface:
    def __init__(self, servername, basepath):
        self.basepath=basepath
        self.servername=servername
        self.bus=dbus.SessionBus()
            
    def setContignuous(self, subpath, v):
        path = self.basepath + subpath
        dev = self.bus.get_object(self.servername, path)
        dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Continuous')
        dev_cont.setValue(v)
           
    def getContignuous(self, subpath):
        path = self.basepath + subpath
        dev = self.bus.get_object(self.servername, path)
        dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Continuous')
        return dev_cont.getValue()        
    
    def setDiscrete(self, subpath, v):
        path = self.basepath + subpath
        dev = self.bus.get_object(self.servername, path)
        dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Discrete')
        dev_cont.setValue(v)
           
    def getDiscrete(self, subpath):
        path = self.basepath + subpath
        dev = self.bus.get_object(self.servername, path)
        dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.Discrete')
        return dev_cont.getValue()

    def setMatrixMixerValue(self, subpath, row, col, v):
        path = self.basepath + subpath
        dev = self.bus.get_object(self.servername, path)
        dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.MatrixMixer')
        dev_cont.setValue(row, col, v)

    def getMatrixMixerValue(self, subpath, row, col):
        path = self.basepath + subpath
        dev = self.bus.get_object(self.servername, path)
        dev_cont = dbus.Interface(dev, dbus_interface='org.ffado.Control.Element.MatrixMixer')
        return dev_cont.getValue(row, col)

class DeviceManagerInterface:
    def __init__(self, servername, basepath):
        self.basepath=basepath + '/DeviceManager'
        self.servername=servername
        self.bus=dbus.SessionBus()
        self.dev = self.bus.get_object(self.servername, self.basepath)
        self.iface = dbus.Interface(self.dev, dbus_interface='org.ffado.Control.Element.Container')
            
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
   

if __name__ == "__main__":
    
    server='org.ffado.Control'
    basepath='/org/ffado/Control'
    
    app = QApplication(sys.argv)
    
    devmgr=DeviceManagerInterface(server, basepath)
    nbDevices=devmgr.getNbDevices()
    
    forms=[];
    for idx in range(nbDevices):
        path=devmgr.getDeviceName(idx)
        print "Found device %d: %s" % (idx, path)
        
        cfgrom=ConfigRomInterface(server, basepath+'/DeviceManager/'+path)
        vendorId=cfgrom.getVendorId()
        modelId=cfgrom.getModelId()
        
        print "Found (%X, %X) %s %s" % (vendorId, modelId, cfgrom.getVendorName() , cfgrom.getModelName())
        
        thisdev=(vendorId, modelId);
        
        for dev in SupportedDevices:
            if dev[0]==thisdev:
                print dev[1]
                exec('forms.append('+dev[1]+'())')
                forms[idx].hw=ControlInterface(server, basepath+'/DeviceManager/'+path)
                forms[idx].initValues()
                forms[idx].show()
                app.setMainWidget(forms[idx])

    if forms:
        QObject.connect(app,SIGNAL("lastWindowClosed()"),app,SLOT("quit()"))

        app.exec_loop()
    else:
        print "No supported device found..."
