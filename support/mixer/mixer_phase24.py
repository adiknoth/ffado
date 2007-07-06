# -*- coding: utf-8 -*-

import sys
from qt import *
from mixer_phase24ui import *

import osc

class mixer_phase24(mixer_phase24ui):
    def __init__(self,parent = None,name = None,modal = 0,fl = 0):
        mixer_phase24ui.__init__(self,parent,name,modal,fl)

    def __set_volume(self, id, vol):
        osc.Message("/devicemanager/dev0/GenericMixer/Feature/%i" % id,
                    ["set", "volume", 0, vol]).sendlocal(17820)
        
    def __set_selector(self, id, val):
        osc.Message("/devicemanager/dev0/GenericMixer/Selector/%i" % id,
                    ["set", "value", val]).sendlocal(17820)
        
    # public slot
    def setVolume12(self,a0):
        self.__set_volume(3, -a0)

    # public slot
    def setVolume34(self,a0):
        self.__set_volume(4, -a0)

    # public slot
    def setVolumeLineIn(self,a0):
        self.__set_volume(6, -a0)

    # public slot
    def setVolumeSPDIFOut(self,a0):
        self.__set_volume(5, -a0)

    # public slot
    def setVolumeSPDIFIn(self,a0):
        self.__set_volume(7, -a0)

    # public slot
    def setVolumeMaster(self,a0):
        self.__set_volume(1, -a0)

    # public slot
    def setLineLevel(self,a0):
        self.__set_volume(2, a0 * -768)
        
    # public slot
    def setFrontLevel(self,a0):
        if(a0 == 0):
            self.__set_volume(8, 0)
        else:
            self.__set_volume(8, 1536)

    # public slot
    def setOutSource12(self,a0):
        self.__set_selector(1, a0)

    # public slot
    def setOutSource34(self,a0):
        self.__set_selector(2, a0)

    # public slot
    def setOutSourceSPDIF(self,a0):
        self.__set_selector(3, a0)

    # public slot
    def setSyncSource(self,a0):
        self.__set_selector(4, a0)


if __name__ == "__main__":
    a = QApplication(sys.argv)
    QObject.connect(a,SIGNAL("lastWindowClosed()"),a,SLOT("quit()"))
    w = mixer_phase24()
    a.setMainWidget(w)
    w.show()
    a.exec_loop()
