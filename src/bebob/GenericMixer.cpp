/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "bebob/GenericMixer.h"

#include "bebob/bebob_avdevice.h"
#include "bebob/bebob_avdevice_subunit.h"

#include "libosc/OscNode.h"
#include "libosc/OscMessage.h"
#include "libosc/OscResponse.h"

#include "libavc/avc_function_block.h"
#include "libavc/avc_serialize.h"

#include "libieee1394/ieee1394service.h"

#include <sstream>
#include <assert.h>

using namespace OSC;

namespace BeBoB {

IMPL_DEBUG_MODULE( GenericMixer, GenericMixer, DEBUG_LEVEL_NORMAL );

GenericMixer::GenericMixer(
                Ieee1394Service& ieee1394service,
                AvDevice &d)
    : OSC::OscNode("GenericMixer")
    , m_p1394Service( ieee1394service )
    , m_device(d)
{

}

GenericMixer::~GenericMixer() {

}

OscResponse 
GenericMixer::processOscMessage(OscMessage *m) {
    debugOutput(DEBUG_LEVEL_NORMAL, "Message\n");
    
    unsigned int nbArgs=m->nbArguments();
    if (nbArgs>=1) {
        OscArgument arg0=m->getArgument(0);
        if(arg0.isString()) { // first argument should be a string command
            string cmd=arg0.getString();
            debugOutput( DEBUG_LEVEL_NORMAL, "(%p) CMD? %s\n", this, cmd.c_str());
            if(cmd == "list") {
                debugOutput( DEBUG_LEVEL_NORMAL, "Listing mixer elements...\n");
                OscMessage rm=mixerListChildren();
                return OscResponse(rm);
            }
            if(cmd == "set") {
                debugOutput( DEBUG_LEVEL_NORMAL, "setting mixer element...\n");
                bool args_ok=true;
                args_ok &= (nbArgs>=3);
                args_ok &= (m->getArgument(1).isString());
                args_ok &= (m->getArgument(2).isInt());

                if (!args_ok) {
                    debugWarning("set: Wrong nb of arguments\n");
                    return OscResponse(OscResponse::eError);
                }

                string type=m->getArgument(1).getString();
                int id=m->getArgument(2).getInt();
                
                if (type == "selector") {
                    if ( !(nbArgs==4)) {
                        debugWarning("set selector: Wrong nb of arguments\n");
                        return OscResponse(OscResponse::eError);
                    }
                    if(!(m->getArgument(3).isInt64() || m->getArgument(3).isFloat())) {
                        debugWarning("set selector: Wrong argument type\n");
                        return OscResponse(OscResponse::eError);
                    }
                        
                    int value=m->getArgument(3).getInt();
                    
                    if (m->getArgument(3).isFloat()) {
                        value=(int)(m->getArgument(3).getFloat());
                    }
                    
                    return mixerSetSelectorValue(id, value);
                }
                
                return OscResponse(OscResponse::eError);
            }
            if(cmd == "get") {
                debugOutput( DEBUG_LEVEL_NORMAL, "getting mixer element...\n");
                bool args_ok=true;
                args_ok &= (nbArgs>=3);
                args_ok &= (m->getArgument(1).isString());
                args_ok &= (m->getArgument(2).isInt());

                if (!args_ok) return OscResponse(OscResponse::eError);

                string type=m->getArgument(1).getString();
                int id=m->getArgument(2).getInt();
                
                if (type == "selector") {
                    return mixerGetSelectorValue(id);
                }
                
                return OscResponse(OscResponse::eError);

            }
        }
    }
    debugWarning("Unhandled OSC message!\n");
    // default action: don't change response
    return OscResponse(OscResponse::eUnhandled);
}

OscMessage
GenericMixer::mixerListChildren() {
    OscMessage m;

    debugOutput(DEBUG_LEVEL_NORMAL,"Listing mixer elements...\n");

    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        ostrm << (*it)->getName() << "/" << (int)((*it)->getId());
        m.addArgument(ostrm.str());
    }

    m.print();
    return m;
}

OscResponse
GenericMixer::mixerGetSelectorValue(int fb_id) {
    OscMessage m;
    
    debugOutput(DEBUG_LEVEL_NORMAL,"Get selector %d value...\n",
        fb_id);

    FunctionBlockCmd fbCmd( m_p1394Service,
                            FunctionBlockCmd::eFBT_Selector,
                            fb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( m_device.getNodeId() );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.m_pFBSelector->m_inputFbPlugNumber=0xFF;

    fbCmd.setVerbose( getDebugLevel() );
    
    if ( !fbCmd.fire() ) {
        debugError( "FunctionBlockCmd failed\n" );
        return OscResponse(OscResponse::eError);
    }

    if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }
    
    if (fbCmd.getResponse() != AVCCommand::eR_Accepted) {
        return OscResponse(OscResponse::eError);
    }
    
    m.addArgument((int32_t)(fbCmd.m_pFBSelector->m_inputFbPlugNumber));

    return OscResponse(m);
}

OscResponse
GenericMixer::mixerSetSelectorValue(int fb_id, int value) {
    OscMessage m;
    
    debugOutput(DEBUG_LEVEL_NORMAL,"Set selector %d to %d...\n",
        fb_id, value);

    FunctionBlockCmd fbCmd( m_p1394Service,
                            FunctionBlockCmd::eFBT_Selector,
                            fb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( m_device.getNodeId() );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Control );
    fbCmd.m_pFBSelector->m_inputFbPlugNumber=(value & 0xFF);

    fbCmd.setVerbose( getDebugLevel() );
    
    if ( !fbCmd.fire() ) {
        debugError( "FunctionBlockCmd failed\n" );
        return OscResponse(OscResponse::eError);
    }

    if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    if (fbCmd.getResponse() != AVCCommand::eR_Accepted) {
        return OscResponse(OscResponse::eError);
    }
    
    return OscResponse(m);
}


} // end of namespace BeBoB
