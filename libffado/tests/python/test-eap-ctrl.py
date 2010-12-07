#!/usr/bin/python
#
# Copyright (C) 2005-2009 by Pieter Palmers
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

import dbus

GUID = "00130e0401400045"

servername = "org.ffado.Control"
path = "/org/ffado/Control/DeviceManager/%s/EAP/" % GUID
bus = dbus.SessionBus()

router = dbus.Interface(bus.get_object(servername, path+"Router"),
                        dbus_interface='org.ffado.Control.Element.CrossbarRouter')
mixer = dbus.Interface(bus.get_object(servername, path+"MatrixMixer"),
                        dbus_interface='org.ffado.Control.Element.MatrixMixer')

# mute the mixer
#nbinputs = mixer.getRowCount()
#nboutputs = mixer.getColCount()

#print nbinputs
#print nboutputs

#for i in range(nbinputs):
    #for o in range(nboutputs):
        #mixer.setValue(i, o, 0)

## reconfigure the router
#def reroute(src, dsts):
    #srcidx = router.getSourceIndex(src)
    #for dst in dsts:
        #print dst
        #dstidx = router.getDestinationIndex(dst)
    
        #sourceidx = 0
        ## disconnect
        #while sourceidx >= 0:
            #sourceidx = router.getSourceForDestination(dstidx)
            #router.setConnectionState(sourceidx, dstidx, 0)
    
        ## connect
        #if router.canConnect(srcidx, dstidx):
            #print router.getConnectionState(srcidx, dstidx)
            #print router.setConnectionState(srcidx, dstidx, 1)

#reroute("InS1:02", ["InS0:00", "InS1:00", "InS1:02", "InS1:04", "InS1:06"])
#reroute("InS1:03", ["InS0:01", "InS1:01", "InS1:03", "InS1:05", "InS1:07"])

tst = router.getConnectionMap()

nb_sources = router.getNbSources()
nb_destinations = router.getNbDestinations()

source_names = router.getSourceNames()
destination_names = router.getDestinationNames()


srcnames_formatted = ["%10s" % s for s in source_names]
s = ""
for i in range(10):
    s += "           |"
    for src in range(nb_sources):
        name = srcnames_formatted[src]
        char = name[i]
        s += " %s" % char
    s += "\n"

s += "-----------+"
for src in range(nb_sources):
    s += "--"
s += "\n"

for dst in range(nb_destinations):
    s += "%10s |" % destination_names[dst]
    for src in range(nb_sources):
        idx = dst * nb_sources + src
        s += " %d" % tst[idx]
    s += "\n"

print s
