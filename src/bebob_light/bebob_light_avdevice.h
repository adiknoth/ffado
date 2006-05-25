/* bebob_light_avdevice.h
 * Copyright (C) 2006 by Daniel Wagner
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

#ifndef BEBOB_LIGHT_AVDEVICE_H
#define BEBOB_LIGHT_AVDEVICE_H

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebobavc/avc_extended_cmd_generic.h"
#include "libfreebob/xmlparser.h"

#include "bebob_light/bebob_light_avplug.h"
#include "bebob_light/bebob_light_avdevicesubunit.h"

#include "libstreaming/AmdtpStreamProcessor.h"
#include "libstreaming/AmdtpPort.h"
#include "libstreaming/AmdtpPortInfo.h"

#include "iavdevice.h"

class ConfigRom;
class Ieee1394Service;
class SubunitPlugSpecificDataPlugAddress;

namespace BeBoB_Light {

class AvDevice : public IAvDevice {
public:
    friend class AvDeviceSubunitAudio;
    friend class AvDeviceSubunitMusic;

    // takes ownership of config rom
    AvDevice( Ieee1394Service& ieee1394service,
              int nodeId,
	      int verboseLevel );
    virtual ~AvDevice();

    virtual bool discover();
    virtual ConfigRom& getConfigRom() const;

    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual int getSamplingFrequency( );

	virtual int getStreamProcessorCount();
	virtual FreebobStreaming::StreamProcessor *getStreamProcessorByIndex(int i);

	virtual bool prepare();

	virtual int startStreamByIndex(int i);
	virtual int stopStreamByIndex(int i);


    virtual bool addXmlDescription( xmlNodePtr deviceNode );
    virtual void showDevice() const;

protected:
    bool discoverStep1();
    bool discoverStep2();
    bool discoverStep3();
    bool discoverStep4();
    bool discoverStep4Plug( AvPlugVector& isoPlugs );
    bool discoverStep5();
    bool discoverStep5Plug( AvPlugVector& isoPlugs );
    bool discoverStep6();
    bool discoverStep6Plug( AvPlugVector& isoPlugs );
    bool discoverStep7();
    bool discoverStep7Plug( AvPlugVector& isoPlugs );
    bool discoverStep8();
    bool discoverStep8Plug( AvPlugVector& isoPlugs );
    bool discoverStep9();
    bool discoverStep9Plug( AvPlugVector& isoPlugs );
    bool discoverStep10();
    bool discoverStep10Plug( AvPlugVector& isoPlugs );

    bool discoverPlugConnection( AvPlug& srcPlug,
                                 SubunitPlugSpecificDataPlugAddress& subunitPlugAddress );

    bool enumerateSubUnits();

    AvDeviceSubunit* getSubunit( subunit_type_t subunitType,
                                 subunit_id_t subunitId ) const;

    unsigned int getNrOfSubunits( subunit_type_t subunitType ) const;
    AvPlugConnection* getPlugConnection( AvPlug& srcPlug ) const;


    AvPlug* getPlugById( AvPlugVector& plugs, int id );
    bool addXmlDescriptionPlug( AvPlug& plug, xmlNodePtr conectionSet );
    bool addXmlDescriptionStreamFormats( AvPlug& plug, xmlNodePtr streamFormats );

	bool addPlugToProcessor( AvPlug& plug, FreebobStreaming::StreamProcessor *processor, 
	                         FreebobStreaming::AmdtpAudioPort::E_Direction direction);

    bool setSamplingFrequencyPlug( AvPlug& plug,
                                   PlugAddress::EPlugDirection direction,
                                   ESamplingFrequency samplingFrequency );
protected:
    Ieee1394Service* m_1394Service;
    ConfigRom*       m_configRom;
    int              m_nodeId;
    int              m_verboseLevel;

    AvPlugVector           m_isoInputPlugs;
    AvPlugVector           m_isoOutputPlugs;

    AvPlugConnectionVector m_plugConnections;

    AvDeviceSubunitVector  m_subunits;

    nr_of_plugs_t m_serialBusIsochronousInputPlugs;
    nr_of_plugs_t m_serialBusIsochronousOutputPlugs;

	// streaming stuff
	FreebobStreaming::AmdtpReceiveStreamProcessor *m_receiveProcessor;
	int m_receiveProcessorBandwidth;
	
	FreebobStreaming::AmdtpTransmitStreamProcessor *m_transmitProcessor;
	int m_transmitProcessorBandwidth;
	
	// FIXME: remove this debug line
	FreebobStreaming::AmdtpReceiveStreamProcessor *m_receiveProcessor2;
	int m_receiveProcessor2Bandwidth;


    DECLARE_DEBUG_MODULE;
};

}

#endif
