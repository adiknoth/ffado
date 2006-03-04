/* avdevice.cpp
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

#include "avdevice.h"
#include "configrom.h"
#include "avdevicesubunit.h"

#include "libfreebobavc/avc_plug_info.h"
#include "libfreebobavc/avc_extended_plug_info.h"
#include "libfreebobavc/avc_subunit_info.h"
#include "libfreebobavc/avc_extended_stream_format.h"
#include "libfreebobavc/serialize.h"
#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <stdint.h>

using namespace std;

IMPL_DEBUG_MODULE( AvDevice, AvDevice, DEBUG_LEVEL_NORMAL );

AvDevice::AvDevice( Ieee1394Service* ieee1394service,
                    ConfigRom* configRom,
                    int nodeId,
                    bool verbose )
    :  m_1394Service( ieee1394service )
    , m_configRom( configRom )
    , m_nodeId( nodeId )
    , m_verbose( verbose )
{
    if ( m_verbose ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Found AvDevice (NodeID %d)\n", nodeId );
}

AvDevice::~AvDevice()
{
    delete m_configRom;
    for ( AvDeviceSubunitVector::iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        delete *it;
    }
    for ( AvPlugConnectionVector::iterator it = m_plugConnections.begin();
          it != m_plugConnections.end();
          ++it )
    {
        delete *it;
    }
    for ( AvPlugVector::iterator it = m_pcrPlugs.begin();
          it != m_pcrPlugs.end();
          ++it )
    {
        delete *it;
    }
    for ( AvPlugVector::iterator it = m_externalPlugs.begin();
          it != m_externalPlugs.end();
          ++it )
    {
        delete *it;
    }
}

bool
AvDevice::discover()
{
    if ( !enumerateSubUnits() ) {
        debugError( "Could not enumarate sub units\n" );
        return false;
    }
    if ( !discoverPlugs() ) {
        debugError( "Detecting plugs failed\n" );
        return false;
    }
    if ( !discoverPlugConnections() ) {
        debugError( "Detecting plug connections failed\n" );
        return false;
    }
    if ( !discoverSyncModes() ) {
        debugError( "Detecting sync modes failed\n" );
        return false;
    }

    return true;
}

bool
AvDevice::discoverPlugs()
{
    //////////////////////////////////////////////
    // Get number of available isochronous input
    // and output plugs of unit

    PlugInfoCmd plugInfoCmd( m_1394Service );
    plugInfoCmd.setNodeId( m_nodeId );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );

    if ( !plugInfoCmd.fire() ) {
        debugError( "plug info command failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_NORMAL, "\n"
                 "\tnumber of iso input plugs = %d\n"
                 "\tnumber of iso output plugs = %d\n"
                 "\tnumber of external input plugs = %d\n "
                 "\tnumber of external output plugs = %d\n",
                 plugInfoCmd.m_serialBusIsochronousInputPlugs,
                 plugInfoCmd.m_serialBusIsochronousOutputPlugs,
                 plugInfoCmd.m_externalInputPlugs,
                 plugInfoCmd.m_externalOutputPlugs );


    if ( !discoverPlugsPCR( AvPlug::eAPD_Input,
                            plugInfoCmd.m_serialBusIsochronousInputPlugs ) )
    {
        debugError( "pcr input plug discovering failed\n" );
        return false;
    }

    if ( !discoverPlugsPCR( AvPlug::eAPD_Output,
                            plugInfoCmd.m_serialBusIsochronousOutputPlugs ) )
    {
        debugError( "pcr output plug discovering failed\n" );
        return false;
    }

    if ( !discoverPlugsExternal( AvPlug::eAPD_Input,
                                 plugInfoCmd.m_externalInputPlugs ) )
    {
        debugError( "external input plug discovering failed\n" );
        return false;
    }

    if ( !discoverPlugsExternal( AvPlug::eAPD_Output,
                                 plugInfoCmd.m_externalOutputPlugs ) )
    {
        debugError( "external output plug discovering failed\n" );
        return false;
    }

    return true;
}

bool
AvDevice::discoverPlugsPCR( AvPlug::EAvPlugDirection plugDirection,
                            plug_id_t plugMaxId )
{
    for ( int plugId = 0;
          plugId < plugMaxId;
          ++plugId )
    {
        AvPlug* plug  = new AvPlug( *m_1394Service,
                                    m_nodeId,
                                    m_plugManager,
                                    AVCCommand::eST_Unit,
                                    0xff,
                                    AvPlug::eAPA_PCR,
                                    plugDirection,
                                    plugId,
                                    m_verbose );
        if ( !plug || !plug->discover() ) {
            debugError( "plug discovering failed\n" );
            return false;
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "plug '%s' found\n",
                     plug->getName() );
        m_pcrPlugs.push_back( plug );
    }

    return true;
}

bool
AvDevice::discoverPlugsExternal( AvPlug::EAvPlugDirection plugDirection,
                                 plug_id_t plugMaxId )
{
    for ( int plugId = 0;
          plugId < plugMaxId;
          ++plugId )
    {
        AvPlug* plug  = new AvPlug( *m_1394Service,
                                    m_nodeId,
                                    m_plugManager,
                                    AVCCommand::eST_Unit,
                                    0xff,
                                    AvPlug::eAPA_ExternalPlug,
                                    plugDirection,
                                    plugId,
                                    m_verbose );
        if ( !plug || !plug->discover() ) {
            debugError( "plug discovering failed\n" );
            return false;
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "plug '%s' found\n",
                     plug->getName() );
        m_externalPlugs.push_back( plug );
    }

    return true;
}

bool
AvDevice::discoverPlugConnections()
{

    for ( AvPlugVector::iterator it = m_pcrPlugs.begin();
          it != m_pcrPlugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( !plug->discoverConnections() ) {
            debugError( "Could not discover plug connections\n" );
            return false;
        }
    }
    for ( AvPlugVector::iterator it = m_externalPlugs.begin();
          it != m_externalPlugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( !plug->discoverConnections() ) {
            debugError( "Could not discover plug connections\n" );
            return false;
        }
    }

    return true;
}

bool
AvDevice::discoverSyncModes()
{
    // Following possible sync plugs exists:
    // - Music subunit sync output plug = internal sync (CSP)
    // - Unit input plug 0 = SYT match
    // - Unit input plut 1 = Sync stream
    //
    // If last sync mode is reported it is mostelikely not
    // implemented *sic*
    //
    // Following sync sources are device specific:
    // - All unit external input plugs which have a
    //   sync information (WS, SPDIF, ...)

    // First we have to find the sync input and output plug
    // in the music subunit.

    AvPlug* syncInputPlug = getPlugByType( m_pcrPlugs,
                                           AvPlug::eAPD_Input,
                                           AvPlug::eAPT_Sync );
    if ( !syncInputPlug ) {
        debugError( "No sync input plug found\n" );
        return false;
    }

    AvPlug* syncOutputPlug = getPlugByType( m_pcrPlugs,
                                            AvPlug::eAPD_Output,
                                            AvPlug::eAPT_Sync );
    if ( !syncOutputPlug ) {
        debugError( "No sync output plug found with\n" );
        return false;
    }

    AvPlug* syncMSUInputPlug = m_plugManager.getPlugByType( AVCCommand::eST_Music,
                                                            0,
                                                            AvPlug::eAPA_SubunitPlug,
                                                            AvPlug::eAPD_Input,
                                                            AvPlug::eAPT_Sync );
    if ( !syncMSUInputPlug ) {
        debugError( "No sync input plug for MSU subunit found\n" );
        return false;
    }

    AvPlug* syncMSUOutputPlug = m_plugManager.getPlugByType( AVCCommand::eST_Music,
                                                             0,
                                                             AvPlug::eAPA_SubunitPlug,
                                                             AvPlug::eAPD_Output,
                                                             AvPlug::eAPT_Sync );
    if ( !syncMSUOutputPlug ) {
        debugError( "No sync output plug for MSU subunit found\n" );
        return false;
    }


    debugOutput( DEBUG_LEVEL_VERBOSE, "syncInputPlug = '%s'\n", syncInputPlug->getName() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "syncOutputPlug = '%s'\n", syncOutputPlug->getName() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "syncMSUInputPlug = '%s'\n", syncMSUInputPlug->getName() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "syncMSUOutputPlug = '%s'\n", syncMSUOutputPlug->getName() );


    // Just some simple tests with the plugs. Some more inquiries are needed.
    if ( syncOutputPlug->inquireConnnection( *syncMSUInputPlug ) ) {
        debugOutput( DEBUG_LEVEL_NORMAL, "Sync connection '%s' -> '%s'\n",
                     syncOutputPlug->getName(),
                     syncMSUInputPlug->getName() );
    }

    if ( syncMSUOutputPlug->inquireConnnection( *syncInputPlug ) ) {
        debugOutput( DEBUG_LEVEL_NORMAL, "Sync connection '%s' -> '%s'\n",
                     syncMSUOutputPlug->getName(),
                     syncInputPlug->getName() );

    }

    /*
    for ( AvPlugVector::iterator it = m_pcrPlugs.begin();
          it != m_pcrPlugs.end();
          ++it )
    {
        AvPlug* plug = *it;

        if ( syncOutputPlug->inquireConnnection( *plug ) ) {
            debugOutput( DEBUG_LEVEL_NORMAL, "Sync connection '%s' -> '%s'\n",
                         syncInputPlug->getName(),
                         plug->getName() );
        }
    }

    for ( AvPlugVector::iterator it = m_externalPlugs.begin();
          it != m_externalPlugs.end();
          ++it )
    {
        AvPlug* plug = *it;

        if ( syncOutputPlug->inquireConnnection( *plug ) ) {
            debugOutput( DEBUG_LEVEL_NORMAL, "Sync connection '%s' -> '%s'\n",
                         syncInputPlug->getName(),
                         plug->getName() );
        }
    }
    */

    return true;
}

bool
AvDevice::enumerateSubUnits()
{
    SubUnitInfoCmd subUnitInfoCmd( m_1394Service );
    //subUnitInfoCmd.setVerbose( 1 );
    subUnitInfoCmd.setCommandType( AVCCommand::eCT_Status );

    // BeBob has always exactly one audio and one music subunit. This
    // means is fits into the first page of the SubUnitInfo command.
    // So there is no need to do more than needed

    subUnitInfoCmd.m_page = 0;
    subUnitInfoCmd.setNodeId( m_nodeId );
    if ( !subUnitInfoCmd.fire() ) {
        debugError( "Subunit info command failed\n" );
    }

    for ( int i = 0; i < subUnitInfoCmd.getNrOfValidEntries(); ++i ) {
        subunit_type_t subunit_type
            = subUnitInfoCmd.m_table[i].m_subunit_type;

        unsigned int subunitId = getNrOfSubunits( subunit_type );

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "subunit_id = %2d, subunit_type = %2d (%s)\n",
                     subunitId,
                     subunit_type,
                     subunitTypeToString( subunit_type ) );

        AvDeviceSubunit* subunit = 0;
        switch( subunit_type ) {
        case AVCCommand::eST_Audio:
            subunit = new AvDeviceSubunitAudio( *this, subunitId, m_verbose );
            if ( !subunit ) {
                debugFatal( "Could not allocate AvDeviceSubunitAudio\n" );
                return false;
            }
            break;
        case AVCCommand::eST_Music:
            subunit = new AvDeviceSubunitMusic( *this, subunitId, m_verbose );
            if ( !subunit ) {
                debugFatal( "Could not allocate AvDeviceSubunitMusic\n" );
                return false;
            }
            break;
        default:
            debugOutput( DEBUG_LEVEL_NORMAL,
                         "Unsupported subunit found, subunit_type = %d (%s)\n",
                         subunit_type,
                         subunitTypeToString( subunit_type ) );
            continue;

        }

        if ( !subunit->discover() ) {
            debugError( "enumerateSubUnits: Could not discover "
                        "subunit_id = %2d, subunit_type = %2d (%s)\n",
                        subunitId,
                        subunit_type,
                        subunitTypeToString( subunit_type ) );
            delete subunit;
            return false;
        }
        m_subunits.push_back( subunit );
    }

    return true;
}


AvDeviceSubunit*
AvDevice::getSubunit( subunit_type_t subunitType,
                      subunit_id_t subunitId ) const
{
    for ( AvDeviceSubunitVector::const_iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        AvDeviceSubunit* subunit = *it;
        if ( ( subunitType == subunit->getSubunitType() )
             && ( subunitId == subunit->getSubunitId() ) )
        {
            return subunit;
        }
    }

    return 0;
}


unsigned int
AvDevice::getNrOfSubunits( subunit_type_t subunitType ) const
{
    unsigned int nrOfSubunits = 0;

    for ( AvDeviceSubunitVector::const_iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        AvDeviceSubunit* subunit = *it;
        if ( subunitType == subunit->getSubunitType() ) {
            nrOfSubunits++;
        }
    }

    return nrOfSubunits;
}

AvPlugConnection*
AvDevice::getPlugConnection( AvPlug& srcPlug ) const
{
    for ( AvPlugConnectionVector::const_iterator it
              = m_plugConnections.begin();
          it != m_plugConnections.end();
          ++it )
    {
        AvPlugConnection* plugConnection = *it;
        if ( plugConnection->m_srcPlug == &srcPlug ) {
            return plugConnection;
        }
    }

    return 0;
}

AvPlug*
AvDevice::getPlugById( AvPlugVector& plugs, AvPlug::EAvPlugDirection plugDirection, int id )
{
    for ( AvPlugVector::iterator it = plugs.begin();
          it != plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( ( id == plug->getPlugId() )
             && ( plugDirection == plug->getPlugDirection() ) )
        {
            return plug;
        }
    }

    return 0;
}

AvPlug*
AvDevice::getPlugByType( AvPlugVector& plugs,
                         AvPlug::EAvPlugDirection plugDirection,
                         AvPlug::EAvPlugType type)
{
    for ( AvPlugVector::iterator it = plugs.begin();
          it != plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( ( type == plug->getPlugType() )
             && ( plugDirection == plug->getPlugDirection() ) )
        {
            return plug;
        }
    }

    return 0;
}

AvPlug*
AvDevice::getSyncPlug( int maxPlugId, AvPlug::EAvPlugDirection )
{
    return 0;
}

std::string
AvDevice::getVendorName()
{
    return m_configRom->getVendorName();
}

std::string
AvDevice::getModelName()
{
    return m_configRom->getModelName();
}

uint64_t
AvDevice::getGuid()
{
    return m_configRom->getGuid();
}

bool
AvDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency )
{
    AvPlug* plug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Input, 0 );
    if ( !plug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }

    if ( !setSamplingFrequencyPlug( *plug, AvPlug::eAPD_Input, samplingFrequency ) ) {
        debugError( "setSampleRate: Setting sample rate failed\n" );
        return false;
    }

    plug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Output,  0 );
    if ( !plug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

    if ( !setSamplingFrequencyPlug( *plug, AvPlug::eAPD_Output, samplingFrequency ) ) {
        debugError( "setSampleRate: Setting sample rate failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "setSampleRate: Set sample rate to %d\n",
                 convertESamplingFrequency( samplingFrequency ) );
    return true;
}

bool
AvDevice::setSamplingFrequencyPlug( AvPlug& plug,
                                    AvPlug::EAvPlugDirection direction,
                                    ESamplingFrequency samplingFrequency )
{
    ExtendedStreamFormatCmd extStreamFormatCmd( m_1394Service,
                                                ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     plug.getPlugId() );

    PlugAddress::EPlugDirection dir;
    switch ( direction ) {
    case AvPlug::eAPD_Input:
        dir = PlugAddress::ePD_Input;
        break;
    case AvPlug::eAPD_Output:
        dir = PlugAddress::ePD_Output;
        break;
    default:
        return false;
    }

    extStreamFormatCmd.setPlugAddress( PlugAddress( dir,
                                                    PlugAddress::ePAM_Unit,
                                                    unitPlugAddress ) );

    extStreamFormatCmd.setNodeId( m_nodeId );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
    //extStreamFormatCmd.setVerbose( true );

    int i = 0;
    bool cmdSuccess = false;
    bool correctFormatFound = false;

    do {
        extStreamFormatCmd.setIndexInStreamFormat( i );
        extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );

        cmdSuccess = extStreamFormatCmd.fire();
        if ( cmdSuccess
             && ( extStreamFormatCmd.getResponse() == AVCCommand::eR_Implemented ) )
        {
            ESamplingFrequency foundFreq = eSF_DontCare;

            FormatInformation* formatInfo =
                extStreamFormatCmd.getFormatInformation();
            FormatInformationStreamsCompound* compoundStream
                = dynamic_cast< FormatInformationStreamsCompound* > (
                    formatInfo->m_streams );
            if ( compoundStream ) {
                foundFreq = static_cast<ESamplingFrequency>( compoundStream->m_samplingFrequency );
            }

            FormatInformationStreamsSync* syncStream
                = dynamic_cast< FormatInformationStreamsSync* > (
                    formatInfo->m_streams );
            if ( syncStream ) {
                foundFreq = static_cast<ESamplingFrequency>( compoundStream->m_samplingFrequency );
            }

            if (  foundFreq == samplingFrequency )
            {
                correctFormatFound = true;
                break;
            }
        }

        ++i;
    } while ( cmdSuccess && ( extStreamFormatCmd.getResponse()
                              == ExtendedStreamFormatCmd::eR_Implemented ) );

    if ( !cmdSuccess ) {
        debugError( "setSampleRatePlug: Failed to retrieve format info\n" );
        return false;
    }

    if ( !correctFormatFound ) {
        debugError( "setSampleRatePlug: %s plug %d does not support sample rate %d\n",
                    plug.getName(),
                    plug.getPlugId(),
                    convertESamplingFrequency( samplingFrequency ) );
        return false;
    }

    extStreamFormatCmd.setSubFunction(
        ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Control );

    if ( !extStreamFormatCmd.fire() ) {
        debugError( "setSampleRate: Could not set sample rate %d "
                    "to %s plug %d\n",
                    convertESamplingFrequency( samplingFrequency ),
                    plug.getName(),
                    plug.getPlugId() );
        return false;
    }

    return true;
}

void
AvDevice::showDevice()
{
    m_plugManager.showPlugs();
}
