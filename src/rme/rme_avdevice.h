/* rme_avdevice.h
 * Copyright (C) 2006 Jonathan Woithe
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

#ifndef RMEDEVICE_H
#define RMEDEVICE_H

#include "iavdevice.h"

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebob/xmlparser.h"

// #include "libstreaming/RmeStreamProcessor.h"

class ConfigRom;
class Ieee1394Service;

namespace Rme {

class RmeDevice : public IAvDevice {
public:
    enum ERmeModel {
      RME_MODEL_NONE        = 0x0000,
      RME_MODEL_FIREFACE800 = 0x0001,
    };

    RmeDevice( std::auto_ptr<ConfigRom>( configRom ),
	      Ieee1394Service& ieee1394Service,
		  int nodeId,
		  int verboseLevel );
    virtual ~RmeDevice();

    static bool probe( ConfigRom& configRom );
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

    signed int getIsoRecvChannel(void);
    signed int getIsoSendChannel(void);

    signed int getEventSize(unsigned int dir);
  
protected:
    std::auto_ptr<ConfigRom>( m_configRom );
    Ieee1394Service* m_1394Service;
    
    signed int       m_rme_model;
    int              m_nodeId;
    int              m_verboseLevel;
    signed int m_id;
    signed int m_iso_recv_channel, m_iso_send_channel;
    signed int m_bandwidth;
    
//	FreebobStreaming::RmeReceiveStreamProcessor *m_receiveProcessor;
//	FreebobStreaming::RmeTransmitStreamProcessor *m_transmitProcessor;

private:
//	bool addPort(FreebobStreaming::StreamProcessor *s_processor,
//		char *name, 
//		enum FreebobStreaming::Port::E_Direction direction,
//		int position, int size);
//	bool addDirPorts(
//		enum FreebobStreaming::Port::E_Direction direction,
//		unsigned int sample_rate, unsigned int optical_mode);
        
	unsigned int ReadRegister(unsigned int reg);
	signed int WriteRegister(unsigned int reg, quadlet_t data);

        // IEEE1394 Vendor IDs.  One would expect only RME, but you never
        // know if a clone might appear some day.
        enum EVendorId {
                RME_VENDOR_RME = 0x00000a35,
        };
        
        // IEEE1394 Model IDs for different RME hardware
        enum EUnitVersionId {
                RME_MODELID_FIREFACE800 = 0x00101800,
        };

    // debug support
    DECLARE_DEBUG_MODULE;
};

}

#endif
