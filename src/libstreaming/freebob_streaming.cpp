/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */

/* freebob_streaming.c
 *
 * Implementation of the FreeBob Streaming API
 *
 */

#include "libfreebob/freebob_streaming.h"
#include "messagebuffer.h"
#include "../devicemanager.h"
#include "../iavdevice.h"

#include "IsoHandler.h"
#include "IsoStream.h"
#include "StreamProcessor.h"
#include "StreamProcessorManager.h"
#include "AmdtpStreamProcessor.h"
#include "IsoHandlerManager.h"
#include "StreamRunner.h"
#include "FreebobPosixThread.h"
#include "AmdtpPort.h"

#include <assert.h>

#include <string.h>
#include <string>

/**
 * Device structure
 */

DECLARE_GLOBAL_DEBUG_MODULE;

using namespace FreebobStreaming;

struct _freebob_device
{	
	DeviceManager * m_deviceManager;
	StreamProcessorManager *processorManager;
	StreamRunner *runner;
	FreebobPosixThread *thread;

	freebob_options_t options;
	freebob_device_info_t device_info;
}; 

freebob_device_t *freebob_streaming_init (freebob_device_info_t *device_info, freebob_options_t options) {
	freebob_messagebuffer_init();
	int i=0;

	struct _freebob_device *dev = new struct _freebob_device;

	if(!dev) {
		debugFatal( "Could not allocate streaming device\n" );
		return 0;
	}

	memcpy((void *)&dev->options, (void *)&options, sizeof(dev->options));
	memcpy((void *)&dev->device_info, (void *)device_info, sizeof(dev->device_info));

	dev->m_deviceManager = new DeviceManager();
	if ( !dev->m_deviceManager ) {
		debugFatal( "Could not allocate device manager\n" );
			delete dev;
		return 0;
	}
	if ( !dev->m_deviceManager->initialize( dev->options.port ) ) {
		debugFatal( "Could not initialize device manager\n" );
		delete dev->m_deviceManager;
			delete dev;
		return 0;
	}


	// also create a processor manager to manage the actual stream
	// processors	
	dev->processorManager = new StreamProcessorManager(dev->options.period_size,dev->options.nb_buffers);
	if(!dev->processorManager) {
		debugFatal("Could not create StreamProcessorManager\n");
        	delete dev->m_deviceManager;
		delete dev;
		return 0;
	}
	dev->processorManager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
	if(!dev->processorManager->init()) {
		debugFatal("Could not init StreamProcessorManager\n");
		delete dev->processorManager;
        	delete dev->m_deviceManager;
		delete dev;
		return 0;
	}

	// now create the runner that does the actual streaming
	dev->runner = new StreamRunner(dev->processorManager);
	if(!dev->runner) {
		debugFatal("Could not create StreamRunner\n");
		delete dev->processorManager;
	        delete dev->m_deviceManager;
		delete dev;
		return 0;
	}

	// and the tread that runs the runner
	dev->thread=new FreebobPosixThread(dev->runner);
	if(!dev->thread) {
		debugFatal("Could not create Thread for streamrunner\n");
		delete dev->runner;
		delete dev->processorManager;
		delete dev->m_deviceManager;
		delete dev;
		return 0;
	}

 	dev->runner->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
	dev->processorManager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

	// discover the devices on the bus
	if(!dev->m_deviceManager->discover(DEBUG_LEVEL_NORMAL)) {
		debugOutput(DEBUG_LEVEL_VERBOSE, "Could not discover devices\n");
		return 0;
	}

	// iterate over the found devices
	// add the stream processors of the devices to the managers
	for(i=0;i<dev->m_deviceManager->getAvDeviceCount();i++) {
		IAvDevice *device=dev->m_deviceManager->getAvDeviceByIndex(i);
		assert(device);

		// prepare the device
		device->prepare();

		int j=0;
		for(j=0; j<device->getStreamCount();j++) {
			StreamProcessor *streamproc=device->getStreamProcessorByIndex(j);

			debugOutput(DEBUG_LEVEL_VERBOSE, "Registering stream processor %d of device %d with processormanager\n",j,i);
			if (!dev->processorManager->registerProcessor(streamproc)) {
				debugWarning("Could not register stream processor (%p) with the Processor manager\n",streamproc);
			}
		}
	}

	// we are ready!

	debugOutputShort(DEBUG_LEVEL_VERBOSE, "\n\n");
	return dev;

}

void freebob_streaming_finish(freebob_device_t *dev) {

	assert(dev);

	delete dev->thread;
	delete dev->runner;
	delete dev->processorManager;
   	delete dev->m_deviceManager;
	delete dev;

	freebob_messagebuffer_exit();
	
	return;
}

int freebob_streaming_start(freebob_device_t *dev) {
	int i=0;
	debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Start -------------\n");

	
	// create the connections for all devices
	// iterate over the found devices
	// add the stream processors of the devices to the managers
	for(i=0;i<dev->m_deviceManager->getAvDeviceCount();i++) {
		IAvDevice *device=dev->m_deviceManager->getAvDeviceByIndex(i);
		assert(device);

		int j=0;
		for(j=0; j<device->getStreamCount();j++) {
			// start the stream
			device->startStreamByIndex(j);
		}
	}

	
	dev->processorManager->prepare();
	
	// start the runner thread
	dev->thread->Start();
	
	dev->processorManager->start();

	int cycle=0;
// 	if(dev->isoManager->startHandlers(cycle)) {
// 		debugFatal("Could not start handlers\n");
// 		return -1;
// 	}
	return 0;
}

int freebob_streaming_stop(freebob_device_t *dev) {
	int i;
	debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Stop -------------\n");

	dev->thread->Stop();

// 	dev->isoManager->stopHandlers();
	dev->processorManager->stop();

	// create the connections for all devices
	// iterate over the found devices
	// add the stream processors of the devices to the managers
	for(i=0;i<dev->m_deviceManager->getAvDeviceCount();i++) {
		IAvDevice *device=dev->m_deviceManager->getAvDeviceByIndex(i);
		assert(device);

		int j=0;
		for(j=0; j<device->getStreamCount();j++) {
			// start the stream
			device->stopStreamByIndex(j);
		}
	}

	return 0;
}

int freebob_streaming_reset(freebob_device_t *dev) {
	debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Reset -------------\n");
	/* 
	 * Reset means:
	 * 1) Stopping the packetizer thread
	 * 2) Bringing all buffers & connections into a know state
	 *    - Clear all capture buffers
	 *    - Put nb_periods*period_size of null frames into the playback buffers
	 * 3) Restarting the packetizer thread
	 */
// 	dev->thread->Stop();
// 
// 	dev->processorManager->stop();
// 	
// 	dev->processorManager->reset();
// 	
// 	dev->processorManager->start();
// 	
// 	dev->thread->Start();

	return 0;
}

int freebob_streaming_wait(freebob_device_t *dev) {

	static int periods=0;
	static int periods_print=0;
		periods++;
  		if(periods>periods_print) {
			debugOutput(DEBUG_LEVEL_VERBOSE, "\n");
			debugOutput(DEBUG_LEVEL_VERBOSE, "============================================\n");
			dev->processorManager->dumpInfo();
			debugOutput(DEBUG_LEVEL_VERBOSE, "--------------------------------------------\n");
			hexDumpQuadlets((quadlet_t*)(dev->processorManager->getPortByIndex(0, Port::E_Capture)->getBufferAddress()),10);
			debugOutput(DEBUG_LEVEL_VERBOSE, "============================================\n");
			debugOutput(DEBUG_LEVEL_VERBOSE, "\n");
			periods_print+=100;
  		}
	dev->processorManager->waitForPeriod();
	return dev->options.period_size;
}

int freebob_streaming_transfer_capture_buffers(freebob_device_t *dev) {
	return dev->processorManager->transfer(StreamProcessor::E_Receive);
}

int freebob_streaming_transfer_playback_buffers(freebob_device_t *dev) {
	return dev->processorManager->transfer(StreamProcessor::E_Transmit);
}

int freebob_streaming_transfer_buffers(freebob_device_t *dev) {
	return dev->processorManager->transfer();
}


int freebob_streaming_write(freebob_device_t *dev, int i, freebob_sample_t *buffer, int nsamples) {
	return 0;
}

int freebob_streaming_read(freebob_device_t *dev, int i, freebob_sample_t *buffer, int nsamples) {
	return 0;
}

pthread_t freebob_streaming_get_packetizer_thread(freebob_device_t *dev) {
	return 0;
}


int freebob_streaming_get_nb_capture_streams(freebob_device_t *dev) {
	return dev->processorManager->getPortCount(Port::E_Capture);
}

int freebob_streaming_get_nb_playback_streams(freebob_device_t *dev) {
	return dev->processorManager->getPortCount(Port::E_Playback);
}

int freebob_streaming_get_capture_stream_name(freebob_device_t *dev, int i, char* buffer, size_t buffersize) {
	Port *p=dev->processorManager->getPortByIndex(i, Port::E_Capture);
	if(!p) {
		debugWarning("Could not get capture port at index %d\n",i);
		return -1;
	}

	std::string name=p->getName();
	if (!strncpy(buffer, name.c_str(), buffersize)) {
		debugWarning("Could not copy name\n");
		return -1;
	} else return 0;
}

int freebob_streaming_get_playback_stream_name(freebob_device_t *dev, int i, char* buffer, size_t buffersize) {
	
	Port *p=dev->processorManager->getPortByIndex(i, Port::E_Playback);
	if(!p) {
		debugWarning("Could not get playback port at index %d\n",i);
		return -1;
	}

	std::string name=p->getName();
	if (!strncpy(buffer, name.c_str(), buffersize)) {
		debugWarning("Could not copy name\n");
		return -1;
	} else return 0;
}

freebob_streaming_stream_type freebob_streaming_get_capture_stream_type(freebob_device_t *dev, int i) {
	Port *p=dev->processorManager->getPortByIndex(i, Port::E_Capture);
	if(!p) {
		debugWarning("Could not get capture port at index %d\n",i);
		return freebob_stream_type_invalid;
	}
	switch(p->getPortType()) {
	case Port::E_Audio:
		return freebob_stream_type_audio;
	case Port::E_Midi:
		return freebob_stream_type_midi;
	case Port::E_Control:
		return freebob_stream_type_control;
	default:
		return freebob_stream_type_unknown;
	}
}

freebob_streaming_stream_type freebob_streaming_get_playback_stream_type(freebob_device_t *dev, int i) {
	Port *p=dev->processorManager->getPortByIndex(i, Port::E_Playback);
	if(!p) {
		debugWarning("Could not get playback port at index %d\n",i);
		return freebob_stream_type_invalid;
	}
	switch(p->getPortType()) {
	case Port::E_Audio:
		return freebob_stream_type_audio;
	case Port::E_Midi:
		return freebob_stream_type_midi;
	case Port::E_Control:
		return freebob_stream_type_control;
	default:
		return freebob_stream_type_unknown;
	}
}
// TODO: the way port buffers are set doesn't satisfy me
int freebob_streaming_set_capture_stream_buffer(freebob_device_t *dev, int i, char *buff,  freebob_streaming_buffer_type t) {
	Port *p=dev->processorManager->getPortByIndex(i, Port::E_Capture);
	if(!p) {
		debugWarning("Could not get playback port at index %d\n",i);
		return -1;
	}
	switch(t) {
	case freebob_buffer_type_uint24:
		p->setDataType(Port::E_Int24);
		p->setBufferAddress((void *)buff);
		return 0;
// 		break;
	case freebob_buffer_type_float:
		p->setDataType(Port::E_Float);
		p->setBufferAddress((void *)buff);
		return 0;
	case freebob_buffer_type_per_stream:
		p->setDataType(Port::E_Int24);
// 		p->detachBuffer();

		break;
	}

	return 0;

}

int freebob_streaming_set_playback_stream_buffer(freebob_device_t *dev, int i, char *buff,  freebob_streaming_buffer_type t) {
	Port *p=dev->processorManager->getPortByIndex(i, Port::E_Playback);
	if(!p) {
		debugWarning("Could not get playback port at index %d\n",i);
		return -1;
	}
	switch(t) {
	case freebob_buffer_type_uint24:
		p->setDataType(Port::E_Int24);
		p->setBufferAddress((void *)buff);
		return 0;
	case freebob_buffer_type_float:
		p->setDataType(Port::E_Float);
		p->setBufferAddress((void *)buff);
		return 0;
	case freebob_buffer_type_per_stream:
		p->setDataType(Port::E_Int24);
// 		p->detachBuffer();

		break;
	}

	return 0;
}

