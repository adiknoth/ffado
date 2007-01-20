/* test-volume.cpp
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

#include "libfreebobavc/avc_function_block.h"
#include "libfreebobavc/avc_serialize.h"
#include "libfreebobavc/ieee1394service.h"

const bool bVerbose = false;

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
    fbCmd.m_pFBFeature->m_pVolume = new FunctionBlockFeatureVolume;

    fbCmd.setVerbose( bVerbose );
    ieee1394service.setVerbose( bVerbose );

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
    fbCmd.m_pFBFeature->m_pVolume = new FunctionBlockFeatureVolume;
    fbCmd.m_pFBFeature->m_pVolume->m_volume = vol;

    fbCmd.setVerbose( bVerbose );
    ieee1394service.setVerbose( bVerbose );

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
