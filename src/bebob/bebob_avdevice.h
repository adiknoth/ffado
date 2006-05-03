/* bebob_avdevice.h
 * Copyright (C) 2005,06 by Daniel Wagner
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

#ifndef BEBOB_AVDEVICE_H
#define BEBOB_AVDEVICE_H

#include <stdint.h>

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebobavc/avc_extended_cmd_generic.h"
#include "libfreebob/xmlparser.h"

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_avdevice_subunit.h"

#include "iavdevice.h"

class ConfigRom;
class Ieee1394Service;
class SubunitPlugSpecificDataPlugAddress;

namespace BeBoB {

class AvDevice : public IAvDevice {
public:
    AvDevice( Ieee1394Service& ieee1394Service,
              int nodeId,
	      int verboseLevel );
    virtual ~AvDevice();

    virtual bool discover();
    virtual ConfigRom& getConfigRom() const;
    virtual bool addXmlDescription( xmlNodePtr deviceNode );
    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual void showDevice() const;

    Ieee1394Service* get1394Service()
	{ return m_1394Service; }

    AvPlugManager& getPlugManager()
	{ return m_plugManager; }

protected:
    bool enumerateSubUnits();

    bool discoverPlugs();
    bool discoverPlugsPCR( AvPlug::EAvPlugDirection plugDirection,
                           plug_id_t plugMaxId );
    bool discoverPlugsExternal( AvPlug::EAvPlugDirection plugDirection,
                                plug_id_t plugMaxId );
    bool discoverPlugConnections();
    bool discoverSyncModes();
    bool discoverSubUnitsPlugConnections();

    AvDeviceSubunit* getSubunit( subunit_type_t subunitType,
                                 subunit_id_t subunitId ) const;

    unsigned int getNrOfSubunits( subunit_type_t subunitType ) const;
    AvPlugConnection* getPlugConnection( AvPlug& srcPlug ) const;

    AvPlug* getSyncPlug( int maxPlugId, AvPlug::EAvPlugDirection );

    AvPlug* getPlugById( AvPlugVector& plugs,
                         AvPlug::EAvPlugDirection plugDireciton,
                         int id );
    AvPlugVector getPlugsByType( AvPlugVector& plugs,
				 AvPlug::EAvPlugDirection plugDirection,
				 AvPlug::EAvPlugType type);

    bool setSamplingFrequencyPlug( AvPlug& plug,
                                   AvPlug::EAvPlugDirection direction,
                                   ESamplingFrequency samplingFrequency );

    void showAvPlugs( AvPlugVector& plugs ) const;

    bool checkSyncConnections( AvPlugVector& plhs, AvPlugVector& prhs );
protected:
    Ieee1394Service* m_1394Service;
    ConfigRom*       m_configRom;
    int              m_nodeId;
    int              m_verboseLevel;

    AvPlugVector     m_pcrPlugs;
    AvPlugVector     m_externalPlugs;

    AvPlugConnectionVector m_plugConnections;

    AvDeviceSubunitVector  m_subunits;

    AvPlugManager    m_plugManager;

    DECLARE_DEBUG_MODULE;
};

}

#endif
