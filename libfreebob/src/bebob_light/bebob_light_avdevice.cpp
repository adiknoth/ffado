/* bebob_light_avdevice.cpp
 * Copyright (C) 2006 by Daniel Wagner
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

#include "bebob_light/bebob_light_avdevice.h"
#include "bebob_light/bebob_light_avdevicesubunit.h"

#include "configrom.h"

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

namespace BeBoB_Light {

using namespace std;

IMPL_DEBUG_MODULE( AvDevice, AvDevice, DEBUG_LEVEL_VERBOSE );

AvDevice::AvDevice( Ieee1394Service& ieee1394service,
                    int nodeId,
                    int verboseLevel )
    : m_1394Service( &ieee1394service )
    , m_nodeId( nodeId )
    , m_verboseLevel( verboseLevel )
    , m_id( 0 )
{
    if ( m_verboseLevel ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB_Light::AvDevice "
                 "(NodeID %d)\n", nodeId );

    m_configRom = new ConfigRom( m_1394Service, m_nodeId );
    m_configRom->initialize();
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
    for ( AvPlugVector::iterator it = m_isoInputPlugs.begin();
          it != m_isoInputPlugs.end();
          ++it )
    {
        delete *it;
    }
    for ( AvPlugVector::iterator it = m_isoOutputPlugs.begin();
          it != m_isoOutputPlugs.end();
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

    if ( !discoverStep1() ) {
        debugError( "Discover step 1 failed\n" );
        return false;
    }
    if ( !discoverStep2() ) {
        debugError( "Discover step 2 failed\n" );
        return false;
    }
    if ( !discoverStep3() ) {
        debugError( "Discover step 3 failed\n" );
        return false;
    }
    if ( !discoverStep4() ) {
        debugError( "Discover step 4 failed\n" );
        return false;
    }
    if ( !discoverStep5() ) {
        debugError( "Discover step 5 failed\n" );
        return false;
    }
    if ( !discoverStep6() ) {
        debugError( "Discover step 6 failed\n" );
        return false;
    }
    if ( !discoverStep7() ) {
        debugError( "Discover step 7 failed\n" );
        return false;
    }
    if ( !discoverStep8() ) {
        debugError( "Discover step 8 failed\n" );
        return false;
    }
    if ( !discoverStep9() ) {
        debugError( "Discover step 9 failed\n" );
        return false;
    }
    if ( !discoverStep10() ) {
        debugError( "Discover step 10 failed\n" );
        return false;
    }

    return true;
}

bool
AvDevice::discoverStep1()
{
    //////////////////////////////////////////////
    // Step 1: Get number of available isochronous input
    // and output plugs of unit

    PlugInfoCmd plugInfoCmd( m_1394Service );
    plugInfoCmd.setNodeId( m_nodeId );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );

    if ( !plugInfoCmd.fire() ) {
        debugError( "discover: plug info command failed (step 1)\n" );
        return false;
    }

    for ( int plugId = 0; plugId < plugInfoCmd.m_serialBusIsochronousInputPlugs; ++plugId )
    {
        AvPlug* plug        = new AvPlug;
        plug->m_plugId      = plugId;
        plug->m_subunitType = PlugAddress::ePAM_Unit;
        plug->m_direction   = PlugAddress::ePD_Input;
        plug->m_name        = "IsoStreamInput";

        m_isoInputPlugs.push_back( plug );
    }

    for ( int plugId = 0; plugId < plugInfoCmd.m_serialBusIsochronousOutputPlugs; ++plugId )
    {
        AvPlug* plug        = new AvPlug;
        plug->m_plugId      = plugId;
        plug->m_subunitType = PlugAddress::ePAM_Unit;
        plug->m_direction   = PlugAddress::ePD_Output;
        plug->m_name        = "IsoStreamOutput";

        m_isoOutputPlugs.push_back( plug );
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "number of iso input plugs = %d, "
                 "number of iso output plugs = %d\n",
                 plugInfoCmd.m_serialBusIsochronousInputPlugs,
                 plugInfoCmd.m_serialBusIsochronousOutputPlugs );

    return true;
}

bool
AvDevice::discoverStep2()
{
    //////////////////////////////////////////////
    // Step 2: For each ISO input plug: get internal
    // output connections (data sink) of the ISO input plug

    for ( AvPlugVector::iterator it = m_isoInputPlugs.begin();
          it != m_isoInputPlugs.end();
          ++it )
    {
        AvPlug* isoInputPlug = *it;

        ExtendedPlugInfoCmd extPlugInfoCmd( m_1394Service );
        UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                         isoInputPlug->getPlugId() );
        extPlugInfoCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
                                                    PlugAddress::ePAM_Unit,
                                                    unitPlugAddress ) );
        extPlugInfoCmd.setNodeId( m_nodeId );
        extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
        //extPlugInfoCmd.setVerbose( true );
        ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
            ExtendedPlugInfoInfoType::eIT_PlugOutput );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep2: plug outputs command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugOutput )
        {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "number of output plugs is %d for iso input "
                         "plug %d\n",
                         infoType->m_plugOutput->m_nrOfOutputPlugs,
                         isoInputPlug->getPlugId() );

            if ( infoType->m_plugOutput->m_nrOfOutputPlugs
                 != infoType->m_plugOutput->m_outputPlugAddresses.size() )
            {
                debugError( "number of output plugs (%d) disagree with "
                            "number of elements in plug address vector (%d)\n",
                            infoType->m_plugOutput->m_nrOfOutputPlugs,
                            infoType->m_plugOutput->m_outputPlugAddresses.size());
            }
            for ( unsigned int i = 0;
                  i < infoType->m_plugOutput->m_outputPlugAddresses.size();
                  ++i )
            {
                PlugAddressSpecificData* plugAddress
                    = infoType->m_plugOutput->m_outputPlugAddresses[i];


                UnitPlugSpecificDataPlugAddress* pUnitPlugAddress =
                    dynamic_cast<UnitPlugSpecificDataPlugAddress*>
                    ( plugAddress->m_plugAddressData );

                if ( pUnitPlugAddress ) {
                    debugOutput( DEBUG_LEVEL_VERBOSE,
                                 "unit plug address is not handled\n" );
                }

                SubunitPlugSpecificDataPlugAddress* pSubunitPlugAddress =
                    dynamic_cast<SubunitPlugSpecificDataPlugAddress*>
                    ( plugAddress->m_plugAddressData );

                if ( pSubunitPlugAddress ) {
                    debugOutput( DEBUG_LEVEL_VERBOSE,
                                 "output plug %d is owned by subunit_type %d, "
                                 "subunit_id = %d\n",
                                 pSubunitPlugAddress->m_plugId,
                                 pSubunitPlugAddress->m_subunitType,
                                 pSubunitPlugAddress->m_subunitId );

                    if ( !discoverPlugConnection( *isoInputPlug,
                                                  *pSubunitPlugAddress ) )
                    {
                        debugError( "Discovering of plug connection failed\n" );
                        return false;
                    }
                }

                FunctionBlockPlugSpecificDataPlugAddress*
                    pFunctionBlockPlugAddress =
                    dynamic_cast<FunctionBlockPlugSpecificDataPlugAddress*>
                    ( plugAddress->m_plugAddressData );

                if (  pFunctionBlockPlugAddress  ) {
                    debugOutput( DEBUG_LEVEL_VERBOSE,
                                 "function block address is not handled\n" );
                }
            }
        } else {
            debugError( "discoverStep2: no valid info type, output plug\n" );
            return false;
        }
    }

    return true;
}

bool
AvDevice::discoverStep3()
{
    //////////////////////////////////////////////
    // Step 3: For each ISO output plug: get internal
    // intput connections (data source) of the ISO output plug

    for ( AvPlugVector::iterator it = m_isoOutputPlugs.begin();
          it != m_isoOutputPlugs.end();
          ++it )
    {
        AvPlug* isoOutputPlug = *it;

        ExtendedPlugInfoCmd extPlugInfoCmd( m_1394Service );
        UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                         isoOutputPlug->getPlugId() );
        extPlugInfoCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Output,
                                                    PlugAddress::ePAM_Unit,
                                                    unitPlugAddress ) );
        extPlugInfoCmd.setNodeId( m_nodeId );
        extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
        //extPlugInfoCmd.setVerbose( true );
        ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
            ExtendedPlugInfoInfoType::eIT_PlugInput );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep3: plug inputs command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugInput )
        {
            PlugAddressSpecificData* plugAddress
                = infoType->m_plugInput->m_plugAddress;


            UnitPlugSpecificDataPlugAddress* pUnitPlugAddress =
                dynamic_cast<UnitPlugSpecificDataPlugAddress*>
                ( plugAddress->m_plugAddressData );

            if ( pUnitPlugAddress ) {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "unit plug address is not handled\n" );
            }

            SubunitPlugSpecificDataPlugAddress* pSubunitPlugAddress =
                dynamic_cast<SubunitPlugSpecificDataPlugAddress*>
                ( plugAddress->m_plugAddressData );

            if ( pSubunitPlugAddress ) {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "output plug %d is owned by subunit_type %d, "
                             "subunit_id %d\n",
                             pSubunitPlugAddress->m_plugId,
                             pSubunitPlugAddress->m_subunitType,
                             pSubunitPlugAddress->m_subunitId );

                if ( !discoverPlugConnection( *isoOutputPlug, *pSubunitPlugAddress ) ) {
                    debugError( "Discovering of plug connection failed\n" );
                    return false;
                }
            }

            FunctionBlockPlugSpecificDataPlugAddress*
                pFunctionBlockPlugAddress =
                dynamic_cast<FunctionBlockPlugSpecificDataPlugAddress*>
                ( plugAddress->m_plugAddressData );

            if (  pFunctionBlockPlugAddress  ) {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "function block address is not handled\n" );
            }
        } else {
            debugError( "discoverStep3: no valid info type, input plug\n" );
            return false;
        }
    }

    return true;
}

bool
AvDevice::discoverStep4Plug( AvPlugVector& isoPlugs )
{
    //////////////////////////////////////////////
    // Step 4: For all ISO plugs with valid internal connections:
    // get type of data stream flowing through plug.
    // Stream type of interest: iso stream & sync stream

    for ( AvPlugVector::iterator it = isoPlugs.begin();
          it != isoPlugs.end();
          ++it )
    {
        AvPlug* isoPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "plug %d has no valid connecton -> skip\n",
                         isoPlug->getPlugId() );
            continue;
        }

        ExtendedPlugInfoCmd extPlugInfoCmd( m_1394Service );
        UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                         isoPlug->getPlugId() );
        PlugAddress::EPlugDirection direction =
            static_cast<PlugAddress::EPlugDirection>( isoPlug->getPlugDirection() );
        extPlugInfoCmd.setPlugAddress( PlugAddress( direction,
                                                    PlugAddress::ePAM_Unit,
                                                    unitPlugAddress ) );
        extPlugInfoCmd.setNodeId( m_nodeId );
        extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
        //extPlugInfoCmd.setVerbose( true );
        ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
            ExtendedPlugInfoInfoType::eIT_PlugType );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep4Plug: plug type command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugType )
        {
            plug_type_t plugType = infoType->m_plugType->m_plugType;

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "plug %d is of type %d (%s)\n",
                         isoPlug->getPlugId(),
                         plugType,
                         extendedPlugInfoPlugTypeToString( plugType ) );

            isoPlug->m_plugType = plugType;
        }
    }

   return true;
}

bool
AvDevice::discoverStep4()
{
    bool success;
    success  = discoverStep4Plug( m_isoInputPlugs );
    success &= discoverStep4Plug( m_isoOutputPlugs );

    return success;
}

bool
AvDevice::discoverStep5Plug( AvPlugVector& isoPlugs )
{
    //////////////////////////////////////////////
    // Step 5: For all ISO plugs with valid internal connections:
    // get data block size of the stream flowing through the plug

    for ( AvPlugVector::iterator it = isoPlugs.begin();
          it != isoPlugs.end();
          ++it )
    {
        AvPlug* isoPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "plug %d has no valid connecton -> skip\n",
                         isoPlug->getPlugId() );
            continue;
        }

        ExtendedPlugInfoCmd extPlugInfoCmd( m_1394Service );
        UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                         isoPlug->getPlugId() );
        PlugAddress::EPlugDirection direction =
            static_cast<PlugAddress::EPlugDirection>( isoPlug->getPlugDirection() );
        extPlugInfoCmd.setPlugAddress( PlugAddress( direction,
                                                    PlugAddress::ePAM_Unit,
                                                    unitPlugAddress ) );
        extPlugInfoCmd.setNodeId( m_nodeId );
        extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
        //extPlugInfoCmd.setVerbose( true );
        ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
            ExtendedPlugInfoInfoType::eIT_NoOfChannels );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep5Plug: number of channels command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugNrOfChns )
        {
            nr_of_channels_t nrOfChannels
                = infoType->m_plugNrOfChns->m_nrOfChannels;

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "plug %d has %d channels\n",
                         isoPlug->getPlugId(),
                         nrOfChannels );

            isoPlug->m_nrOfChannels = nrOfChannels;
        }
    }

    return true;
}


bool
AvDevice::discoverStep5()
{
    bool success;
    success  = discoverStep5Plug( m_isoInputPlugs );
    success &= discoverStep5Plug( m_isoOutputPlugs );

    return success;
}

bool
AvDevice::discoverStep6Plug( AvPlugVector& isoPlugs )
{
    //////////////////////////////////////////////
    // Step 6: For all ISO plugs with valid internal connections:
    // get position of channels within data block

    for ( AvPlugVector::iterator it = isoPlugs.begin();
          it != isoPlugs.end();
          ++it )
    {
        AvPlug* isoPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "plug %d has no valid connecton -> skip\n",
                         isoPlug->getPlugId() );
            continue;
        }

        ExtendedPlugInfoCmd extPlugInfoCmd( m_1394Service );
        UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                         isoPlug->getPlugId() );
        PlugAddress::EPlugDirection direction =
            static_cast<PlugAddress::EPlugDirection>( isoPlug->getPlugDirection() );
        extPlugInfoCmd.setPlugAddress( PlugAddress( direction,
                                                    PlugAddress::ePAM_Unit,
                                                    unitPlugAddress ) );
        extPlugInfoCmd.setNodeId( m_nodeId );
        extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
        //extPlugInfoCmd.setVerbose( true );
        ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
            ExtendedPlugInfoInfoType::eIT_ChannelPosition );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep6Plug: channels position command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugChannelPosition )
        {
            if ( !isoPlug->copyClusterInfo(
                     *( infoType->m_plugChannelPosition ) ) )
            {
                debugError( "discoverStep6Plug: Could not copy channel position "
                            "information\n" );
                return false;
            }

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "plug %d: channel position information "
                         "retrieved\n",
                         isoPlug->getPlugId() );

            isoPlug->debugOutputClusterInfos( DEBUG_LEVEL_VERBOSE );
        }
    }

    return true;
}

bool
AvDevice::discoverStep6()
{
    bool success;
    success  = discoverStep6Plug( m_isoInputPlugs );
    success &= discoverStep6Plug( m_isoOutputPlugs );

    return success;
}

bool
AvDevice::discoverStep7Plug( AvPlugVector& isoPlugs )
{
    //////////////////////////////////////////////
    // Step 7: For all ISO plugs with valid internal connections:
    // get hardware name of channel

    for ( AvPlugVector::iterator it = isoPlugs.begin();
          it != isoPlugs.end();
          ++it )
    {
        AvPlug* isoPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "plug %d has no valid connecton -> skip\n",
                         isoPlug->getPlugId() );
            continue;
        }

        for ( AvPlug::ClusterInfoVector::iterator clit =
                  isoPlug->m_clusterInfos.begin();
              clit != isoPlug->m_clusterInfos.end();
              ++clit )
        {
            AvPlug::ClusterInfo* clitInfo = &*clit;

            for ( AvPlug::ChannelInfoVector::iterator pit =
                      clitInfo->m_channelInfos.begin();
                  pit != clitInfo->m_channelInfos.end();
                  ++pit )
            {
                AvPlug::ChannelInfo* channelInfo = &*pit;

                ExtendedPlugInfoCmd extPlugInfoCmd( m_1394Service );
                UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                                 isoPlug->getPlugId() );
                PlugAddress::EPlugDirection direction =
                    static_cast<PlugAddress::EPlugDirection>( isoPlug->getPlugDirection() );
                extPlugInfoCmd.setPlugAddress( PlugAddress( direction,
                                                            PlugAddress::ePAM_Unit,
                                                            unitPlugAddress ) );
                extPlugInfoCmd.setNodeId( m_nodeId );
                extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
                //extPlugInfoCmd.setVerbose( true );
                ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
                    ExtendedPlugInfoInfoType::eIT_ChannelName );
                extendedPlugInfoInfoType.initialize();
                extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

                ExtendedPlugInfoInfoType* infoType =
                    extPlugInfoCmd.getInfoType();
                if ( infoType ) {
                    infoType->m_plugChannelName->m_streamPosition =
                        channelInfo->m_streamPosition;
                }
                if ( !extPlugInfoCmd.fire() ) {
                    debugError( "discoverStep7: channel name command failed\n" );
                    return false;
                }
                infoType = extPlugInfoCmd.getInfoType();
                if ( infoType
                     && infoType->m_plugChannelName )
                {
                    debugOutput( DEBUG_LEVEL_VERBOSE,
                                 "plug %d stream "
                                 "position %d: channel name = %s\n",
                                 isoPlug->getPlugId(),
                                 channelInfo->m_streamPosition,
                                 infoType->m_plugChannelName->m_plugChannelName.c_str() );
                    channelInfo->m_name =
                        infoType->m_plugChannelName->m_plugChannelName;
                }

            }
        }
    }

    return true;
}
bool
AvDevice::discoverStep7()
{
    bool success;
    success  = discoverStep7Plug( m_isoInputPlugs );
    success &= discoverStep7Plug( m_isoOutputPlugs );

    return success;
}

bool
AvDevice::discoverStep8Plug( AvPlugVector& isoPlugs )
{
    //////////////////////////////////////////////
    // Step 8: For all ISO plugs with valid internal connections:
    // get logical input and output streams provided by the device

    for ( AvPlugVector::iterator it = isoPlugs.begin();
          it != isoPlugs.end();
          ++it )
    {
        AvPlug* isoPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "%s plug %d has no valid connecton -> skip\n",
                         isoPlug->getName(),
                         isoPlug->getPlugId() );
            continue;
        }

        if ( isoPlug->getPlugType()
             == ExtendedPlugInfoPlugTypeSpecificData::eEPIPT_Sync )
        {
            // If the plug is of type sync it is either a normal 2 channel
            // stream (not compound stream) or it is a compound stream
            // with exactly one cluster. This depends on the
            // extended stream format command version which is used.
            // We are not interested in this plug so we skip it.
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "%s plug %d is of type sync -> skip\n",
                         isoPlug->getName(),
                         isoPlug->getPlugId() );
            continue;
        }

        for ( AvPlug::ClusterInfoVector::iterator clit =
                  isoPlug->m_clusterInfos.begin();
              clit != isoPlug->m_clusterInfos.end();
              ++clit )
        {
            AvPlug::ClusterInfo* clusterInfo = &*clit;

            ExtendedPlugInfoCmd extPlugInfoCmd( m_1394Service );
            UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                             isoPlug->getPlugId() );
            PlugAddress::EPlugDirection direction =
                static_cast<PlugAddress::EPlugDirection>( isoPlug->getPlugDirection() );
            extPlugInfoCmd.setPlugAddress( PlugAddress( direction,
                                                        PlugAddress::ePAM_Unit,
                                                        unitPlugAddress ) );
            extPlugInfoCmd.setNodeId( m_nodeId );
            extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
            //extPlugInfoCmd.setVerbose( true );
            ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
                ExtendedPlugInfoInfoType::eIT_ClusterInfo );
            extendedPlugInfoInfoType.initialize();
            extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

            extPlugInfoCmd.getInfoType()->m_plugClusterInfo->m_clusterIndex =
                clusterInfo->m_index;

            if ( !extPlugInfoCmd.fire() ) {
                debugError( "discoverStep8Plug: cluster info command failed\n" );
                return false;
            }

            ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
            if ( infoType
                 && infoType->m_plugClusterInfo )
            {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "%s plug %d: cluster index = %d, "
                             "portType %s, cluster name = %s\n",
                             isoPlug->getName(),
                             isoPlug->getPlugId(),
                             infoType->m_plugClusterInfo->m_clusterIndex,
                             extendedPlugInfoClusterInfoPortTypeToString(
                                 infoType->m_plugClusterInfo->m_portType ),
                             infoType->m_plugClusterInfo->m_clusterName.c_str() );

                clusterInfo->m_portType = infoType->m_plugClusterInfo->m_portType;
                clusterInfo->m_name = infoType->m_plugClusterInfo->m_clusterName;
            }
        }
    }

    return true;
}

bool
AvDevice::discoverStep8()
{
    bool success;
    success  = discoverStep8Plug( m_isoInputPlugs );
    success &= discoverStep8Plug( m_isoOutputPlugs );

    return success;
}

bool
AvDevice::discoverStep9Plug( AvPlugVector& isoPlugs )
{
    //////////////////////////////////////////////
    // Step 9: For all ISO plugs with valid internal connections:
    // get current stream format

    for ( AvPlugVector::iterator it = isoPlugs.begin();
          it != isoPlugs.end();
          ++it )
    {
        AvPlug* isoPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "%s plug %d has no valid connecton -> skip\n",
                         isoPlug->getName(),
                         isoPlug->getPlugId() );
            continue;
        }

        ExtendedStreamFormatCmd extStreamFormatCmd( m_1394Service );
        UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                         isoPlug->getPlugId() );
        PlugAddress::EPlugDirection direction =
            static_cast<PlugAddress::EPlugDirection>( isoPlug->getPlugDirection() );
        extStreamFormatCmd.setPlugAddress( PlugAddress( direction,
                                                        PlugAddress::ePAM_Unit,
                                                        unitPlugAddress ) );

        extStreamFormatCmd.setNodeId( m_nodeId );
        extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
        //extStreamFormatCmd.setVerbose( true );

        if ( !extStreamFormatCmd.fire() ) {
            debugError( "discoverStep9Plug: stream format command failed\n" );
            return false;
        }

        FormatInformation* formatInfo =
            extStreamFormatCmd.getFormatInformation();
        FormatInformationStreamsCompound* compoundStream
            = dynamic_cast< FormatInformationStreamsCompound* > (
                formatInfo->m_streams );
        if ( compoundStream ) {
            isoPlug->m_samplingFrequency =
                compoundStream->m_samplingFrequency;
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "discoverStep9Plug: %s plug %d uses "
                         "sampling frequency %d\n",
                         isoPlug->getName(),
                         isoPlug->getPlugId(),
                         isoPlug->m_samplingFrequency );

            for ( int i = 1;
                  i <= compoundStream->m_numberOfStreamFormatInfos;
                  ++i )
            {
                AvPlug::ClusterInfo* clusterInfo =
                    isoPlug->getClusterInfoByIndex( i );

                if ( !clusterInfo ) {
                    debugError( "discoverStep9Plug: No matching cluster info found "
                                "for index %d\n",  i );
                    return false;
                }
                StreamFormatInfo* streamFormatInfo =
                    compoundStream->m_streamFormatInfos[ i - 1 ];

                int nrOfChannels = clusterInfo->m_nrOfChannels;
                if ( streamFormatInfo->m_streamFormat ==
                     FormatInformation::eFHL2_AM824_MIDI_CONFORMANT )
                {
                    // 8 logical midi channels fit into 1 channel
                    nrOfChannels = ( ( nrOfChannels + 7 ) / 8 );
                }
                // sanity checks
                if (  nrOfChannels !=
                     streamFormatInfo->m_numberOfChannels )
                {
                    debugError( "discoverStep9Plug: Number of channels "
                                "mismatch: %s plug %d discovering reported "
                                "%d channels for cluster '%s', while stream format "
                                "reported %d\n",
                                isoPlug->getName(),
                                isoPlug->getPlugId(),
                                nrOfChannels,
                                clusterInfo->m_name.c_str(),
                                streamFormatInfo->m_numberOfChannels);
                    return false;
                }
                clusterInfo->m_streamFormat = streamFormatInfo->m_streamFormat;

                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "%s plug %d cluster info %d ('%s'): stream format %d\n",
                             isoPlug->getName(),
                             isoPlug->getPlugId(),
                             i,
                             clusterInfo->m_name.c_str(),
                             clusterInfo->m_streamFormat );
            }
        }

        FormatInformationStreamsSync* syncStream
            = dynamic_cast< FormatInformationStreamsSync* > (
                formatInfo->m_streams );
        if ( syncStream ) {
            isoPlug->m_samplingFrequency =
                syncStream->m_samplingFrequency;
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "%s plug %d is sync stream with sampling frequency %d\n",
                             isoPlug->getName(),
                             isoPlug->getPlugId(),
                             isoPlug->m_samplingFrequency );
        }

        if ( !compoundStream && !syncStream ) {
            debugError( "discoverStep9Plug: Unsupported stream format\n" );
            return false;
        }
    }

    return true;
}

bool
AvDevice::discoverStep9()
{
    bool success;
    success  = discoverStep9Plug( m_isoInputPlugs );
    success &= discoverStep9Plug( m_isoOutputPlugs );

    return success;
}

bool
AvDevice::discoverStep10Plug( AvPlugVector& isoPlugs )
{
   for ( AvPlugVector::iterator it = isoPlugs.begin();
          it != isoPlugs.end();
          ++it )
    {
        AvPlug* isoPlug = *it;
        ExtendedStreamFormatCmd extStreamFormatCmd( m_1394Service,
                                                    ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList);
        UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                         isoPlug->getPlugId() );
        PlugAddress::EPlugDirection direction =
            static_cast<PlugAddress::EPlugDirection>( isoPlug->getPlugDirection() );
        extStreamFormatCmd.setPlugAddress( PlugAddress( direction,
                                                        PlugAddress::ePAM_Unit,
                                                        unitPlugAddress ) );

        extStreamFormatCmd.setNodeId( m_nodeId );
        //extStreamFormatCmd.setVerbose( true );

        int i = 0;
        bool cmdSuccess = false;

        do {
            extStreamFormatCmd.setIndexInStreamFormat( i );
            extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
            cmdSuccess = extStreamFormatCmd.fire();
            if ( cmdSuccess
                 && ( extStreamFormatCmd.getResponse() == AVCCommand::eR_Implemented ) )
            {
                AvPlug::FormatInfo formatInfo;
                formatInfo.m_index = i;

                FormatInformationStreamsSync* syncStream
                    = dynamic_cast< FormatInformationStreamsSync* >
                    ( extStreamFormatCmd.getFormatInformation()->m_streams );
                if ( syncStream ) {
                    formatInfo.m_samplingFrequency =
                        syncStream->m_samplingFrequency;
                    formatInfo.m_isSyncStream = true ;
                }

                FormatInformationStreamsCompound* compoundStream
                    = dynamic_cast< FormatInformationStreamsCompound* >
                    ( extStreamFormatCmd.getFormatInformation()->m_streams );
                if ( compoundStream ) {
                    formatInfo.m_samplingFrequency =
                        compoundStream->m_samplingFrequency;
                    formatInfo.m_isSyncStream = false;
                    for ( int j = 0;
                          j < compoundStream->m_numberOfStreamFormatInfos;
                          ++j )
                    {
                        switch ( compoundStream->m_streamFormatInfos[j]->m_streamFormat ) {
                        case AVC1394_STREAM_FORMAT_AM824_MULTI_BIT_LINEAR_AUDIO_RAW:
                            formatInfo.m_audioChannels +=
                                compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
                            break;
                        case AVC1394_STREAM_FORMAT_AM824_MIDI_CONFORMANT:
                            formatInfo.m_midiChannels +=
                                compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
                            break;
                        default:
                            debugWarning( "discoverStep10Plug: unknown stream "
                                          "format for channel (%d)\n", j );
                        }
                    }
                }

                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "[%s:%d] formatInfo[%d].m_samplingFrequency = %d\n",
                             isoPlug->getName(), isoPlug->getPlugId(),
                             i, formatInfo.m_samplingFrequency );
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "[%s:%d] formatInfo[%d].m_isSyncStream = %d\n",
                             isoPlug->getName(), isoPlug->getPlugId(),
                             i, formatInfo.m_isSyncStream );
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "[%s:%d] formatInfo[%d].m_audioChannels = %d\n",
                             isoPlug->getName(), isoPlug->getPlugId(),
                             i, formatInfo.m_audioChannels );
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "[%s:%d] formatInfo[%d].m_midiChannels = %d\n",
                             isoPlug->getName(), isoPlug->getPlugId(),
                             i, formatInfo.m_midiChannels );

                isoPlug->m_formatInfos.push_back( formatInfo );
            }

            ++i;
        } while ( cmdSuccess && ( extStreamFormatCmd.getResponse()
                                  == ExtendedStreamFormatCmd::eR_Implemented ) );
    }

   return true;
}


bool
AvDevice::discoverStep10()
{
    //////////////////////////////////////////////
    // Step 10: For all ISO plugs: get all stream
    // supported formats

    bool success;

    success  = discoverStep10Plug( m_isoInputPlugs );
    success &= discoverStep10Plug( m_isoOutputPlugs );

    return success;
}

bool
AvDevice::discoverPlugConnection( AvPlug& srcPlug,
                                  SubunitPlugSpecificDataPlugAddress& subunitPlugAddress )
{
    AvDeviceSubunit* subunit = getSubunit( subunitPlugAddress.m_subunitType,
                                           subunitPlugAddress.m_subunitId );

    if ( subunit ) {
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "%s plug %d has a valid connection to plug %d of "
                     "%s subunit %d \n",
                     srcPlug.getName(),
                     srcPlug.getPlugId(),
                     subunitPlugAddress.m_plugId,
                     subunit->getName(),
                     subunit->getSubunitId() );

        AvPlug* destPlug        = new AvPlug;
        destPlug->m_plugId      = subunitPlugAddress.m_plugId;
        destPlug->m_subunitType = subunitPlugAddress.m_subunitType;
        destPlug->m_subunitId   = subunitPlugAddress.m_subunitId;

        if ( !subunit->addPlug( *destPlug ) ) {
            debugError( "Could not add plug %d to subunit %d\n",
                        destPlug->getPlugId(), subunit->getSubunitId() );
            return false;
        }

        AvPlugConnection* plugConnection = new AvPlugConnection;
        plugConnection->m_srcPlug = &srcPlug;
        plugConnection->m_destPlug = destPlug;
        m_plugConnections.push_back( plugConnection );

        return true;
    } else {
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "found plug address points to unhandled  "
                     "subunit -> ignored\n" );
    }

    return true;
}

bool
AvDevice::enumerateSubUnits()
{
    bool musicSubunitFound=false;
    bool audioSubunitFound=false;

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
            subunit = new AvDeviceSubunitAudio( this, subunitId );
            if ( !subunit ) {
                debugFatal( "Could not allocate AvDeviceSubunitAudio\n" );
                return false;
            }
            m_subunits.push_back( subunit );
            audioSubunitFound=true;
            break;
        case AVCCommand::eST_Music:
            subunit = new AvDeviceSubunitMusic( this, subunitId );
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
        if ( plugConnection->m_srcPlug == &srcPlug ) {
            return plugConnection;
        }
    }

    return 0;
}

AvPlug*
AvDevice::getPlugById( AvPlugVector& plugs, int id )
{
    for ( AvPlugVector::iterator it = plugs.begin();
          it != plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( id == plug->getPlugId() ) {
            return plug;
        }
    }

    return 0;
}

bool
AvDevice::addXmlDescriptionPlug( AvPlug& plug,
                                 xmlNodePtr connectionSet )
{
    char* result;

    int direction;
    switch ( plug.getPlugDirection() ) {
        case 0:
            direction = FREEBOB_PLAYBACK;
            break;
        case 1:
            direction = FREEBOB_CAPTURE;
            break;
    default:
        debugError( "plug direction invalid (%d)\n",
                    plug.getPlugDirection() );
        return false;
    }

    asprintf( &result, "%d",  direction );
    if ( !xmlNewChild( connectionSet,
                       0,
                       BAD_CAST "Direction",
                       BAD_CAST result ) )
    {
        debugError( "Couldn't create 'Direction' node\n" );
        free( result );
        return false;
    }
    free( result );

    xmlNodePtr connection = xmlNewChild( connectionSet, 0,
                                         BAD_CAST "Connection", 0 );
    if ( !connection ) {
        debugError( "Couldn't create 'Connection' node for "
                    "direction %d\n", plug.getPlugDirection() );
        return false;
    }

    asprintf( &result, "%08x%08x",
              ( quadlet_t )( m_configRom->getGuid() >> 32 ),
              ( quadlet_t )( m_configRom->getGuid() & 0xfffffff ) );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "GUID",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'GUID' node\n" );
        free( result );
        return false;
    }
    free( result );


    asprintf( &result, "%d", m_id & 0xff );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Id",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Id' node\n" );
        free( result );
        return false;
    }
    free( result );

    asprintf( &result, "%d", m_1394Service->getPort() );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Port",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Port' node\n" );
        free( result );
        return false;
    }
    free( result );

    asprintf( &result, "%d",  m_nodeId);
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Node",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Node' node\n" );
        free( result );
        return false;
    }
    free( result );


    asprintf( &result, "%d",  plug.getNrOfChannels() );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Dimension",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Dimension' node\n" );
        free( result );
        return false;
    }
    free( result );


    asprintf( &result, "%d",  plug.getSampleRate() );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Samplerate",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Samplerate' node\n" );
        free( result );
        return false;
    }
    free( result );


    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "IsoChannel", BAD_CAST "-1" ) )
    {
        debugError( "Couldn't create 'IsoChannel' node\n" );
        return false;
    }

    xmlNodePtr streams = xmlNewChild( connection,  0,
                                      BAD_CAST "Streams",  0 );
    if ( !streams ) {
        debugError( "Couldn't create 'Streams' node for "
                    "direction %d\n", plug.getPlugDirection() );
        return false;
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

            xmlNodePtr stream = xmlNewChild( streams,  0,
                                             BAD_CAST "Stream",  0 );
            if ( !stream ) {
                debugError( "Coulnd't create 'Stream' node" );
                free(result);
                return false;
            }

            // \todo: iec61883 backend expects indexing starting from 0
            // but bebob reports it starting from 1. Decide where
            // and how to handle this
            asprintf( &result, "%d", channelInfo->m_streamPosition - 1 );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Position",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Position' node" );
                free( result );
                return false;
            }
            free( result );

            asprintf( &result, "%d", channelInfo->m_location );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Location",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Location' node" );
                free( result );
                return false;
            }
            free( result );

            asprintf( &result, "%d", clusterInfo->m_streamFormat );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Format",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Format' node" );
                free( result );
                return false;
            }
            free( result );


            asprintf( &result, "%d", clusterInfo->m_portType );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Type",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Type' node" );
                free( result );
                return false;
            }
            free( result );


            // \todo XXX: What do to do with DestinationPort value??
            asprintf( &result, "%d", 0 );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "DestinationPort",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'DestinationPort' node" );
                free( result );
                return false;
            }
            free( result );

            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Name",
                               BAD_CAST channelInfo->m_name.c_str() ) )
            {
                debugError( "Couldn't create 'Name' node" );
                return false;
            }
        }
    }

    return true;
}

bool
AvDevice::addXmlDescriptionStreamFormats( AvPlug& plug,
                                          xmlNodePtr streamFormatNode )
{
    int direction;
    switch ( plug.getPlugDirection() ) {
        case 0:
            direction = FREEBOB_PLAYBACK;
            break;
        case 1:
            direction = FREEBOB_CAPTURE;
            break;
    default:
        debugError( "addXmlDescriptionStreamFormats: plug direction invalid (%d)\n",
                    plug.getPlugDirection() );
        return false;
    }

    char* result;
    asprintf( &result, "%d",  direction );
    if ( !xmlNewChild( streamFormatNode,
                       0,
                       BAD_CAST "Direction",
                       BAD_CAST result ) )
    {
        debugError( "addXmlDescriptionStreamFormats: Could not  create 'Direction' node\n" );
        return false;
    }

    for ( AvPlug::FormatInfoVector::iterator it =
              plug.m_formatInfos.begin();
          it != plug.m_formatInfos.end();
          ++it )
    {
        AvPlug::FormatInfo formatInfo = *it;
        xmlNodePtr formatNode = xmlNewChild( streamFormatNode, 0,
                                             BAD_CAST "Format", 0 );
        if ( !formatNode ) {
            debugError( "addXmlDescriptionStreamFormats: Could not create 'Format' node\n" );
            return false;
        }

        asprintf( &result, "%d",
                  convertESamplingFrequency( static_cast<ESamplingFrequency>( formatInfo.m_samplingFrequency ) ) );
        if ( !xmlNewChild( formatNode,  0,
                           BAD_CAST "Samplerate",  BAD_CAST result ) )
        {
            debugError( "Couldn't create 'Samplerate' node\n" );
            free(result);
            return false;
        }

        asprintf( &result, "%d",  formatInfo.m_audioChannels );
        if ( !xmlNewChild( formatNode,  0,
                           BAD_CAST "AudioChannels",  BAD_CAST result ) )
        {
            debugError( "Couldn't create 'AudioChannels' node\n" );
            free(result);
            return false;
        }

        asprintf( &result, "%d",  formatInfo.m_midiChannels );
        if ( !xmlNewChild( formatNode,  0,
                           BAD_CAST "MidiChannels",  BAD_CAST result ) )
        {
            debugError( "Couldn't create 'MidiChannels' node\n" );
            free(result);
            return false;
        }
    }

   free(result);
   return true;
}

bool
AvDevice::addXmlDescription( xmlNodePtr deviceNode )
{
    // connection set
    //  direction
    //  connection
    //    id
    //    port
    //    node
    //    plug
    //    dimension
    //    samplerate
    //    streams
    //      stream


    ///////////
    // get plugs

    AvPlug* inputPlug = getPlugById( m_isoInputPlugs, 0 );
    if ( !inputPlug ) {
        debugError( "addXmlDescription: No iso input plug found with id %d\n" );
        return false;
    }
    AvPlug* outputPlug = getPlugById( m_isoOutputPlugs, 0 );
    if ( !outputPlug ) {
        debugError( "addXmlDescription: No iso output plug found with id %d\n" );
        return false;
    }

    ///////////
    // add connection set output

    xmlNodePtr connectionSet = xmlNewChild( deviceNode, 0,
                                            BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "addXmlDescription:: Could not create 'ConnnectionSet' node for "
                    "direction 1 (playback)\n" );
        return false;
    }

    if ( !addXmlDescriptionPlug( *inputPlug, connectionSet ) ) {
        debugError( "addXmlDescription: Could not add iso input plug 0 to XML description\n" );
        return false;
    }

    // add connection set input

    connectionSet = xmlNewChild( deviceNode, 0,
                                 BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "addXmlDescription: Couldn't create 'ConnectionSet' node for "
                    "direction 0 (recorder)\n" );
        return false;
    }

    if ( !addXmlDescriptionPlug( *outputPlug, connectionSet ) ) {
        debugError( "addXmlDescription: Could not add iso output plug 0 to XML description\n" );
        return false;
    }

    ////////////
    // add stream format

    xmlNodePtr streamFormatNode = xmlNewChild( deviceNode, 0,
                                               BAD_CAST "StreamFormats", 0 );
    if ( !streamFormatNode ) {
        debugError( "addXmlDescription: Could not create 'StreamFormats' node\n" );
        return false;
    }

    if ( !addXmlDescriptionStreamFormats( *inputPlug, streamFormatNode ) ) {
        debugError( "addXmlDescription:: Could not add stream format info\n" );
        return false;
    }

    streamFormatNode= xmlNewChild( deviceNode, 0,
                                 BAD_CAST "StreamFormats", 0 );
    if ( !streamFormatNode ) {
        debugError( "addXmlDescription: Could not create 'StreamFormat' node\n" );
        return false;
    }

    if ( !addXmlDescriptionStreamFormats( *outputPlug, streamFormatNode ) ) {
        debugError( "addXmlDescription:: Could not add stream format info\n" );
        return false;
    }

    return true;
}

bool
AvDevice::setSamplingFrequencyPlug( AvPlug& plug,
                                    PlugAddress::EPlugDirection direction,
                                    ESamplingFrequency samplingFrequency )
{
    ExtendedStreamFormatCmd extStreamFormatCmd( m_1394Service,
                                                ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     plug.getPlugId() );
    extStreamFormatCmd.setPlugAddress( PlugAddress( direction,
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

bool
AvDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency )
{
    AvPlug* plug = getPlugById( m_isoInputPlugs, 0 );
    if ( !plug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }

    if ( !setSamplingFrequencyPlug( *plug, PlugAddress::ePD_Input, samplingFrequency ) ) {
        debugError( "setSampleRate: Setting sample rate failed\n" );
        return false;
    }

    plug = getPlugById( m_isoOutputPlugs, 0 );
    if ( !plug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

    if ( !setSamplingFrequencyPlug( *plug, PlugAddress::ePD_Output, samplingFrequency ) ) {
        debugError( "setSampleRate: Setting sample rate failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "setSampleRate: Set sample rate to %d\n",  convertESamplingFrequency( samplingFrequency ) );
    return true;
}

ConfigRom&
AvDevice::getConfigRom() const
{
    return *m_configRom;
}

void
AvDevice::showDevice() const
{
    printf( "showDevice: not implemented\n" );
}

bool AvDevice::setId( unsigned int id) {
	m_id=id;
	return true;
}

}
