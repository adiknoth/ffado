/* motu_avdevice.h
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

#ifndef MOTUDEVICE_H
#define MOTUDEVICE_H

#include "iavdevice.h"

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebob/xmlparser.h"

#include "libstreaming/MotuStreamProcessor.h"

#define MOTUFW_BASE_ADDR     0xfffff0000000ULL
#define MOTUFW_BASE_RATE_44100          (0<<3)
#define MOTUFW_BASE_RATE_48000          (1<<3)
#define MOTUFW_RATE_MULTIPLIER_1X       (0<<4)
#define MOTUFW_RATE_MULTIPLIER_2X       (1<<4)
#define MOTUFW_RATE_MULTIPLIER_4X       (2<<4)
#define MOTUFW_BASE_RATE_MASK           (0x00000008)
#define MOTUFW_RATE_MULTIPLIER_MASK     (0x00000030)

/* Device registers */
#define MOTUFW_REG_RATECTRL             0x0b14


class ConfigRom;
class Ieee1394Service;

namespace Motu {

class MotuDevice : public IAvDevice {
public:
    MotuDevice( Ieee1394Service& ieee1394Service,
		  int nodeId,
		  int verboseLevel );
    virtual ~MotuDevice();

    virtual bool discover();
    virtual ConfigRom& getConfigRom() const;

    // obsolete, will be removed soon, unused
    virtual bool addXmlDescription( xmlNodePtr deviceNode ) {return true;};

    virtual void showDevice() const;

    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual int getSamplingFrequency( );

    virtual bool setId(unsigned int id);

    virtual int getStreamCount();
    virtual FreebobStreaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool prepare();

    virtual int startStreamByIndex(int i);
    virtual int stopStreamByIndex(int i);

    enum EMotuModel {
      MOTUFW_MODEL_NONE     = 0x0000,
      MOTUFW_MODEL_828mkII  = 0x0001,
      MOTUFW_MODEL_TRAVELER = 0x0002,
    };

protected:
    Ieee1394Service* m_1394Service;
    ConfigRom*       m_configRom;
    signed int       m_motu_model;
    int              m_nodeId;
    int              m_verboseLevel;
    unsigned int m_id;
    
	FreebobStreaming::MotuReceiveStreamProcessor *m_receiveProcessor;
	FreebobStreaming::MotuTransmitStreamProcessor *m_transmitProcessor;

private:

	unsigned int ReadRegister(unsigned int reg);
	signed int WriteRegister(unsigned int reg, quadlet_t data);

        // IEEE1394 Vendor IDs.  One would expect only MOTU, but you never
        // know if a clone might appear some day.
        enum EVendorId {
                MOTUFW_VENDOR_MOTU = 0x000001f2,
        };
        
        // IEEE1394 Unit directory version IDs for different MOTU hardware
        enum EUnitVersionId {
                MOTUFW_UNITVER_828mkII  = 0x00000003,
                MOTUFW_UNITVER_TRAVELER = 0x00000009,
        };

    // debug support
    DECLARE_DEBUG_MODULE;
};

}

#endif
