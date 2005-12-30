/* avdevice.cpp
 * Copyright (C) 2005 by Daniel Wagner
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

#include "debugmodule/debugmodule.h"

#include <iostream>

using namespace std;

IMPL_DEBUG_MODULE( AvDevice, AvDevice, DEBUG_LEVEL_VERBOSE );

AvDevice::AvDevice( Ieee1394Service* ieee1394service,
                    ConfigRom* configRom,
                    int nodeId )
    : m_1394Service( ieee1394service )
    , m_configRom( configRom )
    , m_nodeId( nodeId )
{
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
AvDevice::discoverStep4()
{
    //////////////////////////////////////////////
    // Step 4: For all ISO plugs with valid internal connections:
    // get type of data stream flowing through plug.
    // Stream type of interest: iso stream & sync stream

    for ( AvPlugVector::iterator it = m_isoInputPlugs.begin();
          it != m_isoInputPlugs.end();
          ++it )
    {
        AvPlug* isoInputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoInputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has no valid connecton -> skip\n",
                         isoInputPlug->getPlugId() );
            continue;
        }

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
            ExtendedPlugInfoInfoType::eIT_PlugType );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep4: plug type command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugType )
        {
            plug_type_t plugType = infoType->m_plugType->m_plugType;

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d is of type %d (%s)\n",
                         isoInputPlug->getPlugId(),
                         plugType,
                         extendedPlugInfoPlugTypeToString( plugType ) );

            isoInputPlug->m_plugType = plugType;
        }
    }

    for ( AvPlugVector::iterator it = m_isoOutputPlugs.begin();
          it != m_isoOutputPlugs.end();
          ++it )
    {
        AvPlug* isoOutputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoOutputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has no valid connecton -> skip\n",
                         isoOutputPlug->getPlugId() );
            continue;
        }

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
            ExtendedPlugInfoInfoType::eIT_PlugType );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep4: plug type command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugType )
        {
            plug_type_t plugType = infoType->m_plugType->m_plugType;

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso output plug %d is of type %d (%s)\n",
                         isoOutputPlug->getPlugId(),
                         plugType,
                         extendedPlugInfoPlugTypeToString( plugType ) );

            isoOutputPlug->m_plugType = plugType;
        }
    }

    return true;
}

bool
AvDevice::discoverStep5()
{
    //////////////////////////////////////////////
    // Step 5: For all ISO plugs with valid internal connections:
    // get data block size of the stream flowing through the plug

    for ( AvPlugVector::iterator it = m_isoInputPlugs.begin();
          it != m_isoInputPlugs.end();
          ++it )
    {
        AvPlug* isoInputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoInputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has no valid connecton -> skip\n",
                         isoInputPlug->getPlugId() );
            continue;
        }

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
            ExtendedPlugInfoInfoType::eIT_NoOfChannels );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep5: number of channels command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugNrOfChns )
        {
            nr_of_channels_t nrOfChannels
                = infoType->m_plugNrOfChns->m_nrOfChannels;

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has %d channels\n",
                         isoInputPlug->getPlugId(),
                         nrOfChannels );

            isoInputPlug->m_nrOfChannels = nrOfChannels;
        }
    }

    for ( AvPlugVector::iterator it = m_isoOutputPlugs.begin();
          it != m_isoOutputPlugs.end();
          ++it )
    {
        AvPlug* isoOutputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoOutputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has no valid connecton -> skip\n",
                         isoOutputPlug->getPlugId() );
            continue;
        }

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
            ExtendedPlugInfoInfoType::eIT_NoOfChannels );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep5: nubmer of channels command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugNrOfChns )
        {
            nr_of_channels_t nrOfChannels
                = infoType->m_plugNrOfChns->m_nrOfChannels;

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso output plug %d has %d channels\n",
                         isoOutputPlug->getPlugId(),
                         nrOfChannels );

            isoOutputPlug->m_nrOfChannels = nrOfChannels;
        }
    }
    return true;
}

bool
AvDevice::discoverStep6()
{
    //////////////////////////////////////////////
    // Step 6: For all ISO plugs with valid internal connections:
    // get position of channels within data block

    for ( AvPlugVector::iterator it = m_isoInputPlugs.begin();
          it != m_isoInputPlugs.end();
          ++it )
    {
        AvPlug* isoInputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoInputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has no valid connecton -> skip\n",
                         isoInputPlug->getPlugId() );
            continue;
        }

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
            ExtendedPlugInfoInfoType::eIT_ChannelPosition );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep6: channels position command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugChannelPosition )
        {
            if ( !isoInputPlug->copyClusterInfo(
                     *( infoType->m_plugChannelPosition ) ) )
            {
                debugError( "discoverStep6: Could not copy channel position "
                            "information\n" );
                return false;
            }

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d: channel position information "
                         "retrieved\n",
                         isoInputPlug->getPlugId() );

            isoInputPlug->debugOutputClusterInfos( DEBUG_LEVEL_VERBOSE );
        }
    }

    for ( AvPlugVector::iterator it = m_isoOutputPlugs.begin();
          it != m_isoOutputPlugs.end();
          ++it )
    {
        AvPlug* isoOutputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoOutputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has no valid connecton -> skip\n",
                         isoOutputPlug->getPlugId() );
            continue;
        }

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
            ExtendedPlugInfoInfoType::eIT_ChannelPosition );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverStep6: channel position command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugChannelPosition )
        {
            if ( !isoOutputPlug->copyClusterInfo(
                     *( infoType->m_plugChannelPosition ) ) )
            {
                debugError( "discoverStep6: Could not copy channel position "
                            "information\n" );
                return false;
            }

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso output plug %d: channel position information "
                         "retrieved\n",
                         isoOutputPlug->getPlugId() );

            isoOutputPlug->debugOutputClusterInfos( DEBUG_LEVEL_VERBOSE );
        }
    }

    return true;
}

bool
AvDevice::discoverStep7()
{
    //////////////////////////////////////////////
    // Step 7: For all ISO plugs with valid internal connections:
    // get hardware name of channel

    for ( AvPlugVector::iterator it = m_isoInputPlugs.begin();
          it != m_isoInputPlugs.end();
          ++it )
    {
        AvPlug* isoInputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoInputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has no valid connecton -> skip\n",
                         isoInputPlug->getPlugId() );
            continue;
        }

        for ( AvPlug::ClusterInfoVector::iterator clit =
                  isoInputPlug->m_clusterInfos.begin();
              clit != isoInputPlug->m_clusterInfos.end();
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
                                                 isoInputPlug->getPlugId() );
                extPlugInfoCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
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
                                 "iso input plug %d stream "
                                 "position %d: channel name = %s\n",
                                 isoInputPlug->getPlugId(),
                                 channelInfo->m_streamPosition,
                                 infoType->m_plugChannelName->m_plugChannelName.c_str() );
                    channelInfo->m_name =
                        infoType->m_plugChannelName->m_plugChannelName;
                }

            }
        }
    }

    for ( AvPlugVector::iterator it = m_isoOutputPlugs.begin();
          it != m_isoOutputPlugs.end();
          ++it )
    {
        AvPlug* isoOutputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoOutputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso output plug %d has no valid connecton -> skip\n",
                         isoOutputPlug->getPlugId() );
            continue;
        }


        for ( AvPlug::ClusterInfoVector::iterator clit =
                  isoOutputPlug->m_clusterInfos.begin();
              clit != isoOutputPlug->m_clusterInfos.end();
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
                                                 isoOutputPlug->getPlugId() );
                extPlugInfoCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Output,
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
                                 "iso output plug %d stream "
                                 "position %d: channel name = %s\n",
                                 isoOutputPlug->getPlugId(),
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
AvDevice::discoverStep8()
{
    //////////////////////////////////////////////
    // Step 8: For all ISO plugs with valid internal connections:
    // get logical input and output streams provided by the device

    for ( AvPlugVector::iterator it = m_isoInputPlugs.begin();
          it != m_isoInputPlugs.end();
          ++it )
    {
        AvPlug* isoInputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoInputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has no valid connecton -> skip\n",
                         isoInputPlug->getPlugId() );
            continue;
        }

        for ( AvPlug::ClusterInfoVector::iterator clit =
                  isoInputPlug->m_clusterInfos.begin();
              clit != isoInputPlug->m_clusterInfos.end();
              ++clit )
        {
            AvPlug::ClusterInfo* clusterInfo = &*clit;

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
                ExtendedPlugInfoInfoType::eIT_ClusterInfo );
            extendedPlugInfoInfoType.initialize();
            extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

            extPlugInfoCmd.getInfoType()->m_plugClusterInfo->m_clusterIndex =
                clusterInfo->m_index;

            if ( !extPlugInfoCmd.fire() ) {
                debugError( "discoverStep8: cluster info command failed\n" );
                return false;
            }

            ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
            if ( infoType
                 && infoType->m_plugClusterInfo )
            {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "iso input plug %d: cluster index = %d, "
                             "portType %s, cluster name = %s\n",
                             isoInputPlug->getPlugId(),
                             infoType->m_plugClusterInfo->m_clusterIndex,
                             extendedPlugInfoClusterInfoPortTypeToString(
                                 infoType->m_plugClusterInfo->m_portType ),
                             infoType->m_plugClusterInfo->m_clusterName.c_str() );

                clusterInfo->m_portType = infoType->m_plugClusterInfo->m_portType;
                clusterInfo->m_name = infoType->m_plugClusterInfo->m_clusterName;
            }
        }
    }

    for ( AvPlugVector::iterator it = m_isoOutputPlugs.begin();
          it != m_isoOutputPlugs.end();
          ++it )
    {
        AvPlug* isoOutputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoOutputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso output plug %d has no valid connecton -> skip\n",
                         isoOutputPlug->getPlugId() );
            continue;
        }

        // XXX Don't know why plug 1 can't be questioned about the cluster info.
        // Until I know why this is not working, it's better to skip this then to
        // break the discovering.
        if ( isoOutputPlug->getPlugId() > 0 ) {
            debugWarning( "Skipping plugs with id > 0. Further investigation needed\n" );
            continue;
        }

        for ( AvPlug::ClusterInfoVector::iterator clit =
                  isoOutputPlug->m_clusterInfos.begin();
              clit != isoOutputPlug->m_clusterInfos.end();
              ++clit )
        {
            AvPlug::ClusterInfo* clusterInfo = &*clit;

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
                ExtendedPlugInfoInfoType::eIT_ClusterInfo );
            extendedPlugInfoInfoType.initialize();
            extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

            extPlugInfoCmd.getInfoType()->m_plugClusterInfo->m_clusterIndex =
                clusterInfo->m_index;

            if ( !extPlugInfoCmd.fire() ) {
                debugError( "discoverStep8: cluster info command failed\n" );
                return false;
            }

            ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
            if ( infoType
                 && infoType->m_plugClusterInfo )
            {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "iso output plug %d: cluster index = %d, "
                             "portType %s, cluster name = %s\n",
                             isoOutputPlug->getPlugId(),
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
AvDevice::discoverStep9()
{
    //////////////////////////////////////////////
    // Step 9: For all ISO plugs with valid internal connections:
    // get current stream format

    for ( AvPlugVector::iterator it = m_isoInputPlugs.begin();
          it != m_isoInputPlugs.end();
          ++it )
    {
        AvPlug* isoInputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoInputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso input plug %d has no valid connecton -> skip\n",
                         isoInputPlug->getPlugId() );
            continue;
        }

        ExtendedStreamFormatCmd extStreamFormatCmd( m_1394Service );
        UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                         isoInputPlug->getPlugId() );
        extStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
                                                        PlugAddress::ePAM_Unit,
                                                        unitPlugAddress ) );

        extStreamFormatCmd.setNodeId( m_nodeId );
        extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
        //extStreamFormatCmd.setVerbose( true );

        if ( !extStreamFormatCmd.fire() ) {
            debugError( "discoverStep9: stream format command failed\n" );
            return false;
        }

        FormatInformation* formatInfo =
            extStreamFormatCmd.getFormatInformation();
        FormatInformationStreamsCompound* compoundStream
            = dynamic_cast< FormatInformationStreamsCompound* > (
                formatInfo->m_streams );
        if ( compoundStream ) {
            isoInputPlug->m_samplingFrequency =
                compoundStream->m_samplingFrequency;
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "discoverStep9: iso input plug %d uses "
                         "sampling frequency %d\n",
                         isoInputPlug->getPlugId(),
                         isoInputPlug->m_samplingFrequency );

            for ( int i = 1;
                  i <= compoundStream->m_numberOfStreamFormatInfos;
                  ++i )
            {
                AvPlug::ClusterInfo* clusterInfo =
                    isoInputPlug->getClusterInfoByIndex( i );

                if ( !clusterInfo ) {
                    debugError( "discoverStep9: No matching cluster info found "
                                "for index %d\n",  i );
                    return false;
                }
                StreamFormatInfo* streamFormatInfo =
                    compoundStream->m_streamFormatInfos[ i - 1 ];

                // sanity checks
                if ( clusterInfo->m_nrOfChannels !=
                     streamFormatInfo->m_numberOfChannels )
                {
                    debugError( "discoverStep9: Number of channels "
                                "mismatch: iso input plug %d discovering reported "
                                "%d channels for cluster '%s', while stream format "
                                "reported %d\n",
                                isoInputPlug->getPlugId(),
                                clusterInfo->m_nrOfChannels,
                                clusterInfo->m_name.c_str(),
                                streamFormatInfo->m_numberOfChannels);
                    return false;
                }
                clusterInfo->m_streamFormat = streamFormatInfo->m_streamFormat;

                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "iso input plug %d cluster info %d ('%s'): stream format %d\n",
                             isoInputPlug->getPlugId(),
                             i,
                             clusterInfo->m_name.c_str(),
                             clusterInfo->m_streamFormat );
            }
        }

        FormatInformationStreamsSync* syncStream
            = dynamic_cast< FormatInformationStreamsSync* > (
                formatInfo->m_streams );
        if ( syncStream ) {
            isoInputPlug->m_samplingFrequency =
                syncStream->m_samplingFrequency;
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "iso input plug %d is sync stream with sampling frequency %d\n",
                             isoInputPlug->getPlugId(),
                             isoInputPlug->m_samplingFrequency );
        }

        if ( !compoundStream && !syncStream ) {
            debugError( "discoverStep9: Unsupported stream format\n" );
            return false;
        }
    }

    for ( AvPlugVector::iterator it = m_isoOutputPlugs.begin();
          it != m_isoOutputPlugs.end();
          ++it )
    {
        AvPlug* isoOutputPlug = *it;

        AvPlugConnection* plugConnection = getPlugConnection( *isoOutputPlug );
        if ( !plugConnection ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "iso output plug %d has no valid connecton -> skip\n",
                         isoOutputPlug->getPlugId() );
            continue;
        }

        ExtendedStreamFormatCmd extStreamFormatCmd( m_1394Service );
        UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                         isoOutputPlug->getPlugId() );
        extStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Output,
                                                        PlugAddress::ePAM_Unit,
                                                        unitPlugAddress ) );

        extStreamFormatCmd.setNodeId( m_nodeId );
        extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
        //extStreamFormatCmd.setVerbose( true );

        if ( !extStreamFormatCmd.fire() ) {
            debugError( "discoverStep9: stream format command failed\n" );
            return false;
        }

        FormatInformation* formatInfo =
            extStreamFormatCmd.getFormatInformation();
        FormatInformationStreamsCompound* compoundStream
            = dynamic_cast< FormatInformationStreamsCompound* > (
                formatInfo->m_streams );
        if ( compoundStream ) {
            isoOutputPlug->m_samplingFrequency =
                compoundStream->m_samplingFrequency;
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "discoverStep9: iso output plug %d uses "
                         "sampling frequency %d\n",
                         isoOutputPlug->getPlugId(),
                         isoOutputPlug->m_samplingFrequency );

            for ( int i = 1;
                  i <= compoundStream->m_numberOfStreamFormatInfos;
                  ++i )
            {
                AvPlug::ClusterInfo* clusterInfo =
                    isoOutputPlug->getClusterInfoByIndex( i );

                if ( !clusterInfo ) {
                    debugError( "discoverStep9: No matching cluster info found "
                                "for index %d\n",  i );
                    return false;
                }
                StreamFormatInfo* streamFormatInfo =
                    compoundStream->m_streamFormatInfos[ i - 1 ];

                // sanity checks
                if ( clusterInfo->m_nrOfChannels !=
                     streamFormatInfo->m_numberOfChannels )
                {
                    debugError( "discoverStep9: Number of channels "
                                "mismatch: iso output plug %d discovering reported "
                                "%d channels for cluster '%s', while stream format "
                                "reported %d\n",
                                isoOutputPlug->getPlugId(),
                                clusterInfo->m_nrOfChannels,
                                clusterInfo->m_name.c_str(),
                                streamFormatInfo->m_numberOfChannels);
                    return false;
                }
                clusterInfo->m_streamFormat = streamFormatInfo->m_streamFormat;

                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "iso output plug %d cluster info %d ('%s'): stream format %d\n",
                             isoOutputPlug->getPlugId(),
                             i,
                             clusterInfo->m_name.c_str(),
                             clusterInfo->m_streamFormat );
            }
        }

        FormatInformationStreamsSync* syncStream
            = dynamic_cast< FormatInformationStreamsSync* > (
                formatInfo->m_streams );
        if ( syncStream ) {
            isoOutputPlug->m_samplingFrequency =
                syncStream->m_samplingFrequency;
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "iso output plug %d is sync stream with sampling frequency %d\n",
                             isoOutputPlug->getPlugId(),
                             isoOutputPlug->m_samplingFrequency );
        }

        if ( !compoundStream && !syncStream ) {
            debugError( "discoverStep9: Unsupported stream format\n" );
            return false;
        }
    }

    return true;
}

bool AvDevice::discoverPlugConnection( AvPlug& srcPlug,
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
        max_subunit_id_t max_subunit_id
            = subUnitInfoCmd.m_table[i].m_max_subunit_id;

        unsigned int subunitId = getNrOfSubunits( subunit_type );

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "subunit_id = %2d, subunit_type = %2d (%s), "
                     "max_subunit_ID = %d\n",
                     subunitId,
                     subunit_type,
                     subunitTypeToString( subunit_type ),
                     max_subunit_id );

        AvDeviceSubunit* subunit = 0;
        switch( subunit_type ) {
        case AVCCommand::eST_Audio:
            subunit = new AvDeviceSubunitAudio( this, subunitId );
            if ( !subunit ) {
                debugFatal( "Could not allocate AvDeviceSubunitAudio\n" );
                return false;
            }
            break;
        case AVCCommand::eST_Music:
            subunit = new AvDeviceSubunitMusic( this, subunitId );
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

std::string
AvDevice::getVendorName()
{
    return m_configRom->getModelName();
}

std::string
AvDevice::getModelName()
{
    return m_configRom->getVendorName();
}

bool
AvDevice::addPlugToXmlDescription( AvPlug& plug,
                                   xmlNodePtr connectionSet )
{
    char* result;
    asprintf( &result, "%d", plug.getPlugDirection() );
    if ( !xmlNewChild( connectionSet,
                       0,
                       BAD_CAST "Direction",
                       BAD_CAST result ) )
    {
        debugError( "Couldn't create direction node\n" );
        return false;
    }

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
        return false;
    }

    asprintf( &result, "%d", m_1394Service->getPort() );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Port",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Port' node\n" );
        return false;
    }

    asprintf( &result, "%d",  m_nodeId);
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "NodeId",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'NodeId' node\n" );
        return false;
    }

    asprintf( &result, "%d",  plug.getNrOfChannels() );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Dimension",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Dimension' node\n" );
        return false;
    }

    asprintf( &result, "%d",  plug.getSampleRate() );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Samplerate",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Samplerate' node\n" );
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
                return false;
            }

            asprintf( &result, "%d", channelInfo->m_location );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Location",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Location' node" );
                return false;
            }

            asprintf( &result, "%d", clusterInfo->m_streamFormat );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Format",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Format' node" );
                return false;
            }

            asprintf( &result, "%d", clusterInfo->m_portType );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Type",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Type' node" );
                return false;
            }

            // \todo XXX: What do to do with DestinationPort value??
            asprintf( &result, "%d", 0 );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "DestinationPort",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'DestinationPort' node" );
                return false;
            }

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

    xmlNodePtr connectionSet = xmlNewChild( deviceNode, 0,
                                            BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "Couldn't create connection set node for "
                    "direction 1 (playback)\n" );
        return false;
    }

    AvPlug* inputPlug = getPlugById( m_isoInputPlugs, 0 );
    if ( !inputPlug ) {
        debugError( "No iso input plug found with id %d\n" );
        return false;
    }
    if ( !addPlugToXmlDescription( *inputPlug, connectionSet ) ) {
        debugError( "Could not add iso input plug 0 to XML description\n" );
        return false;
    }

    connectionSet = xmlNewChild( deviceNode, 0,
                                 BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "Couldn't create connection set node for "
                    "direction 0 (recorder)\n" );
        return false;
    }

    AvPlug* outputPlug = getPlugById( m_isoOutputPlugs, 0 );
    if ( !outputPlug ) {
        debugError( "No iso output plug found with id %d\n" );
        return false;
    }
    if ( !addPlugToXmlDescription( *outputPlug, connectionSet ) ) {
        debugError( "Could not add iso output plug 0 to XML description\n" );
        return false;
    }

    return true;
}
