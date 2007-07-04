/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "bebob/bebob_avdevice.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "bebob/GenericMixer.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_plug_info.h"
#include "libavc/avc_extended_plug_info.h"
#include "libavc/avc_subunit_info.h"
#include "libavc/avc_extended_stream_format.h"
#include "libavc/avc_serialize.h"
#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>

namespace BeBoB {

static VendorModelEntry supportedDeviceList[] =
{
    {0x00000f, 0x00010065, "Mackie", "Onyx Firewire"},

    {0x0003db, 0x00010048, "Apogee Electronics", "Rosetta 200"},

    {0x0007f5, 0x00010048, "BridgeCo", "RD Audio1"},

    {0x000a92, 0x00010000, "PreSonus", "FIREBOX"},
    {0x000a92, 0x00010066, "PreSonus", "FirePOD"},

    {0x000aac, 0x00000003, "TerraTec Electronic GmbH", "Phase 88 FW"},
    {0x000aac, 0x00000004, "TerraTec Electronic GmbH", "Phase X24 FW (model version 4)"},
    {0x000aac, 0x00000007, "TerraTec Electronic GmbH", "Phase X24 FW (model version 7)"},

    {0x000f1b, 0x00010064, "ESI", "Quatafire 610"},

    {0x00130e, 0x00000003, "Focusrite", "Saffire Pro26IO"},
    {0x00130e, 0x00000006, "Focusrite", "Saffire Pro10IO"},

    {0x0040ab, 0x00010048, "EDIROL", "FA-101"},
    {0x0040ab, 0x00010049, "EDIROL", "FA-66"},

    {0x000d6c, 0x00010062, "M-Audio", "FW Solo"},
    {0x000d6c, 0x00010081, "M-Audio", "NRV10"},

};

AvDevice::AvDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId )
    : IAvDevice( configRom, ieee1394service, nodeId )
    , m_pPlugManager( new AvPlugManager( DEBUG_LEVEL_NORMAL ) )
    , m_activeSyncInfo( 0 )
    , m_model ( NULL )
    , m_Mixer ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::AvDevice (NodeID %d)\n",
                 nodeId );
    addOption(Util::OptionContainer::Option("snoopMode",false));
}

AvDevice::~AvDevice()
{
    if(m_Mixer != NULL) {
        if (!removeChildOscNode(m_Mixer)) {
            debugWarning("failed to unregister mixer from OSC namespace\n");
        }
        delete m_Mixer;
    }

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

void
AvDevice::setVerboseLevel(int l)
{
//     m_pPlugManager->setVerboseLevel(l);

    IAvDevice::setVerboseLevel(l);
}

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
    unsigned int vendorId = m_pConfigRom->getNodeVendorId();
    unsigned int modelId = m_pConfigRom->getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId )
           )
        {
            m_model = &(supportedDeviceList[i]);
            break;
        }
    }

    if (m_model != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                m_model->vendor_name, m_model->model_name);
    } else return false;

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

    // create a GenericMixer and add it as an OSC child node
    //  remove if already there
    if(m_Mixer != NULL) {
        if (!removeChildOscNode(m_Mixer)) {
            debugWarning("failed to unregister mixer from OSC namespace\n");
        }
        delete m_Mixer;
    }

    //  create the mixer & register it
    if(getAudioSubunit(0) == NULL) {
        debugWarning("Could not find audio subunit, mixer not available.\n");
        m_Mixer = NULL;
    } else {
        m_Mixer = new GenericMixer(*m_p1394Service , *this);
        if (!addChildOscNode(m_Mixer)) {
            debugWarning("failed to register mixer in OSC namespace\n");
        }
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
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if(snoopMode) {
        int current_sr=getSamplingFrequency();
        if (current_sr != convertESamplingFrequency( samplingFrequency ) ) {
            debugError("In snoop mode it is impossible to set the sample rate.\n");
            debugError("Please start the client with the correct setting.\n");
            return false;
        }
        return true;
    } else {
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
    // not executable
    return false;
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

int
AvDevice::getConfigurationIdSampleRate()
{
    ExtendedStreamFormatCmd extStreamFormatCmd( *m_p1394Service );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     m_nodeId );
    extStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
                                                    PlugAddress::ePAM_Unit,
                                                    unitPlugAddress ) );

    extStreamFormatCmd.setNodeId( m_nodeId );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
    extStreamFormatCmd.setVerbose( true );

    if ( !extStreamFormatCmd.fire() ) {
        debugError( "Stream format command failed\n" );
        return false;
    }

    FormatInformation* formatInfo =
        extStreamFormatCmd.getFormatInformation();
    FormatInformationStreamsCompound* compoundStream
        = dynamic_cast< FormatInformationStreamsCompound* > (
            formatInfo->m_streams );
    if ( compoundStream ) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Sample rate 0x%02x\n",
                    compoundStream->m_samplingFrequency );
        return compoundStream->m_samplingFrequency;
    }

    debugError( "Could not retrieve sample rate\n" );
    return 0;
}

int
AvDevice::getConfigurationIdNumberOfChannel( PlugAddress::EPlugDirection ePlugDirection )
{
    ExtendedPlugInfoCmd extPlugInfoCmd( *m_p1394Service );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     m_nodeId );
    extPlugInfoCmd.setPlugAddress( PlugAddress( ePlugDirection,
                                                PlugAddress::ePAM_Unit,
                                                unitPlugAddress ) );
    extPlugInfoCmd.setNodeId( m_nodeId );
    extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extPlugInfoCmd.setVerbose( true );
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_NoOfChannels );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "Number of channels command failed\n" );
        return false;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugNrOfChns )
    {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Number of channels 0x%02x\n",
                    infoType->m_plugNrOfChns->m_nrOfChannels );
        return infoType->m_plugNrOfChns->m_nrOfChannels;
    }

    debugError( "Could not retrieve number of channels\n" );
    return 0;
}

int
AvDevice::getConfigurationIdSyncMode()
{
    SignalSourceCmd signalSourceCmd( *m_p1394Service );
    SignalUnitAddress signalUnitAddr;
    signalUnitAddr.m_plugId = 0x01;
    signalSourceCmd.setSignalDestination( signalUnitAddr );
    signalSourceCmd.setNodeId( m_nodeId );
    signalSourceCmd.setSubunitType( AVCCommand::eST_Unit  );
    signalSourceCmd.setSubunitId( 0xff );

    signalSourceCmd.setCommandType( AVCCommand::eCT_Status );

    if ( !signalSourceCmd.fire() ) {
        debugError( "Number of channels command failed\n" );
        return false;
    }

    SignalAddress* pSyncPlugSignalAddress = signalSourceCmd.getSignalSource();
    SignalSubunitAddress* pSyncPlugSubunitAddress
        = dynamic_cast<SignalSubunitAddress*>( pSyncPlugSignalAddress );
    if ( pSyncPlugSubunitAddress ) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Sync mode 0x%02x\n",
                    ( pSyncPlugSubunitAddress->m_subunitType << 3
                      | pSyncPlugSubunitAddress->m_subunitId ) << 8
                    | pSyncPlugSubunitAddress->m_plugId );

        return ( pSyncPlugSubunitAddress->m_subunitType << 3
                 | pSyncPlugSubunitAddress->m_subunitId ) << 8
            | pSyncPlugSubunitAddress->m_plugId;
    }

    debugError( "Could not retrieve sync mode\n" );
    return 0;
}

int
AvDevice::getConfigurationId()
{
    // create a unique configuration id.
    int id = 0;
    id = getConfigurationIdSampleRate();
    id |= ( getConfigurationIdNumberOfChannel( PlugAddress::ePD_Input )
            + getConfigurationIdNumberOfChannel( PlugAddress::ePD_Output ) ) << 8;
    id |= getConfigurationIdSyncMode() << 16;

    return id;
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
AvDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
        "%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
        m_nodeId);

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

bool
AvDevice::lock() {
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (snoopMode) {
        // don't lock
    } else {

    }

    return true;
}

bool
AvDevice::unlock() {
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (snoopMode) {
        // don't unlock
    } else {

    }
    return true;
}

bool
AvDevice::prepare() {
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

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

    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing receive processor...\n");
    // create & add streamprocessors
    Streaming::StreamProcessor *p;

    p=new Streaming::AmdtpReceiveStreamProcessor(
                             m_p1394Service->getPort(),
                             samplerate,
                             outputPlug->getNrOfChannels());

    if(!p->init()) {
        debugFatal("Could not initialize receive processor!\n");
        delete p;
        return false;
    }

    if (!addPlugToProcessor(*outputPlug,p,
        Streaming::Port::E_Capture)) {
        debugFatal("Could not add plug to processor!\n");
        delete p;
        return false;
    }

    m_receiveProcessors.push_back(p);

    // do the transmit processor
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing transmit processor%s...\n",
            (snoopMode?" in snoop mode":""));
    if (snoopMode) {
        // we are snooping, so this is receive too.
        p=new Streaming::AmdtpReceiveStreamProcessor(
                                  m_p1394Service->getPort(),
                                  samplerate,
                                  inputPlug->getNrOfChannels());
    } else {
        p=new Streaming::AmdtpTransmitStreamProcessor(
                                m_p1394Service->getPort(),
                                samplerate,
                                inputPlug->getNrOfChannels());
    }

    if(!p->init()) {
        debugFatal("Could not initialize transmit processor %s!\n",
            (snoopMode?" in snoop mode":""));
        delete p;
        return false;
    }

    if (snoopMode) {
        if (!addPlugToProcessor(*inputPlug,p,
            Streaming::Port::E_Capture)) {
            debugFatal("Could not add plug to processor!\n");
            return false;
        }
    } else {
        if (!addPlugToProcessor(*inputPlug,p,
            Streaming::Port::E_Playback)) {
            debugFatal("Could not add plug to processor!\n");
            return false;
        }
    }

    // we put this SP into the transmit SP vector,
    // no matter if we are in snoop mode or not
    // this allows us to find out what direction
    // a certain stream should have.
    m_transmitProcessors.push_back(p);

    return true;
}

bool
AvDevice::addPlugToProcessor(
    AvPlug& plug,
    Streaming::StreamProcessor *processor,
    Streaming::AmdtpAudioPort::E_Direction direction) {

    std::string id=std::string("dev?");
    if(!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defauling to 'dev?'\n");
    }

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

            portname << id << "_" << channelInfo->m_name;

            Streaming::Port *p=NULL;
            switch(clusterInfo->m_portType) {
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Speaker:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Headphone:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Microphone:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Line:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Analog:
                p=new Streaming::AmdtpAudioPort(
                        portname.str(),
                        direction,
                        // \todo: streaming backend expects indexing starting from 0
                        // but bebob reports it starting from 1. Decide where
                        // and how to handle this (pp: here)
                        channelInfo->m_streamPosition - 1,
                        channelInfo->m_location - 1,
                        Streaming::AmdtpPortInfo::E_MBLA
                );
                break;

            case ExtendedPlugInfoClusterInfoSpecificData::ePT_MIDI:
                p=new Streaming::AmdtpMidiPort(
                        portname.str(),
                        direction,
                        // \todo: streaming backend expects indexing starting from 0
                        // but bebob reports it starting from 1. Decide where
                        // and how to handle this (pp: here)
                        channelInfo->m_streamPosition - 1,
                        channelInfo->m_location - 1,
                        Streaming::AmdtpPortInfo::E_Midi
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

int
AvDevice::getStreamCount() {
    return m_receiveProcessors.size() + m_transmitProcessors.size();
}

Streaming::StreamProcessor *
AvDevice::getStreamProcessorByIndex(int i) {

    if (i<(int)m_receiveProcessors.size()) {
        return m_receiveProcessors.at(i);
    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        return m_transmitProcessors.at(i-m_receiveProcessors.size());
    }

    return NULL;
}

bool
AvDevice::startStreamByIndex(int i) {
    int iso_channel=-1;
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (i<(int)m_receiveProcessors.size()) {
        int n=i;
        Streaming::StreamProcessor *p=m_receiveProcessors.at(n);

        if(snoopMode) { // a stream from the device to another host
            // FIXME: put this into a decent framework!
            // we should check the oPCR[n] on the device
            struct iec61883_oPCR opcr;
            if (iec61883_get_oPCRX(
                    m_p1394Service->getHandle(),
                    m_pConfigRom->getNodeId() | 0xffc0,
                    (quadlet_t *)&opcr,
                    n)) {

                debugWarning("Error getting the channel for SP %d\n",i);
                return false;
            }

            iso_channel=opcr.channel;
        } else {
            iso_channel=m_p1394Service->allocateIsoChannelCMP(
                m_pConfigRom->getNodeId() | 0xffc0, n,
                m_p1394Service->getLocalNodeId()| 0xffc0, -1);
        }
        if (iso_channel<0) {
            debugError("Could not allocate ISO channel for SP %d\n",i);
            return false;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "Started SP %d on channel %d\n",i,iso_channel);

        p->setChannel(iso_channel);
        return true;

    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        int n=i-m_receiveProcessors.size();
        Streaming::StreamProcessor *p=m_transmitProcessors.at(n);

        if(snoopMode) { // a stream from another host to the device
            // FIXME: put this into a decent framework!
            // we should check the iPCR[n] on the device
            struct iec61883_iPCR ipcr;
            if (iec61883_get_iPCRX(
                    m_p1394Service->getHandle(),
                    m_pConfigRom->getNodeId() | 0xffc0,
                    (quadlet_t *)&ipcr,
                    n)) {

                debugWarning("Error getting the channel for SP %d\n",i);
                return false;
            }

            iso_channel=ipcr.channel;

        } else {
            iso_channel=m_p1394Service->allocateIsoChannelCMP(
                m_p1394Service->getLocalNodeId()| 0xffc0, -1,
                m_pConfigRom->getNodeId() | 0xffc0, n);
        }

        if (iso_channel<0) {
            debugError("Could not allocate ISO channel for SP %d\n",i);
            return false;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "Started SP %d on channel %d\n",i,iso_channel);

        p->setChannel(iso_channel);
        return true;
    }

    debugError("SP index %d out of range!\n",i);
    return false;
}

bool
AvDevice::stopStreamByIndex(int i) {
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (i<(int)m_receiveProcessors.size()) {
        int n=i;
        Streaming::StreamProcessor *p=m_receiveProcessors.at(n);

        if(snoopMode) {

        } else {
            // deallocate ISO channel
            if(!m_p1394Service->freeIsoChannel(p->getChannel())) {
                debugError("Could not deallocate iso channel for SP %d\n",i);
                return false;
            }
        }
        p->setChannel(-1);

        return true;

    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        int n=i-m_receiveProcessors.size();
        Streaming::StreamProcessor *p=m_transmitProcessors.at(n);

        if(snoopMode) {

        } else {
            // deallocate ISO channel
            if(!m_p1394Service->freeIsoChannel(p->getChannel())) {
                debugError("Could not deallocate iso channel for SP %d\n",i);
                return false;
            }
        }
        p->setChannel(-1);

        return true;
    }

    debugError("SP index %d out of range!\n",i);
    return false;
}

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

    result &= serializeOptions( basePath + "Options", ser );

//     result &= ser.write( basePath + "m_id", id );

    return result;
}

AvDevice*
AvDevice::deserialize( Glib::ustring basePath,
                       Util::IODeserialize& deser,
                       Ieee1394Service& ieee1394Service )
{

    ConfigRom *configRom =
        ConfigRom::deserialize( basePath + "m_pConfigRom/", deser, ieee1394Service );

    if ( !configRom ) {
        return NULL;
    }

    AvDevice* pDev = new AvDevice(
        std::auto_ptr<ConfigRom>(configRom),
        ieee1394Service, configRom->getNodeId());

    if ( pDev ) {
        bool result;
        result  = deser.read( basePath + "m_verboseLevel", pDev->m_verboseLevel );

        if (pDev->m_pPlugManager) delete pDev->m_pPlugManager;
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

        result &= deserializeOptions( basePath + "Options", deser, *pDev );
    }

    return pDev;
}

} // end of namespace
