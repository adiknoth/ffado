/* dice_avdevice.h
 * Copyright (C) 2007 by Pieter Palmers
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
#ifndef DICEDEVICE_H
#define DICEDEVICE_H

#include "iavdevice.h"

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebob/xmlparser.h"

#include "libstreaming/AmdtpStreamProcessor.h"

class ConfigRom;
class Ieee1394Service;

namespace Dice {

// struct to define the supported devices
struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    char *vendor_name;
    char *model_name;
};

class DiceAvDevice : public IAvDevice {
public:
    DiceAvDevice( std::auto_ptr<ConfigRom>( configRom ),
	      Ieee1394Service& ieee1394Service,
		  int nodeId,
		  int verboseLevel );
    ~DiceAvDevice();

    static bool probe( ConfigRom& configRom );
    bool discover();
    ConfigRom& getConfigRom() const;

    // obsolete, will be removed soon, unused
    bool addXmlDescription( xmlNodePtr deviceNode ) {return true;};

    void showDevice() const;

    bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    int getSamplingFrequency( );

    bool setId(unsigned int id);

    int getStreamCount();
    Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    bool prepare();
    
    bool lock();
    bool unlock();
    
    bool startStreamByIndex(int i);
    bool stopStreamByIndex(int i);

    signed int getIsoRecvChannel(void);
    signed int getIsoSendChannel(void);
  
protected:
    std::auto_ptr<ConfigRom>( m_configRom );
    Ieee1394Service* m_1394Service;
    
    struct VendorModelEntry *m_model;
    
    int              m_nodeId;
    int              m_verboseLevel;
    signed int m_id;
    signed int m_iso_recv_channel, m_iso_send_channel;
    
	Streaming::AmdtpReceiveStreamProcessor *m_receiveProcessor;
	Streaming::AmdtpTransmitStreamProcessor *m_transmitProcessor;

private:
    // debug support
    DECLARE_DEBUG_MODULE;
};

}

#endif
