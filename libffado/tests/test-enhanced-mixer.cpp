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
#include "libutil/cmd_serialize.h"

#include "libieee1394/ieee1394service.h"
#include <cstdlib>
#include <cstring>

using namespace AVC;

bool
doApp( Ieee1394Service& ieee1394service, int node_id, int fb_id )
{
    AVC::FunctionBlockCmd fbCmd( ieee1394service,
                                 FunctionBlockCmd::eFBT_Processing,
                                 fb_id,
                                 FunctionBlockCmd::eCA_Current);
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );

    // Ok, this enhanced  mixer setting here is just a hack, we need
    // a sane way to set processing features (read pointer management)
    delete fbCmd.m_pFBProcessing->m_pMixer;
    fbCmd.m_pFBProcessing->m_pMixer = 0;
    AVC::FunctionBlockProcessingEnhancedMixer em;
    fbCmd.m_pFBProcessing->m_pEnhancedMixer = em.clone();

    fbCmd.m_pFBProcessing->m_fbInputPlugNumber = 0x00;
    fbCmd.m_pFBProcessing->m_inputAudioChannelNumber = 0xff;
    fbCmd.m_pFBProcessing->m_outputAudioChannelNumber = 0xff;
    fbCmd.m_pFBProcessing->m_pEnhancedMixer->m_statusSelector = 1;
    
    if ( !fbCmd.fire() ) {
        printf( "cmd failed\n" );
        return false;
    }
    
    // Util::Cmd::CoutSerializer se;
    // fbCmd.serialize( se );
    
    return true;
}

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{

    if (argc < 2) {
        printf("usage: NODE_ID FB_ID\n");
        exit(0);
    }

    errno = 0;
    char* tail;
    int node_id = strtol( argv[1], &tail, 0 );
    int fb_id   = strtol( argv[2], &tail, 0 );

    if (errno) {
        perror("argument parsing failed:");
        return -1;
    }
    Ieee1394Service ieee1394service;
    if ( !ieee1394service.initialize( 0 ) ) {
        fprintf( stderr, "could not set port on ieee1394service\n" );
        return -1;
    }

    doApp( ieee1394service, node_id, fb_id );

    return 0;
}
