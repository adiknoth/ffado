#!/usr/bin/python

'''
This test is for checking playback-routing of devices based on Echo Fireworks.
Only two devices support this function, AudioFire2 and AudioFire4.

 AudioFire2:
	Playback stream ch1/2, 3/4, 5/6 can be assigned to analog out ch1/2,
	headphone out ch1/2, and digital out ch1/2.
	
 AudioFire4:
	Playback stream ch1/2, 3/4, 5/6 can be assigned to analog out ch1/2,
	3/4 and digital out ch1/2.

 Usage:
	To set routing, give two arguments below.

	Hardware port id as a first argument.
	id	AudioFire2		AudioFire4
	0	Analog out ch1/2	Analog out ch1/2
	1	Headphone out ch1/2	Analog out ch4/4
	2	Digital out ch1/2	Digital out ch1/2

	Playback stream id as a second argument
	0	Stream out ch1/2
	1	Stream out ch3/4
	2	Stream out ch5/6 (shown as digital out)

	Give no arguments, current state is printed.
'''

import sys
import dbus

GUID = "0014860f5a616e83" # for AudioFire4
#GUID = "001486069D5BD6CA" # for AudioFire2

servername = "org.ffado.Control"
path = "/org/ffado/Control/DeviceManager/%s/" % GUID
bus = dbus.SessionBus()

# connect to ffado-dbus-server to ask hardware information
try:
	hwinfo = dbus.Interface(bus.get_object(servername, path + 'HwInfo/PlaybackRouting'),
				dbus_interface='org.ffado.Control.Element.Discrete')
except:
	print "'ffado-dbus-server' should be started in advance."
	sys.exit()

# ask the device supports playback-routing
try:
	if(not hwinfo.getValue()):
		raise Exception()
except:
	print "Your device doesn't support playback-routing."
	sys.exit()

router = dbus.Interface(bus.get_object(servername, path + "PlaybackRouting"),
                        dbus_interface='org.ffado.Control.Element.Discrete')

argvs = sys.argv
argc = len(argvs)

# set status
if (argc == 3):
	port = int(argvs[1])
	strm = int(argvs[2])
	if ((port > 2) or (strm > 2)):
		print 'arguments should be less than 3.'
		sys.exit()
	s = router.setValueIdx(port, strm);
	print s
# get status
else:
	print "port\tstream"
	for i in range(3):
		print i, "\t", router.getValueIdx(i);

