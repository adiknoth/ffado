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
getVolume( Ieee1394Service& ieee1394service, int node_id, int ffb_id,
           FunctionBlockCmd::EControlAttribute control_attrib )
{
    FunctionBlockCmd fbCmd( ieee1394service,
                            FunctionBlockCmd::eFBT_Feature,
                            ffb_id,
                            control_attrib );
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.m_pFBFeature->m_audioChannelNumber = 0;
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_Volume;
    fbCmd.m_pFBFeature->m_pVolume->m_volume = 0;

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

    return fbCmd.m_pFBFeature->m_pVolume->m_volume;
}

bool
setVolume( Ieee1394Service& ieee1394service, int node_id, int ffb_id, int vol )
{
    FunctionBlockCmd fbCmd( ieee1394service,
                            FunctionBlockCmd::eFBT_Feature,
                            ffb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Control );
    fbCmd.m_pFBFeature->m_audioChannelNumber = 0;
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_Volume;
    fbCmd.m_pFBFeature->m_pVolume->m_volume = vol;

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
doApp( Ieee1394Service& ieee1394service, int node_id, int fb_id, int vol )
{
    short int maxVolume = getVolume( ieee1394service, node_id, fb_id, FunctionBlockCmd::eCA_Maximum );
    short int minVolume = getVolume( ieee1394service, node_id, fb_id, FunctionBlockCmd::eCA_Minimum );
    short int curVolume = getVolume( ieee1394service, node_id, fb_id, FunctionBlockCmd::eCA_Current );
    printf( "max volume value = %d\n", maxVolume );
    printf( "min volume value = %d\n", minVolume );
    printf( "old volume value = %d\n", curVolume);

    setVolume( ieee1394service, node_id, fb_id, vol );

    curVolume = getVolume( ieee1394service, node_id, fb_id, FunctionBlockCmd::eCA_Current );
    printf( "new volume value = %d\n", curVolume );

    return true;
}

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{

    if (argc < 3) {
        printf("usage: NODE_ID FB_ID VOL\n");
        exit(0);
    }

    int errno = 0;
    char* tail;
    int node_id = strtol( argv[1], &tail, 0 );
    int fb_id   = strtol( argv[2], &tail, 0 );
    int vol     = strtol( argv[3], &tail, 0 );

    if (errno) {
        perror("argument parsing failed:");
        return -1;
    }
    Ieee1394Service ieee1394service;
    if ( !ieee1394service.initialize( 0 ) ) {
        fprintf( stderr, "could not set port on ieee1394service\n" );
        return -1;
    }

    doApp( ieee1394service, node_id, fb_id, vol );

    return 0;
}
