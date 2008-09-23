/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "libavc/audiosubunit/avc_function_block.h"
#include "libutil/serialize.h"
#include "libutil/cmd_serialize.h"

#include "libieee1394/ieee1394service.h"

const bool bVerbose = false;

using namespace AVC;
using namespace Util;
using namespace Util::Cmd;

short int
getPan( Ieee1394Service& ieee1394service, int node_id, int ffb_id,
           FunctionBlockCmd::EControlAttribute control_attrib )
{
    FunctionBlockCmd fbCmd( ieee1394service,
                            FunctionBlockCmd::eFBT_Feature,
                            ffb_id,
                            control_attrib );
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.m_pFBFeature->m_audioChannelNumber = 2;
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_LRBalance;
    AVC::FunctionBlockFeatureLRBalance lr;
    fbCmd.m_pFBFeature->m_pLRBalance = lr.clone();
    fbCmd.m_pFBFeature->m_pLRBalance->m_lrBalance = 0;

    fbCmd.setVerbose( bVerbose );
    if (bVerbose) {
        ieee1394service.setVerboseLevel( DEBUG_LEVEL_VERBOSE );
    }

    if ( !fbCmd.fire() ) {
        printf( "cmd failed\n" );
    }

    if ( bVerbose ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    return fbCmd.m_pFBFeature->m_pLRBalance->m_lrBalance;
}

bool
setPan( Ieee1394Service& ieee1394service, int node_id, int ffb_id, int pan )
{
    FunctionBlockCmd fbCmd( ieee1394service,
                            FunctionBlockCmd::eFBT_Feature,
                            ffb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Control );
    fbCmd.m_pFBFeature->m_audioChannelNumber = 2;
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_LRBalance;
    AVC::FunctionBlockFeatureLRBalance lr;
    fbCmd.m_pFBFeature->m_pLRBalance = lr.clone();
    fbCmd.m_pFBFeature->m_pLRBalance->m_lrBalance = pan;

    fbCmd.setVerbose( bVerbose );
    if (bVerbose) {
        ieee1394service.setVerboseLevel( DEBUG_LEVEL_VERBOSE );
    }

    bool bStatus = fbCmd.fire();
    if ( !bStatus ) {
        printf( "cmd failed\n" );
    }

    if ( bVerbose ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    return bStatus;
}

bool
doApp( Ieee1394Service& ieee1394service, int node_id, int fb_id, int pan )
{
    short int maxPan = getPan( ieee1394service, node_id, fb_id, FunctionBlockCmd::eCA_Maximum );
    short int minPan = getPan( ieee1394service, node_id, fb_id, FunctionBlockCmd::eCA_Minimum );
    short int curPan = getPan( ieee1394service, node_id, fb_id, FunctionBlockCmd::eCA_Current );
    printf( "max pan value = %d\n", maxPan );
    printf( "min pan value = %d\n", minPan );
    printf( "old pan value = %d\n", curPan );

    //setPan( ieee1394service, node_id, fb_id, pan );

    curPan = getPan( ieee1394service, node_id, fb_id, FunctionBlockCmd::eCA_Current );
    printf( "new pan value = %d\n", curPan );

    return true;
}

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{

    if (argc < 4) {
        printf("usage: NODE_ID FB_ID PAN\n");
        exit(0);
    }

    int errno = 0;
    char* tail;
    int node_id = strtol( argv[1], &tail, 0 );
    int fb_id   = strtol( argv[2], &tail, 0 );
    int pan     = strtol( argv[3], &tail, 0 );

    if (errno) {
        perror("argument parsing failed:");
        return -1;
    }
    Ieee1394Service ieee1394service;
    if ( !ieee1394service.initialize( 0 ) ) {
        fprintf( stderr, "could not set port on ieee1394service\n" );
        return -1;
    }

    doApp( ieee1394service, node_id, fb_id, pan );

    return 0;
}
