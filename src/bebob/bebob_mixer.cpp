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

#include "bebob/bebob_mixer.h"

#include "bebob/bebob_avdevice.h"
#include "bebob/bebob_avdevice_subunit.h"

#include "libavc/audiosubunit/avc_function_block.h"
#include "libutil/cmd_serialize.h"

#include "libcontrol/BasicElements.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/configrom.h"

#include <string>
#include <sstream>
#include <iostream>

#include <assert.h>

using namespace AVC;

namespace BeBoB {

template <class T>
bool from_string(T& t, 
                 const std::string& s, 
                 std::ios_base& (*f)(std::ios_base&))
{
  std::istringstream iss(s);
  return !(iss >> f >> t).fail();
}

IMPL_DEBUG_MODULE( Mixer, Mixer, DEBUG_LEVEL_NORMAL );

Mixer::Mixer(AvDevice &d)
    : Control::Container()
    , m_device(d)
{
    addElementForAllFunctionBlocks();
    if (!d.addElement(this)) {
        debugWarning("Could not add myself to Control::Container\n");
    }
}

Mixer::~Mixer() {
    
    debugOutput(DEBUG_LEVEL_NORMAL,"Unregistering from Control::Container...\n");
    if (!m_device.deleteElement(this)) {
        debugWarning("Could not delete myself from Control::Container\n");
    }
    
    // delete all our elements since we should have created
    // them ourselves.
    for ( Control::ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        debugOutput(DEBUG_LEVEL_NORMAL,"deleting %s...\n", (*it)->getName().c_str());
        delete *it;
    }
}

bool
Mixer::deleteElement(Element *e)
{
    if (Control::Container::deleteElement(e)) {
        delete e;
        return true;
    } else return false;
}

bool
Mixer::clearElements() {
    // delete all our elements since we should have created
    // them ourselves.
    for ( Control::ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        delete *it;
    }
    m_Children.clear();
    
    return true;
}

bool
Mixer::addElementForFunctionBlock(FunctionBlock& b) {
    bool retval=true;
    
    std::ostringstream ostrm;
    ostrm << b.getName() << "_" << (int)(b.getId());
    
    debugOutput(DEBUG_LEVEL_NORMAL,"Adding element for functionblock %s...\n",
        ostrm.str().c_str());

    Control::Element *e=NULL;
    
    if (dynamic_cast<FunctionBlockSelector *>(&b) != NULL) {
        FunctionBlockSelector *bf=dynamic_cast<FunctionBlockSelector *>(&b);
        debugOutput( DEBUG_LEVEL_VERBOSE, "FB is a SelectorFunctionBlock\n");
        e=new MixerFBSelector(*this, *bf);
        if (e) {
            e->setVerboseLevel(getDebugLevel());
            retval &= Control::Container::addElement(e);
        } else {
            debugError("Control element creation failed\n");
            retval &= false;
        }
    }
    if (dynamic_cast<FunctionBlockFeature *>(&b) != NULL) {
        FunctionBlockFeature *bf=dynamic_cast<FunctionBlockFeature *>(&b);
        debugOutput( DEBUG_LEVEL_VERBOSE, "FB is a FeatureFunctionBlock\n");
        e=new MixerFBFeature(*this, *bf);
        if (e) {
            e->setVerboseLevel(getDebugLevel());
            retval &= Control::Container::addElement(e);
        } else {
            debugError("Control element creation failed\n");
            retval &= false;
        }
    }
    
    if (!e) {
        debugError("No control element created\n");
        return false;
    }

    return retval;
}

bool
Mixer::addElementForAllFunctionBlocks() {
    debugOutput(DEBUG_LEVEL_NORMAL,"Adding elements for functionblocks...\n");

    BeBoB::SubunitAudio *asu =
        dynamic_cast<BeBoB::SubunitAudio *>(m_device.getAudioSubunit(0));

    if(asu == NULL) {
        debugWarning("No BeBoB audio subunit found\n");
        return false;
    }
    FunctionBlockVector functions=asu->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        if (!addElementForFunctionBlock(*(*it))) {
            std::ostringstream ostrm;
            ostrm << (*it)->getName() << " " << (int)((*it)->getId());
            
            debugWarning("Failed to add element for function block %s\n",
                ostrm.str().c_str());
        };
    }
    return true;
}

// --- element implementation classes

MixerFBFeature::MixerFBFeature(Mixer& parent, FunctionBlockFeature& s)
: Control::Continuous()
, m_Parent(parent) 
, m_Slave(s)
{
    std::ostringstream ostrm;
    ostrm << s.getName() << "_" << (int)(s.getId());
    
    Control::Continuous::setName(ostrm.str());
    
    ostrm.str("");
    ostrm << "Label for " << s.getName() << " " << (int)(s.getId());
    setLabel(ostrm.str());
    
    ostrm.str("");
    ostrm << "Description for " << s.getName() << " " << (int)(s.getId());
    setDescription(ostrm.str());
}

bool
MixerFBFeature::setValue(double v)
{
    int volume=(int)v;
    debugOutput(DEBUG_LEVEL_NORMAL,"Set feature volume %d to %d...\n",
        m_Slave.getId(), volume);

    FunctionBlockCmd fbCmd( m_Parent.getParent().get1394Service(),
                            FunctionBlockCmd::eFBT_Feature,
                            m_Slave.getId(),
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( m_Parent.getParent().getNodeId() );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Control );
    fbCmd.m_pFBFeature->m_audioChannelNumber=0; // m_channel
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_Volume;
    fbCmd.m_pFBFeature->m_pVolume->m_volume=volume;
    fbCmd.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );

    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
        return false;
    }

//     if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
//         Util::CoutSerializer se;
//         fbCmd.serialize( se );
//     }
    
    if((fbCmd.getResponse() != AVCCommand::eR_Accepted)) {
        debugWarning("fbCmd.getResponse() != AVCCommand::eR_Accepted\n");
    }

    return (fbCmd.getResponse() == AVCCommand::eR_Accepted);
}

double
MixerFBFeature::getValue()
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get feature volume %d...\n",
        m_Slave.getId());

    FunctionBlockCmd fbCmd( m_Parent.getParent().get1394Service(),
                            FunctionBlockCmd::eFBT_Feature,
                            m_Slave.getId(),
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( m_Parent.getParent().getNodeId()  );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.m_pFBFeature->m_audioChannelNumber=0;
    fbCmd.m_pFBFeature->m_controlSelector=FunctionBlockFeature::eCSE_Feature_Volume; // FIXME
    fbCmd.m_pFBFeature->m_pVolume->m_volume=0;
    fbCmd.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );

    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
        return 0;
    }
    
//     if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
//         Util::CoutSerializer se;
//         fbCmd.serialize( se );
//     }

    if((fbCmd.getResponse() != AVCCommand::eR_Implemented)) {
        debugWarning("fbCmd.getResponse() != AVCCommand::eR_Implemented\n");
    }
    
    int16_t volume=(int16_t)(fbCmd.m_pFBFeature->m_pVolume->m_volume);
    
    return volume;
}

// --- element implementation classes

MixerFBSelector::MixerFBSelector(Mixer& parent, FunctionBlockSelector& s)
: Control::Discrete()
, m_Parent(parent) 
, m_Slave(s)
{
    std::ostringstream ostrm;
    ostrm << s.getName() << "_" << (int)(s.getId());
    
    Control::Discrete::setName(ostrm.str());
    
    ostrm.str("");
    ostrm << "Label for " << s.getName() << " " << (int)(s.getId());
    setLabel(ostrm.str());
    
    ostrm.str("");
    ostrm << "Description for " << s.getName() << " " << (int)(s.getId());
    setDescription(ostrm.str());
}

bool
MixerFBSelector::setValue(int v)
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Set selector %d to %d...\n",
        m_Slave.getId(), v);

    FunctionBlockCmd fbCmd( m_Parent.getParent().get1394Service(),
                            FunctionBlockCmd::eFBT_Selector,
                            m_Slave.getId(),
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( m_Parent.getParent().getNodeId() );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Control );
    fbCmd.m_pFBSelector->m_inputFbPlugNumber=(v & 0xFF);
    fbCmd.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );

    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
        return false;
    }

//     if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
//         Util::CoutSerializer se;
//         fbCmd.serialize( se );
//     }
//     
    if((fbCmd.getResponse() != AVCCommand::eR_Accepted)) {
        debugWarning("fbCmd.getResponse() != AVCCommand::eR_Accepted\n");
    }

    return (fbCmd.getResponse() == AVCCommand::eR_Accepted);
}

int
MixerFBSelector::getValue()
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get selector %d...\n",
        m_Slave.getId());

    FunctionBlockCmd fbCmd( m_Parent.getParent().get1394Service(),
                            FunctionBlockCmd::eFBT_Selector,
                            m_Slave.getId(),
                            FunctionBlockCmd::eCA_Current );
    fbCmd.setNodeId( m_Parent.getParent().getNodeId()  );
    fbCmd.setSubunitId( 0x00 );
    fbCmd.setCommandType( AVCCommand::eCT_Status );
    fbCmd.m_pFBSelector->m_inputFbPlugNumber=0;
    fbCmd.setVerboseLevel( DEBUG_LEVEL_VERY_VERBOSE );

    if ( !fbCmd.fire() ) {
        debugError( "cmd failed\n" );
        return 0;
    }
    
//     if ( getDebugLevel() >= DEBUG_LEVEL_NORMAL ) {
//         Util::CoutSerializer se;
//         fbCmd.serialize( se );
//     }

    if((fbCmd.getResponse() != AVCCommand::eR_Implemented)) {
        debugWarning("fbCmd.getResponse() != AVCCommand::eR_Implemented\n");
    }
    
    return fbCmd.m_pFBSelector->m_inputFbPlugNumber;
}

} // end of namespace BeBoB
