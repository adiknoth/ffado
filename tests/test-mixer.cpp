/*
 * Copyright (C) 2005-2007 by by Daniel Wagner
 * Copyright (C) 2005-2007 by by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * FFADO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FFADO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFADO; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "libavc/avc_function_block.h"
#include "libavc/avc_serialize.h"

#include "libieee1394/ieee1394service.h"

const bool bVerbose = true;

bool
doApp( Ieee1394Service& ieee1394service, int node_id, int fb_id )
{
    FunctionBlockCmd fbCmd( ieee1394service,
                            FunctionBlockCmd::eFBT_Processing,
                            fb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.m_pFBProcessing->m_pEnhancedMixer = new FunctionBlockProcessingEnhancedMixer;

    fbCmd.m_pFBProcessing->m_fbInputPlugNumber = 0x01;
    fbCmd.m_pFBProcessing->m_inputAudioChannelNumber  = 0xff;
    fbCmd.m_pFBProcessing->m_outputAudioChannelNumber = 0xff;
    fbCmd.m_pFBProcessing->m_pEnhancedMixer->m_statusSelector
        = FunctionBlockProcessingEnhancedMixer::eSS_ProgramableState;
//     fbCmd.m_pFBProcessing->m_pEnhancedMixer->m_statusSelector
//         = FunctionBlockProcessingEnhancedMixer::eSS_Level;

    fbCmd.setVerbose( bVerbose );
    if (bVerbose) {
        ieee1394service.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );
    }

    if ( !fbCmd.fire() ) {
        printf( "cmd failed\n" );
    }

    if ( bVerbose ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    return true;
}

///////////////////////////
// main
//////////////////////////
int
main( int argc, char **argv )
{

    if (argc < 3) {
        printf("usage: PORT NODE_ID FB_ID\n");
        exit(0);
    }

    int errno = 0;
    char* tail;
    int port = strtol( argv[1], &tail, 0 );
    int node_id = strtol( argv[2], &tail, 0 );
    int fb_id   = strtol( argv[3], &tail, 0 );

    if (errno) {
        perror("argument parsing failed:");
        return -1;
    }
    Ieee1394Service ieee1394service;
    if ( !ieee1394service.initialize( port ) ) {
        fprintf( stderr, "could not set port on ieee1394service\n" );
        return -1;
    }

    doApp( ieee1394service, node_id, fb_id );

    return 0;
}
