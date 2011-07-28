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

/*
 * "unmute-ozonic" by Mark Brand (orania), based on "test-enhanced-mixer" by Daniel Wagner
 * This is a knee-jerk reaction to the pathetic situation of requiring a dual-boot setup
 * for the sole and only purpose of unmuting the outputs of my M-Audio Ozonic.  Seriously!? ;)
 *
 * This utility requires patches to avc_function_block.cpp and bebob_function_block.cpp
 * which might be problematic for other devices?  I've included my patch to tests/SConscript.
 *
 * Also had a preliminary go at the mixer, which seems to work... but is incomplete.
 * All were originally based on revision 1957, though everything works for me under 1985.
 */

#include "libavc/audiosubunit/avc_function_block.h"
#include "libutil/cmd_serialize.h"

#include "libieee1394/ieee1394service.h"
#include <cstdlib>
#include <cstring>

using namespace AVC;

bool
doApp( Ieee1394Service& ieee1394service, int node_id, int fb_id, int inputChannel, int outputChannel, int value )
{
    AVC::FunctionBlockCmd fbCmd( ieee1394service,
                                 FunctionBlockCmd::eFBT_Processing,
                                 fb_id,
                                 FunctionBlockCmd::eCA_Current);
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Control );

    // Daniel says: "Ok, this enhanced mixer setting here is just a hack, we need
    // a sane way to set processing features (read pointer management)"
    delete fbCmd.m_pFBProcessing->m_pMixer;
    fbCmd.m_pFBProcessing->m_pMixer = 0;
    AVC::FunctionBlockProcessingEnhancedMixer em;
    fbCmd.m_pFBProcessing->m_pEnhancedMixer = em.clone();

    fbCmd.m_pFBProcessing->m_inputAudioChannelNumber = inputChannel;
    fbCmd.m_pFBProcessing->m_outputAudioChannelNumber = outputChannel;
    fbCmd.m_pFBProcessing->m_pEnhancedMixer->m_statusSelector = FunctionBlockProcessingEnhancedMixer::eSS_Level;

    fbCmd.m_pFBProcessing->m_pEnhancedMixer->m_LevelData.clear();
    fbCmd.m_pFBProcessing->m_pEnhancedMixer->m_LevelData.push_back((mixer_level_t) value);

    if ( !fbCmd.fire() ) {
        printf( "cmd failed\n" );
        return false;
    }

    return true;
}

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{
    Ieee1394Service ieee1394service;
    if ( !ieee1394service.initialize( 0 ) ) {
        fprintf( stderr, "could not set port on ieee1394service\n" );
        return -1;
    }

    /* MB: various values work below, these chosen *almost* arbitrarily */
    doApp( ieee1394service, 0, 1, 1, 1, 0 );
    return 0;
}
