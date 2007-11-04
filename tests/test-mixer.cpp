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
#include "debugmodule/debugmodule.h"

#include "libieee1394/ieee1394service.h"
#include <string.h>

DECLARE_GLOBAL_DEBUG_MODULE;

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

    debugOutput(DEBUG_LEVEL_NORMAL, "Requesting mixer programmable state...\n");

    fbCmd.m_pFBProcessing->m_fbInputPlugNumber = 0x00;
    fbCmd.m_pFBProcessing->m_inputAudioChannelNumber  = 0xff;
    fbCmd.m_pFBProcessing->m_outputAudioChannelNumber = 0xff;
    fbCmd.m_pFBProcessing->m_pEnhancedMixer->m_statusSelector
        = FunctionBlockProcessingEnhancedMixer::eSS_ProgramableState;

    fbCmd.setVerbose( bVerbose );
    if (bVerbose) {
        ieee1394service.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );
    }

    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
    }

    if ( bVerbose ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }
    
    debugOutput(DEBUG_LEVEL_NORMAL, "Requesting mixer level state...\n");
    
    fbCmd.m_pFBProcessing->m_fbInputPlugNumber = 0x00;
    fbCmd.m_pFBProcessing->m_inputAudioChannelNumber  = 0x00;
    fbCmd.m_pFBProcessing->m_outputAudioChannelNumber = 0x00;
    fbCmd.m_pFBProcessing->m_pEnhancedMixer->m_statusSelector
        = FunctionBlockProcessingEnhancedMixer::eSS_Level;
    
    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
    }

    if ( bVerbose ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    return true;
}

bool
selectorGet( Ieee1394Service& ieee1394service, int node_id, int fb_id )
{
    FunctionBlockCmd fbCmd( ieee1394service,
                            FunctionBlockCmd::eFBT_Selector,
                            fb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.m_pFBSelector->m_inputFbPlugNumber=0;

    debugOutput(DEBUG_LEVEL_NORMAL, "Requesting selector state...\n");

    fbCmd.setVerbose( bVerbose );
    if (bVerbose) {
        ieee1394service.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );
    }
    
    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
    }

    if ( bVerbose ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    return true;
}

bool
selectorSet( Ieee1394Service& ieee1394service, int node_id, int fb_id , int val )
{
    FunctionBlockCmd fbCmd( ieee1394service,
                            FunctionBlockCmd::eFBT_Selector,
                            fb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Control );
    fbCmd.m_pFBSelector->m_inputFbPlugNumber=val;

    debugOutput(DEBUG_LEVEL_NORMAL, "Setting selector state to %d...\n", val);

    fbCmd.setVerbose( bVerbose );
    if (bVerbose) {
        ieee1394service.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );
    }
    
    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
    }

    if ( bVerbose ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    return true;
}

bool
volumeGet( Ieee1394Service& ieee1394service, int node_id, int fb_id, int channel )
{
    FunctionBlockCmd fbCmd( ieee1394service,
                            FunctionBlockCmd::eFBT_Feature,
                            fb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.m_pFBFeature->m_audioChannelNumber=channel;
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_Volume;
    fbCmd.m_pFBFeature->m_pVolume->m_volume=0;

    debugOutput(DEBUG_LEVEL_NORMAL, "Requesting volume feature block state...\n");

    fbCmd.setVerbose( bVerbose );
    if (bVerbose) {
        ieee1394service.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );
    }
    
    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
    }

    if ( bVerbose ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    return true;
}

bool
volumeSet( Ieee1394Service& ieee1394service, int node_id, int fb_id, int channel, int value )
{
    FunctionBlockCmd fbCmd( ieee1394service,
                            FunctionBlockCmd::eFBT_Feature,
                            fb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( node_id );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Control );
    fbCmd.m_pFBFeature->m_audioChannelNumber=channel;
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_Volume;
    fbCmd.m_pFBFeature->m_pVolume->m_volume=value;

    debugOutput(DEBUG_LEVEL_NORMAL, "Setting volume feature block channel %d state to %d...\n", channel, value);

    fbCmd.setVerbose( bVerbose );
    if (bVerbose) {
        ieee1394service.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );
    }
    
    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
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

    if (argc < 4) {
        debugError("usage: PORT NODE_ID CMD FB_ID [VAL1] [VAL2]\n");
        exit(0);
    }

    int errno = 0;
    char* tail;
    int port = strtol( argv[1], &tail, 0 );
    int node_id = strtol( argv[2], &tail, 0 );
    int cmd = strtol( argv[3], &tail, 0 );
    int fb_id   = strtol( argv[4], &tail, 0 );
    
    int value1=-1;
    int value2=-1;
    
    if (argc>=6)
        value1 = strtol(argv[5], &tail, 0 );
        
    if (argc>=7)
        value2 = strtol(argv[6], &tail, 0 );
    
    if (errno) {
        debugError("argument parsing failed: %s", strerror(errno));
        return -1;
    }
    Ieee1394Service ieee1394service;
    if ( !ieee1394service.initialize( port ) ) {
        debugError( "could not set port on ieee1394service\n" );
        return -1;
    }

    switch(cmd) {
    case 0:
        doApp( ieee1394service, node_id, fb_id );
        break;
    case 1:
        selectorGet( ieee1394service, node_id, fb_id );
        break;
    case 2:
        selectorSet( ieee1394service, node_id, fb_id , value1 );
        break;
    case 3:
        volumeGet( ieee1394service, node_id, fb_id, value1);
        break;
    case 4:
        volumeSet( ieee1394service, node_id, fb_id, value1, value2);
        break;
    }

    return 0;
}
