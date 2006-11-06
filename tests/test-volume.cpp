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

#include "libfreebobavc/avc_feature_function_block.h"
#include "libfreebobavc/serialize.h"
#include "libfreebobavc/ieee1394service.h"

short int getMaxVolume(Ieee1394Service& ieee1394service, int node_id, int ffb_id)
{
    FeatureFunctionBlockCmd ffbCmd( &ieee1394service );
    ffbCmd.setNodeId( node_id );
    ffbCmd.setSubunitId( 0x00 );
    ffbCmd.setCommandType( AVCCommand::eCT_Status );
    ffbCmd.setFunctionBlockId( ffb_id );
    ffbCmd.setControlAttribute(FeatureFunctionBlockCmd::eCA_Maximum);
        
    if ( !ffbCmd.fire() ) {
        printf( "cmd failed\n" );
    }
    return ffbCmd.m_volumeControl.m_volume;
}

short int getMinVolume(Ieee1394Service& ieee1394service, int node_id, int ffb_id)
{
    FeatureFunctionBlockCmd ffbCmd( &ieee1394service );
    ffbCmd.setNodeId( node_id );
    ffbCmd.setSubunitId( 0x00 );
    ffbCmd.setCommandType( AVCCommand::eCT_Status );
    ffbCmd.setFunctionBlockId( ffb_id );
    ffbCmd.setControlAttribute(FeatureFunctionBlockCmd::eCA_Minimum);

    if ( !ffbCmd.fire() ) {
        printf( "cmd failed\n" );
    }
    return ffbCmd.m_volumeControl.m_volume;
}

short int getCurrentVolume(Ieee1394Service& ieee1394service, int node_id, int ffb_id)
{
    FeatureFunctionBlockCmd ffbCmd( &ieee1394service );
    ffbCmd.setNodeId( node_id );
    ffbCmd.setSubunitId( 0x00 );
    ffbCmd.setCommandType( AVCCommand::eCT_Status );
    ffbCmd.setFunctionBlockId( ffb_id );

    if ( !ffbCmd.fire() ) {
        printf( "cmd failed\n" );
    }
    return ffbCmd.m_volumeControl.m_volume;
}

bool
doApp(Ieee1394Service& ieee1394service, int node_id, int ffb_id, int vol )
{
    short int maxVolume = getMaxVolume(ieee1394service, node_id, ffb_id);
    short int minVolume = getMinVolume(ieee1394service, node_id, ffb_id);

    printf("max volume value = %d\n", maxVolume);
    printf("min volume value = %d\n", minVolume);

    short int volume = getCurrentVolume(ieee1394service, node_id, ffb_id);
    printf("old volume value = %d\n", volume);

    FeatureFunctionBlockCmd ffbCmd( &ieee1394service );
    ffbCmd.setNodeId( node_id );
    ffbCmd.setSubunitId( 0x00 );
    ffbCmd.setCommandType( AVCCommand::eCT_Control );
    ffbCmd.setFunctionBlockId( ffb_id );
    ffbCmd.m_volumeControl.m_volume = vol;

    if ( !ffbCmd.fire() ) {
        printf( "cmd failed\n" );
    }

    volume = getCurrentVolume(ieee1394service, node_id, ffb_id);
    printf("new volume value = %d\n", volume);

    return true;
}

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{

    if (argc < 3) {
        printf("usage: NODE_ID FFB_ID VOL\n");
        exit(0);
    }

    int errno = 0;
    char* tail;
    int node_id = strtol( argv[1], &tail, 0 );
    int ffb_id  = strtol( argv[2], &tail, 0 );
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

    doApp( ieee1394service, node_id, ffb_id, vol );

    return 0;
}
