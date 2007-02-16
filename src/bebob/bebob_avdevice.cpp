/* bebob_avdevice.cpp
 * Copyright (C) 2005,06,07 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "bebob/bebob_avdevice.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "configrom.h"

#include "libfreebobavc/avc_plug_info.h"
#include "libfreebobavc/avc_extended_plug_info.h"
#include "libfreebobavc/avc_subunit_info.h"
#include "libfreebobavc/avc_extended_stream_format.h"
#include "libfreebobavc/avc_serialize.h"
#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>

namespace BeBoB {

IMPL_DEBUG_MODULE( AvDevice, AvDevice, DEBUG_LEVEL_NORMAL );

AvDevice::AvDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId,
                    int verboseLevel )
    : m_pConfigRom( configRom )
    , m_p1394Service( &ieee1394service )
    , m_verboseLevel( verboseLevel )
    , m_pPlugManager( new AvPlugManager( verboseLevel ) )
    , m_activeSyncInfo( 0 )
    , m_id( 0 )
    , m_receiveProcessor ( 0 )
    , m_receiveProcessorBandwidth ( -1 )
    , m_transmitProcessor ( 0 )
    , m_transmitProcessorBandwidth ( -1 )
{
    setDebugLevel( verboseLevel );
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::AvDevice (NodeID %d)\n",
                 nodeId );
}

AvDevice::AvDevice()
    : m_pConfigRom( 0 )
    , m_p1394Service( 0 )
    , m_verboseLevel( 0 )
    , m_pPlugManager( 0 )
    , m_activeSyncInfo( 0 )
    , m_id( 0 )
    , m_receiveProcessor ( 0 )
    , m_receiveProcessorBandwidth ( -1 )
    , m_transmitProcessor ( 0 )
    , m_transmitProcessorBandwidth ( -1 )
{
}

AvDevice::~AvDevice()
{
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

ConfigRom&
AvDevice::getConfigRom() const
{
    return *m_pConfigRom;
}

struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
};

static VendorModelEntry supportedDeviceList[] =
{
    {0x00000f, 0x00010065},  // Mackie, Onyx Firewire

    {0x0003db, 0x00010048},  // Apogee Electronics, Rosetta 200

    {0x0007f5, 0x00010048},  // BridgeCo, RD Audio1

    {0x000a92, 0x00010000},  // PreSonus FIREBOX
    {0x000a92, 0x00010066},  // PreSonus FirePOD

    {0x000aac, 0x00000003},  // TerraTec Electronic GmbH, Phase 88 FW
    {0x000aac, 0x00000004},  // TerraTec Electronic GmbH, Phase X24 FW (model version 4)
    {0x000aac, 0x00000007},  // TerraTec Electronic GmbH, Phase X24 FW (model version 7)

    {0x000f1b, 0x00010064},  // ESI, Quatafire 610

    {0x00130e, 0x00000003},  // Focusrite, Pro26IO (Saffire 26)

    {0x0040ab, 0x00010048},  // EDIROL, FA-101
    {0x0040ab, 0x00010049},  // EDIROL, FA-66
};

bool
AvDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId ) )
        {
            return true;
        }
    }

    return false;
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

    if ( !discoverSubUnitsPlugConnections() ) {
        debugError( "Detecting plug connnection failed\n" );
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

    PlugInfoCmd plugInfoCmd( *m_p1394Service );
    plugInfoCmd.setNodeId( m_pConfigRom->getNodeId() );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    plugInfoCmd.setVerbose( m_verboseLevel );

    if ( !plugInfoCmd.fire() ) {
        debugError( "plug info command failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_NORMAL, "number of iso input plugs = %d\n",
                 plugInfoCmd.m_serialBusIsochronousInputPlugs );
    debugOutput( DEBUG_LEVEL_NORMAL, "number of iso output plugs = %d\n",
                 plugInfoCmd.m_serialBusIsochronousOutputPlugs );
    debugOutput( DEBUG_LEVEL_NORMAL, "number of external input plugs = %d\n",
                 plugInfoCmd.m_externalInputPlugs );
    debugOutput( DEBUG_LEVEL_NORMAL, "number of external output plugs = %d\n",
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
        AvPlug* plug  = new AvPlug( *m_p1394Service,
                                    *m_pConfigRom,
                                    *m_pPlugManager,
                                    AVCCommand::eST_Unit,
                                    0xff,
                                    0xff,
                                    0xff,
                                    AvPlug::eAPA_PCR,
                                    plugDirection,
                                    plugId,
                                    m_verboseLevel );
        if ( !plug || !plug->discover() ) {
            debugError( "plug discovering failed\n" );
            delete plug;
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
        AvPlug* plug  = new AvPlug( *m_p1394Service,
                                    *m_pConfigRom,
                                    *m_pPlugManager,
                                    AVCCommand::eST_Unit,
                                    0xff,
                                    0xff,
                                    0xff,
                                    AvPlug::eAPA_ExternalPlug,
                                    plugDirection,
                                    plugId,
                                    m_verboseLevel );
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
AvDevice::discoverSubUnitsPlugConnections()
{
    for ( AvDeviceSubunitVector::iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        AvDeviceSubunit* subunit = *it;
        if ( !subunit->discoverConnections() ) {
            debugError( "Subunit '%s'  plug connections failed\n",
                        subunit->getName() );
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

    // Note PCR input means 1394bus-to-device where as
    // MSU input means subunit-to-device

    AvPlugVector syncPCRInputPlugs = getPlugsByType( m_pcrPlugs,
                                                     AvPlug::eAPD_Input,
                                                     AvPlug::eAPT_Sync );
    if ( !syncPCRInputPlugs.size() ) {
        debugWarning( "No PCR sync input plug found\n" );
    }

    AvPlugVector syncPCROutputPlugs = getPlugsByType( m_pcrPlugs,
                                                      AvPlug::eAPD_Output,
                                                      AvPlug::eAPT_Sync );
    if ( !syncPCROutputPlugs.size() ) {
        debugWarning( "No PCR sync output plug found\n" );
    }

    AvPlugVector isoPCRInputPlugs = getPlugsByType( m_pcrPlugs,
                                                    AvPlug::eAPD_Input,
                                                    AvPlug::eAPT_IsoStream );
    if ( !isoPCRInputPlugs.size() ) {
        debugWarning( "No PCR iso input plug found\n" );

    }

    AvPlugVector isoPCROutputPlugs = getPlugsByType( m_pcrPlugs,
                                                    AvPlug::eAPD_Output,
                                                    AvPlug::eAPT_IsoStream );
    if ( !isoPCROutputPlugs.size() ) {
        debugWarning( "No PCR iso output plug found\n" );

    }

    AvPlugVector digitalPCRInputPlugs = getPlugsByType( m_externalPlugs,
                                                    AvPlug::eAPD_Input,
                                                    AvPlug::eAPT_Digital );

    AvPlugVector syncMSUInputPlugs = m_pPlugManager->getPlugsByType(
        AVCCommand::eST_Music,
        0,
        0xff,
        0xff,
        AvPlug::eAPA_SubunitPlug,
        AvPlug::eAPD_Input,
        AvPlug::eAPT_Sync );
    if ( !syncMSUInputPlugs.size() ) {
        debugWarning( "No sync input plug for MSU subunit found\n" );
    }

    AvPlugVector syncMSUOutputPlugs = m_pPlugManager->getPlugsByType(
        AVCCommand::eST_Music,
        0,
        0xff,
        0xff,
        AvPlug::eAPA_SubunitPlug,
        AvPlug::eAPD_Output,
        AvPlug::eAPT_Sync );
    if ( !syncMSUOutputPlugs.size() ) {
        debugWarning( "No sync output plug for MSU subunit found\n" );
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "PCR Sync Input Plugs:\n" );
    showAvPlugs( syncPCRInputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "PCR Sync Output Plugs:\n" );
    showAvPlugs( syncPCROutputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "PCR Iso Input Plugs:\n" );
    showAvPlugs( isoPCRInputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "PCR Iso Output Plugs:\n" );
    showAvPlugs( isoPCROutputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "PCR digital Input Plugs:\n" );
    showAvPlugs( digitalPCRInputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "MSU Sync Input Plugs:\n" );
    showAvPlugs( syncMSUInputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "MSU Sync Output Plugs:\n" );
    showAvPlugs( syncMSUOutputPlugs );

    // Check all possible PCR input to MSU input connections
    // -> sync stream input
    checkSyncConnectionsAndAddToList( syncPCRInputPlugs,
                                      syncMSUInputPlugs,
                                      "Sync Stream Input" );

    // Check all possible MSU output to PCR output connections
    // -> sync stream output
    checkSyncConnectionsAndAddToList( syncMSUOutputPlugs,
                                      syncPCROutputPlugs,
                                      "Sync Stream Output" );

    // Check all PCR iso input to MSU input connections
    // -> SYT match
    checkSyncConnectionsAndAddToList( isoPCRInputPlugs,
                                      syncMSUInputPlugs,
                                      "Syt Match" );

    // Check all MSU sync output to MSU input connections
    // -> CSP
    checkSyncConnectionsAndAddToList( syncMSUOutputPlugs,
                                      syncMSUInputPlugs,
                                      "Internal (CSP)" );

    // Check all external PCR digital input to MSU input connections
    // -> SPDIF/ADAT sync
    checkSyncConnectionsAndAddToList( digitalPCRInputPlugs,
                                      syncMSUInputPlugs,
                                      "Digital Input Sync" );

    // Currently active connection signal source cmd, command type
    // status, source unknown, destination MSU sync input plug

    for ( AvPlugVector::const_iterator it = syncMSUInputPlugs.begin();
          it != syncMSUInputPlugs.end();
          ++it )
    {
        AvPlug* msuPlug = *it;
        for ( AvPlugVector::const_iterator jt =
                  msuPlug->getInputConnections().begin();
              jt != msuPlug->getInputConnections().end();
              ++jt )
        {
            AvPlug* plug = *jt;

            for ( SyncInfoVector::iterator it = m_syncInfos.begin();
                  it != m_syncInfos.end();
                  ++it )
            {
                SyncInfo* pSyncInfo = &*it;
                if ( ( pSyncInfo->m_source == plug )
                     && ( pSyncInfo->m_destination == msuPlug ) )
                {
                    m_activeSyncInfo = pSyncInfo;
                    break;
                }
            }
            debugOutput( DEBUG_LEVEL_NORMAL,
                         "Active Sync Connection: '%s' -> '%s'\n",
                         plug->getName(),
                         msuPlug->getName() );
        }
    }

    return true;
}

bool
AvDevice::enumerateSubUnits()
{
    bool musicSubunitFound=false;
    bool audioSubunitFound=false;

    SubUnitInfoCmd subUnitInfoCmd( *m_p1394Service );
    //subUnitInfoCmd.setVerbose( 1 );
    subUnitInfoCmd.setCommandType( AVCCommand::eCT_Status );

    // BeBoB has always exactly one audio and one music subunit. This
    // means is fits into the first page of the SubUnitInfo command.
    // So there is no need to do more than needed

    subUnitInfoCmd.m_page = 0;
    subUnitInfoCmd.setNodeId( m_pConfigRom->getNodeId() );
    subUnitInfoCmd.setVerbose( m_verboseLevel );
    if ( !subUnitInfoCmd.fire() ) {
        debugError( "Subunit info command failed\n" );
        // shouldn't this be an error situation?
        return false;
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
            subunit = new AvDeviceSubunitAudio( *this,
                                                subunitId,
                                                m_verboseLevel );
            if ( !subunit ) {
                debugFatal( "Could not allocate AvDeviceSubunitAudio\n" );
                return false;
            }

            m_subunits.push_back( subunit );
            audioSubunitFound=true;

            break;
        case AVCCommand::eST_Music:
            subunit = new AvDeviceSubunitMusic( *this,
                                                subunitId,
                                                m_verboseLevel );
            if ( !subunit ) {
                debugFatal( "Could not allocate AvDeviceSubunitMusic\n" );
                return false;
            }

            m_subunits.push_back( subunit );
            musicSubunitFound=true;

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

    }

    // a BeBoB always has an audio and a music subunit
    return (musicSubunitFound && audioSubunitFound);
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
        if ( &( plugConnection->getSrcPlug() ) == &srcPlug ) {
            return plugConnection;
        }
    }

    return 0;
}

AvPlug*
AvDevice::getPlugById( AvPlugVector& plugs,
                       AvPlug::EAvPlugDirection plugDirection,
                       int id )
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

AvPlugVector
AvDevice::getPlugsByType( AvPlugVector& plugs,
                          AvPlug::EAvPlugDirection plugDirection,
                          AvPlug::EAvPlugType type)
{
    AvPlugVector plugVector;
    for ( AvPlugVector::iterator it = plugs.begin();
          it != plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( ( type == plug->getPlugType() )
             && ( plugDirection == plug->getPlugDirection() ) )
        {
            plugVector.push_back( plug );
        }
    }

    return plugVector;
}

AvPlug*
AvDevice::getSyncPlug( int maxPlugId, AvPlug::EAvPlugDirection )
{
    return 0;
}

bool
AvDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency )
{

    AvPlug* plug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Input, 0 );
    if ( !plug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }

    if ( !setSamplingFrequencyPlug( *plug,
                                    AvPlug::eAPD_Input,
                                    samplingFrequency ) )
    {
        debugError( "setSampleRate: Setting sample rate failed\n" );
        return false;
    }

    plug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Output,  0 );
    if ( !plug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

    if ( !setSamplingFrequencyPlug( *plug,
                                    AvPlug::eAPD_Output,
                                    samplingFrequency ) )
    {
        debugError( "setSampleRate: Setting sample rate failed\n" );
        return false;
    }


    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "setSampleRate: Set sample rate to %d\n",
                 convertESamplingFrequency( samplingFrequency ) );
    return true;
}

int
AvDevice::getSamplingFrequency( ) {
    AvPlug* inputPlug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Input, 0 );
    if ( !inputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }
    AvPlug* outputPlug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Output, 0 );
    if ( !outputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

    int samplerate_playback=inputPlug->getSampleRate();
    int samplerate_capture=outputPlug->getSampleRate();

    if (samplerate_playback != samplerate_capture) {
        debugWarning("Samplerates for capture and playback differ!\n");
    }
    return samplerate_capture;
}


bool
AvDevice::setSamplingFrequencyPlug( AvPlug& plug,
                                    AvPlug::EAvPlugDirection direction,
                                    ESamplingFrequency samplingFrequency )
{

    ExtendedStreamFormatCmd extStreamFormatCmd(
        *m_p1394Service,
        ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     plug.getPlugId() );

    extStreamFormatCmd.setPlugAddress(
        PlugAddress(
            AvPlug::convertPlugDirection(direction ),
            PlugAddress::ePAM_Unit,
            unitPlugAddress ) );

    extStreamFormatCmd.setNodeId( m_pConfigRom->getNodeId() );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );

    int i = 0;
    bool cmdSuccess = false;
    bool correctFormatFound = false;

    do {
        extStreamFormatCmd.setIndexInStreamFormat( i );
        extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
        extStreamFormatCmd.setVerbose( m_verboseLevel );

        cmdSuccess = extStreamFormatCmd.fire();

        if ( cmdSuccess
             && ( extStreamFormatCmd.getResponse() ==
                  AVCCommand::eR_Implemented ) )
        {
            ESamplingFrequency foundFreq = eSF_DontCare;

            FormatInformation* formatInfo =
                extStreamFormatCmd.getFormatInformation();
            FormatInformationStreamsCompound* compoundStream
                = dynamic_cast< FormatInformationStreamsCompound* > (
                    formatInfo->m_streams );
            if ( compoundStream ) {
                foundFreq =
                    static_cast< ESamplingFrequency >(
                        compoundStream->m_samplingFrequency );
            }

            FormatInformationStreamsSync* syncStream
                = dynamic_cast< FormatInformationStreamsSync* > (
                    formatInfo->m_streams );
            if ( syncStream ) {
                foundFreq =
                    static_cast< ESamplingFrequency >(
                        syncStream->m_samplingFrequency );
            }

            if ( foundFreq == samplingFrequency )
            {
                correctFormatFound = true;
                break;
            }
        }

        ++i;
    } while ( cmdSuccess
              && ( extStreamFormatCmd.getResponse() ==
                   ExtendedStreamFormatCmd::eR_Implemented ) );

    if ( !cmdSuccess ) {
        debugError( "setSampleRatePlug: Failed to retrieve format info\n" );
        return false;
    }

    if ( !correctFormatFound ) {
        debugError( "setSampleRatePlug: %s plug %d does not support "
                    "sample rate %d\n",
                    plug.getName(),
                    plug.getPlugId(),
                    convertESamplingFrequency( samplingFrequency ) );
        return false;
    }

    extStreamFormatCmd.setSubFunction(
        ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Control );
    extStreamFormatCmd.setVerbose( m_verboseLevel );

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
AvDevice::showDevice() const
{
    m_pPlugManager->showPlugs();
}

void
AvDevice::showAvPlugs( AvPlugVector& plugs ) const
{
    int i = 0;
    for ( AvPlugVector::const_iterator it = plugs.begin();
          it != plugs.end();
          ++it, ++i )
    {
        AvPlug* plug = *it;
        debugOutput( DEBUG_LEVEL_VERBOSE, "Plug %d\n", i );
        plug->showPlug();
    }
}

bool
AvDevice::checkSyncConnectionsAndAddToList( AvPlugVector& plhs,
                                            AvPlugVector& prhs,
                                            std::string syncDescription )
{
    for ( AvPlugVector::iterator plIt = plhs.begin();
          plIt != plhs.end();
          ++plIt )
    {
        AvPlug* pl = *plIt;
        for ( AvPlugVector::iterator prIt = prhs.begin();
              prIt != prhs.end();
              ++prIt )
        {
            AvPlug* pr = *prIt;
            if ( pl->inquireConnnection( *pr ) ) {
                m_syncInfos.push_back( SyncInfo( *pl, *pr, syncDescription ) );
                debugOutput( DEBUG_LEVEL_NORMAL,
                             "Sync connection '%s' -> '%s'\n",
                             pl->getName(),
                             pr->getName() );
            }
        }
    }
    return true;
}

bool AvDevice::setActiveSync(const SyncInfo& syncInfo)
{
    return syncInfo.m_source->setConnection( *syncInfo.m_destination );
}

bool AvDevice::setId( unsigned int id)
{
    // FIXME: decent ID system nescessary
    m_id=id;
	return true;
}

bool
AvDevice::prepare() {
    ///////////
    // get plugs

    AvPlug* inputPlug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Input, 0 );
    if ( !inputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }
    AvPlug* outputPlug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Output, 0 );
    if ( !outputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

    int samplerate=outputPlug->getSampleRate();
    m_receiveProcessor=new FreebobStreaming::AmdtpReceiveStreamProcessor(
                             m_p1394Service->getPort(),
                             samplerate,
                             outputPlug->getNrOfChannels());

    if(!m_receiveProcessor->init()) {
        debugFatal("Could not initialize receive processor!\n");
        return false;
    }

    if (!addPlugToProcessor(*outputPlug,m_receiveProcessor,
        FreebobStreaming::AmdtpAudioPort::E_Capture)) {
        debugFatal("Could not add plug to processor!\n");
        return false;
    }

    // do the transmit processor
//     if (m_snoopMode) {
//         // we are snooping, so these are receive too.
//         samplerate=inputPlug->getSampleRate();
//         m_receiveProcessor2=new FreebobStreaming::AmdtpReceiveStreamProcessor(
//                                   channel,
//                                   m_1394Service->getPort(),
//                                   samplerate,
//                                   inputPlug->getNrOfChannels());
//
//         if(!m_receiveProcessor2->init()) {
//             debugFatal("Could not initialize snooping receive processor!\n");
//             return false;
//         }
//         if (!addPlugToProcessor(*inputPlug,m_receiveProcessor2,
//             FreebobStreaming::AmdtpAudioPort::E_Capture)) {
//             debugFatal("Could not add plug to processor!\n");
//             return false;
//         }
//     } else {
        // do the transmit processor
        samplerate=inputPlug->getSampleRate();
        m_transmitProcessor=new FreebobStreaming::AmdtpTransmitStreamProcessor(
                                m_p1394Service->getPort(),
                                samplerate,
                                inputPlug->getNrOfChannels());

        if(!m_transmitProcessor->init()) {
            debugFatal("Could not initialize transmit processor!\n");
            return false;

        }

        if (!addPlugToProcessor(*inputPlug,m_transmitProcessor,
            FreebobStreaming::AmdtpAudioPort::E_Playback)) {
            debugFatal("Could not add plug to processor!\n");
            return false;
        }
//     }

    return true;
}

bool
AvDevice::addPlugToProcessor(
    AvPlug& plug,
    FreebobStreaming::StreamProcessor *processor,
    FreebobStreaming::AmdtpAudioPort::E_Direction direction) {

    AvPlug::ClusterInfoVector& clusterInfos = plug.getClusterInfos();
    for ( AvPlug::ClusterInfoVector::const_iterator it = clusterInfos.begin();
          it != clusterInfos.end();
          ++it )
    {
        const AvPlug::ClusterInfo* clusterInfo = &( *it );

        AvPlug::ChannelInfoVector channelInfos = clusterInfo->m_channelInfos;
        for ( AvPlug::ChannelInfoVector::const_iterator it = channelInfos.begin();
              it != channelInfos.end();
              ++it )
        {
            const AvPlug::ChannelInfo* channelInfo = &( *it );
            std::ostringstream portname;

            portname << "dev" << m_id << "_" << channelInfo->m_name;

            FreebobStreaming::Port *p=NULL;
            switch(clusterInfo->m_portType) {
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Speaker:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Headphone:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Microphone:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Line:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Analog:
                p=new FreebobStreaming::AmdtpAudioPort(
                        portname.str(),
                        direction,
                        // \todo: streaming backend expects indexing starting from 0
                        // but bebob reports it starting from 1. Decide where
                        // and how to handle this (pp: here)
                        channelInfo->m_streamPosition - 1,
                        channelInfo->m_location,
                        FreebobStreaming::AmdtpPortInfo::E_MBLA,
                        clusterInfo->m_portType
                );
                break;

            case ExtendedPlugInfoClusterInfoSpecificData::ePT_MIDI:
                p=new FreebobStreaming::AmdtpMidiPort(
                        portname.str(),
                        direction,
                        // \todo: streaming backend expects indexing starting from 0
                        // but bebob reports it starting from 1. Decide where
                        // and how to handle this (pp: here)
                        channelInfo->m_streamPosition - 1,
                        channelInfo->m_location,
                        FreebobStreaming::AmdtpPortInfo::E_Midi,
                        clusterInfo->m_portType
                );

                break;
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_SPDIF:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_ADAT:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_TDIF:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_MADI:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Digital:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_NoType:
            default:
            // unsupported
                break;
            }

            if (!p) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",channelInfo->m_name.c_str());
            } else {

                if (!processor->addPort(p)) {
                    debugWarning("Could not register port with stream processor\n");
                    return false;
                }
            }
         }
    }
    return true;
}

// #define TEST_XMIT_ONLY

#ifdef TEST_XMIT_ONLY
int
AvDevice::getStreamCount() {
//     return 2; // one receive, one transmit
    return 1; // one receive, one transmit
}

FreebobStreaming::StreamProcessor *
AvDevice::getStreamProcessorByIndex(int i) {
    switch (i) {
    case 0:
//         return m_receiveProcessor;
//     case 1:
//         if (m_snoopMode) {
//             return m_receiveProcessor2;
//         } else {
            return m_transmitProcessor;
//         }
    default:
        return NULL;
    }
    return 0;
}

// FIXME: error checking
int
AvDevice::startStreamByIndex(int i) {
    int iso_channel=0;
    int plug=0;
    int hostplug=-1;

//     if (m_snoopMode) {
//
//         switch (i) {
//         case 0:
//             // snooping doesn't use CMP, but obtains the info of the channel
//             // from the target plug
//
//             // TODO: get isochannel from plug
//
//             // set the channel obtained by the connection management
//             m_receiveProcessor->setChannel(iso_channel);
//             break;
//         case 1:
//             // snooping doesn't use CMP, but obtains the info of the channel
//             // from the target plug
//
//             // TODO: get isochannel from plug
//
//             // set the channel obtained by the connection management
//             m_receiveProcessor2->setChannel(iso_channel);
//
//             break;
//         default:
//             return 0;
//         }
//     } else {

        switch (i) {
        case 0:
//             // do connection management: make connection
//             iso_channel = iec61883_cmp_connect(
//                 m_p1394Service->getHandle(),
//                 m_pConfigRom->getNodeId() | 0xffc0,
//                 &plug,
//                 raw1394_get_local_id (m_p1394Service->getHandle()),
//                 &hostplug,
//                 &m_receiveProcessorBandwidth);
// 
//             // set the channel obtained by the connection management
//             m_receiveProcessor->setChannel(iso_channel);
//             break;
//         case 1:
            // do connection management: make connection
            iso_channel = iec61883_cmp_connect(
                m_p1394Service->getHandle(),
                raw1394_get_local_id (m_p1394Service->getHandle()),
                &hostplug,
                m_pConfigRom->getNodeId() | 0xffc0,
                &plug,
                &m_transmitProcessorBandwidth);

            // set the channel obtained by the connection management
            m_transmitProcessor->setChannel(iso_channel);
            break;
        default:
            return -1;
        }
//     }

    if (iso_channel < 0) return -1;
    
    return 0;

}

// FIXME: error checking
int
AvDevice::stopStreamByIndex(int i) {
    // do connection management: break connection

    int plug=0;
    int hostplug=-1;
//     if (m_snoopMode) {
//         switch (i) {
//         case 0:
//             // do connection management: break connection
//
//             break;
//         case 1:
//             // do connection management: break connection
//
//             break;
//         default:
//             return 0;
//         }
//     } else {
        switch (i) {
        case 0:
//             // do connection management: break connection
//             iec61883_cmp_disconnect(
//                 m_p1394Service->getHandle(),
//                 m_pConfigRom->getNodeId() | 0xffc0,
//                 plug,
//                 raw1394_get_local_id (m_p1394Service->getHandle()),
//                 hostplug,
//                 m_receiveProcessor->getChannel(),
//                 m_receiveProcessorBandwidth);
// 
//             break;
//         case 1:
            // do connection management: break connection
            iec61883_cmp_disconnect(
                m_p1394Service->getHandle(),
                raw1394_get_local_id (m_p1394Service->getHandle()),
                hostplug,
                m_pConfigRom->getNodeId() | 0xffc0,
                plug,
                m_transmitProcessor->getChannel(),
                m_transmitProcessorBandwidth);

            break;
        default:
            return 0;
        }
//     }

    return 0;
}
#else
int
AvDevice::getStreamCount() {
    return 2; // one receive, one transmit
}

FreebobStreaming::StreamProcessor *
AvDevice::getStreamProcessorByIndex(int i) {
    switch (i) {
    case 0:
        return m_receiveProcessor;
    case 1:
//         if (m_snoopMode) {
//             return m_receiveProcessor2;
//         } else {
            return m_transmitProcessor;
//         }
    default:
        return NULL;
    }
    return 0;
}

// FIXME: error checking
int
AvDevice::startStreamByIndex(int i) {
    int iso_channel=0;
    int plug=0;
    int hostplug=-1;

//     if (m_snoopMode) {
//
//         switch (i) {
//         case 0:
//             // snooping doesn't use CMP, but obtains the info of the channel
//             // from the target plug
//
//             // TODO: get isochannel from plug
//
//             // set the channel obtained by the connection management
//             m_receiveProcessor->setChannel(iso_channel);
//             break;
//         case 1:
//             // snooping doesn't use CMP, but obtains the info of the channel
//             // from the target plug
//
//             // TODO: get isochannel from plug
//
//             // set the channel obtained by the connection management
//             m_receiveProcessor2->setChannel(iso_channel);
//
//             break;
//         default:
//             return 0;
//         }
//     } else {

        switch (i) {
        case 0:
            // do connection management: make connection
            iso_channel = iec61883_cmp_connect(
                m_p1394Service->getHandle(),
                m_pConfigRom->getNodeId() | 0xffc0,
                &plug,
                raw1394_get_local_id (m_p1394Service->getHandle()),
                &hostplug,
                &m_receiveProcessorBandwidth);

            // set the channel obtained by the connection management
            m_receiveProcessor->setChannel(iso_channel);
            break;
        case 1:
            // do connection management: make connection
            iso_channel = iec61883_cmp_connect(
                m_p1394Service->getHandle(),
                raw1394_get_local_id (m_p1394Service->getHandle()),
                &hostplug,
                m_pConfigRom->getNodeId() | 0xffc0,
                &plug,
                &m_transmitProcessorBandwidth);

            // set the channel obtained by the connection management
            m_transmitProcessor->setChannel(iso_channel);
            break;
        default:
            return -1;
        }
//     }

    if (iso_channel < 0) return -1;
    
    return 0;

}

// FIXME: error checking
int
AvDevice::stopStreamByIndex(int i) {
    // do connection management: break connection

    int plug=0;
    int hostplug=-1;
//     if (m_snoopMode) {
//         switch (i) {
//         case 0:
//             // do connection management: break connection
//
//             break;
//         case 1:
//             // do connection management: break connection
//
//             break;
//         default:
//             return 0;
//         }
//     } else {
        switch (i) {
        case 0:
            // do connection management: break connection
            iec61883_cmp_disconnect(
                m_p1394Service->getHandle(),
                m_pConfigRom->getNodeId() | 0xffc0,
                plug,
                raw1394_get_local_id (m_p1394Service->getHandle()),
                hostplug,
                m_receiveProcessor->getChannel(),
                m_receiveProcessorBandwidth);

            break;
        case 1:
            // do connection management: break connection
            iec61883_cmp_disconnect(
                m_p1394Service->getHandle(),
                raw1394_get_local_id (m_p1394Service->getHandle()),
                hostplug,
                m_pConfigRom->getNodeId() | 0xffc0,
                plug,
                m_transmitProcessor->getChannel(),
                m_transmitProcessorBandwidth);

            break;
        default:
            return 0;
        }
//     }

    return 0;
}
#endif

template <typename T> bool serializeVector( Glib::ustring path,
                                            Util::IOSerialize& ser,
                                            const T& vec )
{
    bool result = true; // if vec.size() == 0
    int i = 0;
    for ( typename T::const_iterator it = vec.begin(); it != vec.end(); ++it ) {
        std::ostringstream strstrm;
        strstrm << path << i;
        result &= ( *it )->serialize( strstrm.str() + "/", ser );
        i++;
    }
    return result;
}

template <typename T, typename VT> bool deserializeVector( Glib::ustring path,
                                                           Util::IODeserialize& deser,
                                                           AvDevice& avDevice,
                                                           VT& vec )
{
    int i = 0;
    bool bFinished = false;
    do {
        std::ostringstream strstrm;
        strstrm << path << i << "/";
        T* ptr = T::deserialize( strstrm.str(),
                                 deser,
                                 avDevice );
        if ( ptr ) {
            vec.push_back( ptr );
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return true;
}

bool
AvDevice::serializeSyncInfoVector( Glib::ustring basePath,
                                   Util::IOSerialize& ser,
                                   const SyncInfoVector& vec )
{
    bool result = true;
    int i = 0;

    for ( SyncInfoVector::const_iterator it = vec.begin();
          it != vec.end();
          ++it )
    {
        const SyncInfo& info = *it;

        std::ostringstream strstrm;
        strstrm << basePath << i << "/";

        result &= ser.write( strstrm.str() + "m_source", info.m_source->getGlobalId() );
        result &= ser.write( strstrm.str() + "m_destination", info.m_destination->getGlobalId() );
        result &= ser.write( strstrm.str() + "m_description", Glib::ustring( info.m_description ) );

        i++;
    }

    return result;
}

bool
AvDevice::deserializeSyncInfoVector( Glib::ustring basePath,
                                     Util::IODeserialize& deser,
                                     AvDevice& avDevice,
                                     SyncInfoVector& vec )
{
    int i = 0;
    bool bFinished = false;
    do {
        bool result;
        std::ostringstream strstrm;
        strstrm << basePath << i << "/";

        plug_id_t sourceId;
        plug_id_t destinationId;
        Glib::ustring description;

        result  = deser.read( strstrm.str() + "m_source", sourceId );
        result &= deser.read( strstrm.str() + "m_destination", destinationId );
        result &= deser.read( strstrm.str() + "m_description", description );

        if ( result ) {
            SyncInfo syncInfo;
            syncInfo.m_source = avDevice.getPlugManager().getPlug( sourceId );
            syncInfo.m_destination = avDevice.getPlugManager().getPlug( destinationId );
            syncInfo.m_description = description;

            vec.push_back( syncInfo );
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return true;
}

static bool
deserializeAvPlugUpdateConnections( Glib::ustring path,
                                    Util::IODeserialize& deser,
                                    AvPlugVector& vec )
{
    bool result = true;
    for ( AvPlugVector::iterator it = vec.begin();
          it != vec.end();
          ++it )
    {
        AvPlug* pPlug = *it;
        result &= pPlug->deserializeUpdate( path, deser );
    }
    return result;
}

bool
AvDevice::serialize( Glib::ustring basePath,
                     Util::IOSerialize& ser ) const
{
    bool result;
    result  = m_pConfigRom->serialize( basePath + "m_pConfigRom/", ser );
    result &= ser.write( basePath + "m_verboseLevel", m_verboseLevel );
    result &= m_pPlugManager->serialize( basePath + "AvPlug", ser ); // serialize all av plugs
    result &= serializeVector( basePath + "PlugConnection", ser, m_plugConnections );
    result &= serializeVector( basePath + "Subunit", ser, m_subunits );
    result &= serializeSyncInfoVector( basePath + "SyncInfo", ser, m_syncInfos );

    int i = 0;
    for ( SyncInfoVector::const_iterator it = m_syncInfos.begin();
          it != m_syncInfos.end();
          ++it )
    {
        const SyncInfo& info = *it;
        if ( m_activeSyncInfo == &info ) {
            result &= ser.write( basePath + "m_activeSyncInfo",  i );
            break;
        }
        i++;
    }

    result &= ser.write( basePath + "m_id", m_id );

    return result;
}

AvDevice*
AvDevice::deserialize( Glib::ustring basePath,
                       Util::IODeserialize& deser,
                       Ieee1394Service& ieee1394Service )
{
    AvDevice* pDev = new AvDevice;

    if ( pDev ) {
        pDev->m_pConfigRom = std::auto_ptr<ConfigRom> (
            ConfigRom::deserialize( basePath + "m_pConfigRom/", deser, ieee1394Service )
            );
        if ( !pDev->m_pConfigRom.get() ) {
            delete pDev;
            return 0;
        }

        pDev->m_p1394Service = &ieee1394Service;
        bool result;
        result  = deser.read( basePath + "m_verboseLevel", pDev->m_verboseLevel );
        pDev->m_pPlugManager = AvPlugManager::deserialize( basePath + "AvPlug", deser, *pDev );
        if ( !pDev->m_pPlugManager ) {
            delete pDev;
            return 0;
        }
        result &= deserializeAvPlugUpdateConnections( basePath + "AvPlug", deser, pDev->m_pcrPlugs );
        result &= deserializeAvPlugUpdateConnections( basePath + "AvPlug", deser, pDev->m_externalPlugs );
        result &= deserializeVector<AvPlugConnection>( basePath + "PlugConnnection", deser, *pDev, pDev->m_plugConnections );
        result &= deserializeVector<AvDeviceSubunit>( basePath + "Subunit",  deser, *pDev, pDev->m_subunits );
        result &= deserializeSyncInfoVector( basePath + "SyncInfo", deser, *pDev, pDev->m_syncInfos );

        unsigned int i;
        result &= deser.read( basePath + "m_activeSyncInfo", i );

        if ( result ) {
            if ( i < pDev->m_syncInfos.size() ) {
                pDev->m_activeSyncInfo = &pDev->m_syncInfos[i];
            }
        }

        result &= deser.read( basePath + "m_id", pDev->m_id );
    }

    return pDev;
}

} // end of namespace
