#!/usr/bin/python

'''
This test is for checking a function to switch digital interface on devices
based on Echo Fireworks. Only two devices support this function, AudioFire8A
(as of July 2009) and AudioFirePre8.

If no argument given, this script output current status.
If one argument given, this script try to set it.

The value of argument is:
	0: S/PDIF over coaxial digital interface
	2: S/PDIF over optical digital interface
	3: ADAT over optical digital interface

The value '1' seems to be 'ADAT over coaxial digital interface'. But according
to users manual, these devices don't support this. I think there is no
specification to satisfy 'ADAT over coaxial digital interface'.
'''

import sys
import dbus

# Echo AudioFirePre8
GUID = "0014860a5b6bdb9b"
# GUID = "001486085b68e02a" # Echo AudioFire8A
# GUID = "001486045B65D6E8" # Echo AudioFire8

servername = "org.ffado.Control"
path = "/org/ffado/Control/DeviceManager/%s/" % GUID
bus = dbus.SessionBus()

# connect to ffado-dbus-server to ask hardware information
try:
	hwinfo = dbus.Interface(bus.get_object(servername,
				path + 'HwInfo/OpticalInterface'),
				dbus_interface='org.ffado.Control.Element.Discrete')
except:
	print "'ffado-dbus-server' should be started in advance."
	sys.exit()

# ask the device has optical digital interface
try:
	if (not hwinfo.getValue()):
		raise Exception()
except:
	print "Your device just supports coaxial digital interface."
	sys.exit()

interface = dbus.Interface(bus.get_object(servername,
				path + "DigitalInterface"),
				dbus_interface='org.ffado.Control.Element.Discrete')

argvs = sys.argv
argc = len(argvs)

if (argc > 2):
	print "too many arguments"
	sys.exit()
elif (argc > 1):
	param = int(argvs[1])
	if ((param == 1) or (param > 3)):
		print "wrong argument. it should be:"
		print "\t0(S/PDIF coaxial), 2(S/PDIF optical), or 3(ADAT optical)"
	else:
		print interface.setValue(param)
		
else:
	print interface.getValue()

