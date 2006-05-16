/* motu_avdevice.cpp
 * Copyright (C) 2006 by Pieter Palmers
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "motu/motu_avdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>

namespace Motu {

IMPL_DEBUG_MODULE( MotuDevice, MotuDevice, DEBUG_LEVEL_NORMAL );

MotuDevice::MotuDevice( Ieee1394Service& ieee1394service,
                        int nodeId,
                        int verboseLevel )
    : m_1394Service( &ieee1394service )
    , m_nodeId( nodeId )
    , m_verboseLevel( verboseLevel )
{
    if ( m_verboseLevel ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Motu::MotuDevice (NodeID %d)\n",
                 nodeId );
    m_configRom = new ConfigRom( m_1394Service, m_nodeId );
    m_configRom->initialize();

}

MotuDevice::~MotuDevice()
{
	delete m_configRom;
}

ConfigRom&
MotuDevice::getConfigRom() const
{
    return *m_configRom;
}

bool
MotuDevice::discover()
{
	signed int i, is_motu_fw_audio;

	/* A list of all IEEE1394 Vendor IDs used by MOTU.  One would expect this to
	* include only one, but stranger things have happened in this world.  The
	* list is terminated with a 0xffffffff value.
	*/
	const unsigned int motu_vendor_ids[] = {
		VENDOR_MOTU,
		VENDOR_MOTU_TEST,
		0xffffffff,
	};
	
	/* A list of all valid IEEE1394 model IDs for MOTU firewire audio devices,
	* terminated by 0xffffffff.
	*/
	const unsigned int motu_fw_audio_model_ids[] = {
		MOTU_828mkII, MOTU_TRAVELER, MOTU_TEST,
		0xffffffff,
	};
	
	/* Find out if this device is one we know about */
	is_motu_fw_audio = i = 0;

	while ((motu_vendor_ids[i]!=0xffffffff) 
			&& (m_configRom->getVendorId() != motu_vendor_ids[i]))
		i++;

	if (motu_vendor_ids[i] != 0xffffffff) {
		/* Device is made by MOTU.  See if the model is one we know about */
		i = 0;

		while ((motu_fw_audio_model_ids[i]!=0xffffffff) 
				&& (m_configRom->getModelId() != motu_fw_audio_model_ids[i]))
			i++;

		if (motu_fw_audio_model_ids[i]!=0xffffffff)
			is_motu_fw_audio = 1;
	}

	if (is_motu_fw_audio) {
		debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
			m_configRom->getVendorName(), m_configRom->getModelName());
		return true;
	}

    return false;
}



bool
MotuDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency )
{
    quadlet_t new_rate=0;
	int supported=true;

	switch ( samplingFrequency ) {
    case eSF_22050Hz:
		supported=false;
        break;
    case eSF_24000Hz:
		supported=false;
        break;
    case eSF_32000Hz:
		supported=false;
        break;
    case eSF_44100Hz:
        new_rate = MOTU_BASE_RATE_44100 | MOTU_RATE_MULTIPLIER_1X;
        break;
    case eSF_48000Hz:
        new_rate = MOTU_BASE_RATE_48000 | MOTU_RATE_MULTIPLIER_1X;
        break;
    case eSF_88200Hz:
        new_rate = MOTU_BASE_RATE_44100 | MOTU_RATE_MULTIPLIER_2X;
        break;
    case eSF_96000Hz:
        new_rate = MOTU_BASE_RATE_48000 | MOTU_RATE_MULTIPLIER_2X;
        break;
    case eSF_176400Hz:
        new_rate = MOTU_BASE_RATE_44100 | MOTU_RATE_MULTIPLIER_4X;
        break;
    case eSF_192000Hz:
        new_rate = MOTU_BASE_RATE_48000 | MOTU_RATE_MULTIPLIER_4X;
        break;
    default:
        supported=false;
    }

	// update the register
	if(supported) {
 		quadlet_t value=ReadRegister(0x0B14);
		value &= ~(MOTU_RATE_MASK);
		value |= new_rate;

		if(WriteRegister(0x0B14,value) == 0) {
			supported=true;
		} else {
			supported=false;
		}
	}

    return supported;
}

void
MotuDevice::showDevice() const
{
    printf( "%s %s at node %d\n",
	        m_configRom->getVendorName(), 
	        m_configRom->getModelName(),
	        m_nodeId );

}

bool
MotuDevice::addXmlDescription( xmlNodePtr deviceNode )
{
    return true;
}

/* ======================================================================== */

unsigned int MotuDevice::ReadRegister(unsigned int reg) {
/*
 * Attempts to read the requested register from the MOTU.
 */

	quadlet_t quadlet;
	assert(m_1394Service);
	
	quadlet = 0;
	if (m_1394Service->read(m_nodeId, MOTU_BASE_ADDR+reg, 4, &quadlet) < 0) {
		debugError("Error doing motu read from register 0x%06x\n",reg);
	}


	return ntohl(quadlet);
}

signed int MotuDevice::WriteRegister(unsigned int reg, quadlet_t data) {
/*
 * Attempts to write the given data to the requested MOTU register.
 */

  unsigned int err = 0;

  data = htonl(data);

  if (m_1394Service->write(m_nodeId, MOTU_BASE_ADDR+reg, 4, &data) < 0) {
    err = 1;
    debugError("Error doing motu write to register 0x%06x\n",reg);
  }

  usleep(100);

  return (err==0)?0:-1;
}

}
