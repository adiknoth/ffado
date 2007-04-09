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

#include <string>
#include <sstream>
#include <iostream>

#include <assert.h>

using namespace OSC;

namespace BeBoB {

template <class T>
bool from_string(T& t, 
                 const std::string& s, 
                 std::ios_base& (*f)(std::ios_base&))
{
  std::istringstream iss(s);
  return !(iss >> f >> t).fail();
}

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
    debugOutput( DEBUG_LEVEL_NORMAL, "PROCESS: %s\n", m->getPath().c_str());
    
    std::string path=m->getPath();
    
    // delete leading slashes
    if(path.find_first_of('/')==0) path=path.substr(1,path.size());

    // delete trailing slashes
    if(path.find_last_of('/')==path.size()-1) path=path.substr(0,path.size()-1);
    
    std::string target="";
    std::string newpath="";
    
    if (path == "") return processOscMessageRoot(m);
    
    // not targetted at the root node, continue processing
    int firstsep=path.find_first_of('/');
    if (firstsep == -1) { // this will most likely be a list request
        target=path;
        newpath="";
    } else {
        target=path.substr(0,firstsep);
        newpath=path.substr(firstsep+1);
    }
    
    if(target == "Selector") return processOscMessageSelector(newpath, m);
    if(target == "Feature") return processOscMessageFeature(newpath, m);
    if(target == "Processing") return processOscMessageProcessing(newpath, m);
    
    // unhandled
    return OscResponse(OscResponse::eUnhandled);
}

//// MIXER ROOT MESSAGES

OscResponse 
GenericMixer::processOscMessageRoot(OscMessage *m) {

    unsigned int nbArgs=m->nbArguments();
    if (nbArgs>=1) {
        OscArgument arg0=m->getArgument(0);
        if(arg0.isString()) { // first argument should be a string command
            string cmd=arg0.getString();
            debugOutput( DEBUG_LEVEL_NORMAL, "(%p) CMD: %s\n", this, cmd.c_str());
            
            if(cmd == "list") {
                debugOutput( DEBUG_LEVEL_NORMAL, "Listing root elements...\n");
                return OscResponse(rootListChildren());
            }
//             if(cmd == "params") {
//                 debugOutput( DEBUG_LEVEL_NORMAL, "Listing root params...\n");
//                 return OscResponse(rootListParams());
//             }
//             if(cmd == "get") {
//                 
//             }
//             if(cmd == "set") {
//                 
//             }
            return OscResponse(OscResponse::eUnhandled);
        } else {
            debugWarning("Wrong command datatype\n");
            return OscResponse(OscResponse::eError);
        }
    } else {
        debugWarning("Not enough arguments\n");
        return OscResponse(OscResponse::eError);
    }
}

OscResponse
GenericMixer::rootListChildren() {
    OscMessage m;

    debugOutput(DEBUG_LEVEL_NORMAL,"Listing root elements...\n");

    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    bool hasSelector=false;
    bool hasFeature=false;
    bool hasProcessing=false;
    bool hasCodec=false;

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        hasSelector |= (((*it)->getType())==FunctionBlockCmd::eFBT_Selector);
        hasFeature |= (((*it)->getType())==FunctionBlockCmd::eFBT_Feature);
        hasProcessing |= (((*it)->getType())==FunctionBlockCmd::eFBT_Processing);
        hasCodec |= (((*it)->getType())==FunctionBlockCmd::eFBT_Codec);
    }

    if (hasSelector) m.addArgument("Selector");
    if (hasFeature) m.addArgument("Feature");
    if (hasProcessing) m.addArgument("Processing");
    if (hasCodec) m.addArgument("Codec");

    return OscResponse(m);
}

//// SELECTOR MESSAGES

OscResponse 
GenericMixer::processOscMessageSelector(std::string path, OscMessage *m) {
    debugOutput( DEBUG_LEVEL_NORMAL, "Selector: %s\n", path.c_str());
    
    // delete leading slashes
    if(path.find_first_of('/')==0) path=path.substr(1,path.size());

    // delete trailing slashes
    if(path.find_last_of('/')==path.size()-1) path=path.substr(0,path.size()-1);
    
    int id=-1;
    if ((path != "") && (!from_string<int>(id, path, std::dec))) {
        debugWarning( "could not get id\n" );
        return OscResponse(OscResponse::eError);
    }
    
    unsigned int nbArgs=m->nbArguments();
    if (nbArgs>=1) {
        OscArgument arg0=m->getArgument(0);
        if(arg0.isString()) { // first argument should be a string command
            string cmd=arg0.getString();
            debugOutput( DEBUG_LEVEL_NORMAL, "(%p) CMD: %s\n", this, cmd.c_str());
            
            if(cmd == "list") {
                debugOutput( DEBUG_LEVEL_NORMAL, "Listing selector elements...\n");
                return selectorListChildren(id);
            }
            if(cmd == "params") {
                debugOutput( DEBUG_LEVEL_NORMAL, "Listing selector %d params...\n", id);
                return selectorListParams(id);
            }
            if(cmd == "get") {
                if (nbArgs<2 || !m->getArgument(1).isString()) {
                    debugWarning("Argument error\n");
                    m->print();
                    return OscResponse(OscResponse::eError);
                }
                debugOutput( DEBUG_LEVEL_NORMAL, "Getting selector params %s...\n",
                    m->getArgument(1).getString().c_str());
                return selectorGetParam(id,m->getArgument(1).getString(), m);
            }
            if(cmd == "set") {
                if (nbArgs<3 || !m->getArgument(1).isString() || !m->getArgument(2).isNumeric()) {
                    debugWarning("Argument error\n");
                    m->print();
                    return OscResponse(OscResponse::eError);
                }
                debugOutput( DEBUG_LEVEL_NORMAL, "Setting selector param %s...\n",
                    m->getArgument(1).getString().c_str());
                return selectorSetParam(id,m->getArgument(1).getString(), m);
            }
            return OscResponse(OscResponse::eUnhandled);
        } else {
            debugWarning("Wrong command datatype\n");
            return OscResponse(OscResponse::eError);
        }
    } else {
        debugWarning("Not enough arguments\n");
        return OscResponse(OscResponse::eError);
    }
}

/*
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
                    if(!m->getArgument(3).isNumeric()) {
                        debugWarning("set selector: Wrong argument type\n");
                        return OscResponse(OscResponse::eError);
                    }
                    int value=m->getArgument(3).toInt();
                    
                    return setSelectorValue(id, value);
                }
                
                if (type == "volume") {
                    if ( !(nbArgs>=5)) {
                        debugWarning("set volume: Wrong nb of arguments\n");
                        return OscResponse(OscResponse::eError);
                    }
                    if(!m->getArgument(3).isNumeric()) {
                        debugWarning("set volume: Wrong argument (3) type\n");
                        return OscResponse(OscResponse::eError);
                    }
                    if(!m->getArgument(4).isNumeric()) {
                        debugWarning("set volume: Wrong argument (4) type\n");
                        return OscResponse(OscResponse::eError);
                    }
                    
                    int channel=m->getArgument(3).toInt();
                    int volume=m->getArgument(4).toInt();
                    
                    return mixerSetFeatureVolumeValue(id, channel, volume);
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
                
                if (type == "volume") {
                    if ( !(nbArgs==6)) {
                        debugWarning("set volume: Wrong nb of arguments\n");
                        return OscResponse(OscResponse::eError);
                    }
                    if(!m->getArgument(3).isNumeric()) {
                        debugWarning("set volume: Wrong argument (3) type\n");
                        return OscResponse(OscResponse::eError);
                    }

                    int channel=m->getArgument(3).toInt();
                    
                    return mixerGetFeatureVolumeValue(id, channel);
                }
                
                return OscResponse(OscResponse::eError);

            }
        }
    }
    debugWarning("Unhandled OSC message!\n");
    // default action: don't change response
    return OscResponse(OscResponse::eUnhandled);
}*/

OscResponse
GenericMixer::selectorListChildren(int id) {
    OscMessage m;

    if (id != -1) return m;

    debugOutput(DEBUG_LEVEL_NORMAL,"Listing selector elements...\n");
    
    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if (((*it)->getType()) == FunctionBlockCmd::eFBT_Selector) {
            ostrm << (int)((*it)->getId());
            m.addArgument(ostrm.str());
        }
    }
    return OscResponse(m);
}

OscResponse
GenericMixer::selectorListParams(int id) {
    OscMessage m;

    if (id == -1) return m;

    debugOutput(DEBUG_LEVEL_NORMAL,"Listing selector params...\n");
    
    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if ((((*it)->getType()) == FunctionBlockCmd::eFBT_Selector)
            && (((*it)->getId()) == id))
        {
            m.addArgument("name");
            m.addArgument("value");
        }
    }
    return OscResponse(m);
}

OscResponse
GenericMixer::selectorGetParam(int id, std::string p, OscMessage *src) {
    OscMessage m;

    if (id == -1) return m;

    debugOutput(DEBUG_LEVEL_NORMAL,"Getting selector parameter: '%s'\n", p.c_str());
    
    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if ((((*it)->getType()) == FunctionBlockCmd::eFBT_Selector)
            && (((*it)->getId()) == id))
        {
            if (p=="name") {
                m.addArgument("testname");
            } else if (p=="value") {
                int value;
                if( getSelectorValue(id, value) ) {
                    m.addArgument(value);
                }
            }
        }
    }
    return OscResponse(m);
}

OscResponse
GenericMixer::selectorSetParam(int id, std::string p, OscMessage *src) {
    OscMessage m;

    if (id == -1) return m;

    debugOutput(DEBUG_LEVEL_NORMAL,"Setting selector parameter: '%s'\n", p.c_str());
    
    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if ((((*it)->getType()) == FunctionBlockCmd::eFBT_Selector)
            && (((*it)->getId()) == id))
        {
            if (p=="name") {

            } else if (p=="value") {
                assert(src->nbArguments()>=3);
                int value=src->getArgument(2).toInt();
                if( ! setSelectorValue(id, value) ) {
                    return OscResponse(OscResponse::eError);
                }
            }
        }
    }
    return OscResponse(m);
}

bool
GenericMixer::getSelectorValue(int fb_id, int &value) {
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
        return false;
    }

    if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }
    
    value=(int)(fbCmd.m_pFBSelector->m_inputFbPlugNumber);
    return (fbCmd.getResponse() == AVCCommand::eR_Implemented);
}

bool
GenericMixer::setSelectorValue(int fb_id, int value) {
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
        return false;
    }

    if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    return (fbCmd.getResponse() == AVCCommand::eR_Accepted);
}


//// FEATURE
OscResponse 
GenericMixer::processOscMessageFeature(std::string path, OscMessage *m) {
    debugOutput( DEBUG_LEVEL_NORMAL, "Feature: %s\n", path.c_str());
    // delete leading slashes
    if(path.find_first_of('/')==0) path=path.substr(1,path.size());

    // delete trailing slashes
    if(path.find_last_of('/')==path.size()-1) path=path.substr(0,path.size()-1);
    
    int id=-1;
    if ((path != "") && (!from_string<int>(id, path, std::dec))) {
        debugWarning( "could not get id\n" );
        return OscResponse(OscResponse::eError);
    }
    
    unsigned int nbArgs=m->nbArguments();
    if (nbArgs>=1) {
        OscArgument arg0=m->getArgument(0);
        if(arg0.isString()) { // first argument should be a string command
            string cmd=arg0.getString();
            debugOutput( DEBUG_LEVEL_NORMAL, "(%p) CMD: %s\n", this, cmd.c_str());
            
            if(cmd == "list") {
                debugOutput( DEBUG_LEVEL_NORMAL, "Listing feature elements...\n");
                return featureListChildren(id);
            }
            if(cmd == "params") {
                debugOutput( DEBUG_LEVEL_NORMAL, "Listing feature %d params...\n", id);
                return featureListParams(id);
            }
            if(cmd == "get") {
                if (nbArgs<2 || !m->getArgument(1).isString()) {
                    debugWarning("Argument error\n");
                    m->print();
                    return OscResponse(OscResponse::eError);
                }
                debugOutput( DEBUG_LEVEL_NORMAL, "Getting feature param %s...\n",
                    m->getArgument(1).getString().c_str());
                return featureGetParam(id,m->getArgument(1).getString(), m);
            }
            if(cmd == "set") {
                if (nbArgs<3 || !m->getArgument(1).isString()) {
                    debugWarning("Argument error\n");
                    m->print();
                    return OscResponse(OscResponse::eError);
                }
                debugOutput( DEBUG_LEVEL_NORMAL, "Setting feature param %s...\n",
                    m->getArgument(1).getString().c_str());
                return featureSetParam(id,m->getArgument(1).getString(), m);
            }
            return OscResponse(OscResponse::eUnhandled);
        } else {
            debugWarning("Wrong command datatype\n");
            return OscResponse(OscResponse::eError);
        }
    } else {
        debugWarning("Not enough arguments\n");
        return OscResponse(OscResponse::eError);
    }

}

OscResponse
GenericMixer::featureListChildren(int id) {
    OscMessage m;

    if (id != -1) return m;

    debugOutput(DEBUG_LEVEL_NORMAL,"Listing feature elements...\n");
    
    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if (((*it)->getType()) == FunctionBlockCmd::eFBT_Feature) {
            ostrm << (int)((*it)->getId());
            m.addArgument(ostrm.str());
        }
    }
    return OscResponse(m);
}

OscResponse
GenericMixer::featureListParams(int id) {
    OscMessage m;

    if (id == -1) return m;

    debugOutput(DEBUG_LEVEL_NORMAL,"Listing feature params...\n");
    
    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if ((((*it)->getType()) == FunctionBlockCmd::eFBT_Feature)
            && (((*it)->getId()) == id))
        {
            m.addArgument("name");
            m.addArgument("volume");
        }
    }
    return OscResponse(m);
}

OscResponse
GenericMixer::featureGetParam(int id, std::string p, OscMessage *src) {
    OscMessage m;

    if (id == -1) return m;
    
    debugOutput(DEBUG_LEVEL_NORMAL,"Getting feature parameter: '%s'\n", p.c_str());
    
    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if ((((*it)->getType()) == FunctionBlockCmd::eFBT_Feature)
            && (((*it)->getId()) == id))
        {
            if (p=="name") {
                m.addArgument("testname");
            } else if (p=="volume") {
                if (src->nbArguments()<3) return OscResponse(OscResponse::eError);
                int value;
                int channel=src->getArgument(2).toInt();
               
                if( getFeatureVolumeValue(id, channel, value) ) {
                    m.addArgument(value);
                }
            }
        }
    }
    return OscResponse(m);
}

OscResponse
GenericMixer::featureSetParam(int id, std::string p, OscMessage *src) {
    OscMessage m;

    if (id == -1) return m;

    debugOutput(DEBUG_LEVEL_NORMAL,"Setting feature parameter: '%s'\n", p.c_str());
    
    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if ((((*it)->getType()) == FunctionBlockCmd::eFBT_Feature)
            && (((*it)->getId()) == id))
        {
            if (p=="name") {
                m.addArgument("testname");
            } else if (p=="volume") {
                if (src->nbArguments()<4) return OscResponse(OscResponse::eError);
                int channel=src->getArgument(2).toInt();
                int value=src->getArgument(3).toInt();;
               
                if( !setFeatureVolumeValue(id, channel, value) ) {
                    return OscResponse(OscResponse::eError);
                }
            }
        }
    }
    return OscResponse(m);
}


bool 
GenericMixer::getFeatureVolumeValue(int fb_id, int channel, int &volume) {
    OscMessage m;
    
    debugOutput(DEBUG_LEVEL_NORMAL,"Get feature volume %d channel %d...\n",
        fb_id, channel);

    FunctionBlockCmd fbCmd( m_p1394Service,
                            FunctionBlockCmd::eFBT_Feature,
                            fb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( m_device.getNodeId()  );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.m_pFBFeature->m_audioChannelNumber=channel;
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_Volume;
    fbCmd.m_pFBFeature->m_pVolume->m_volume=0;

    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
        return false;
    }
    
    if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    volume=(int)(fbCmd.m_pFBFeature->m_pVolume->m_volume);
    
    return (fbCmd.getResponse() == AVCCommand::eR_Implemented);
}

bool 
GenericMixer::setFeatureVolumeValue(int fb_id, int channel, 
                                        int volume) {
    debugOutput(DEBUG_LEVEL_NORMAL,"Set feature volume %d channel %d to %d...\n",
        fb_id, channel, volume);

    FunctionBlockCmd fbCmd( m_p1394Service,
                            FunctionBlockCmd::eFBT_Feature,
                            fb_id,
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( m_device.getNodeId()  );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Control );
    fbCmd.m_pFBFeature->m_audioChannelNumber=channel;
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_Volume;
    fbCmd.m_pFBFeature->m_pVolume->m_volume=volume;

    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
        return false;
    }

    if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
        CoutSerializer se;
        fbCmd.serialize( se );
    }

    return (fbCmd.getResponse() == AVCCommand::eR_Accepted);
}

//// PROCESSING

OscResponse 
GenericMixer::processOscMessageProcessing(std::string path, OscMessage *m) {
    debugOutput( DEBUG_LEVEL_NORMAL, "Processing: %s\n", path.c_str());
    return OscResponse(OscResponse::eUnhandled);
}

OscResponse
GenericMixer::processingListChildren(int id) {
    OscMessage m;
    
    if (id != -1) return m;
    
    debugOutput(DEBUG_LEVEL_NORMAL,"Listing processing elements...\n");

    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if (((*it)->getType()) == FunctionBlockCmd::eFBT_Processing) {
            ostrm << (int)((*it)->getId());
            m.addArgument(ostrm.str());
        }
    }
    return OscResponse(m);
}

//// CODEC

OscResponse
GenericMixer::codecListChildren(int id) {
    OscMessage m;

    if (id != -1) return m;
    
    debugOutput(DEBUG_LEVEL_NORMAL,"Listing codec elements...\n");

    assert(m_device.getAudioSubunit(0)); // FIXME!!
    FunctionBlockVector functions=m_device.getAudioSubunit(0)->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        std::ostringstream ostrm;
        if (((*it)->getType()) == FunctionBlockCmd::eFBT_Codec) {
            ostrm << (int)((*it)->getId());
            m.addArgument(ostrm.str());
        }
    }
    return OscResponse(m);
}

} // end of namespace BeBoB
