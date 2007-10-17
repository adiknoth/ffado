/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "focusrite_saffirepro.h"
#include "focusrite_cmd.h"

namespace BeBoB {
namespace Focusrite {

FocusriteDevice::FocusriteDevice( Ieee1394Service& ieee1394Service,
                            std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::AvDevice( ieee1394Service, configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Focusrite::FocusriteDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );

}

void
FocusriteDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::Focusrite::FocusriteDevice\n");
    BeBoB::AvDevice::showDevice();
}

void
FocusriteDevice::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );

    BeBoB::AvDevice::setVerboseLevel(l);
}

bool
FocusriteDevice::setSpecificValue(uint32_t id, uint32_t v)
{
    
    FocusriteVendorDependentCmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setSubunitType( AVC::eST_Unit  );
    cmd.setSubunitId( 0xff );
    
    cmd.setVerbose( getDebugLevel() );
//         cmd.setVerbose( DEBUG_LEVEL_VERY_VERBOSE );
    
    cmd.m_id=id;
    cmd.m_value=v;
    
    if ( !cmd.fire() ) {
        debugError( "FocusriteVendorDependentCmd info command failed\n" );
        return false;
    }
    
    return true;
}

bool
FocusriteDevice::getSpecificValue(uint32_t id, uint32_t *v)
{
    
    FocusriteVendorDependentCmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Status );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setSubunitType( AVC::eST_Unit  );
    cmd.setSubunitId( 0xff );
    
    cmd.setVerbose( getDebugLevel() );
//         cmd.setVerbose( DEBUG_LEVEL_VERY_VERBOSE );
    
    cmd.m_id=id;
    
    if ( !cmd.fire() ) {
        debugError( "FocusriteVendorDependentCmd info command failed\n" );
        return false;
    }
    
    *v=cmd.m_value;

    return true;
}

int
FocusriteDevice::convertDefToSr( uint32_t def ) {
    switch(def) {
        case FOCUSRITE_CMD_SAMPLERATE_44K1:  return 44100;
        case FOCUSRITE_CMD_SAMPLERATE_48K:   return 48000;
        case FOCUSRITE_CMD_SAMPLERATE_88K2:  return 88200;
        case FOCUSRITE_CMD_SAMPLERATE_96K:   return 96000;
        case FOCUSRITE_CMD_SAMPLERATE_176K4: return 176400;
        case FOCUSRITE_CMD_SAMPLERATE_192K:  return 192000;
        default: return 0;
    }
}

uint32_t
FocusriteDevice::convertSrToDef( int sr ) {
    switch(sr) {
        case 44100:  return FOCUSRITE_CMD_SAMPLERATE_44K1;
        case 48000:  return FOCUSRITE_CMD_SAMPLERATE_48K;
        case 88200:  return FOCUSRITE_CMD_SAMPLERATE_88K2;
        case 96000:  return FOCUSRITE_CMD_SAMPLERATE_96K;
        case 176400: return FOCUSRITE_CMD_SAMPLERATE_176K4;
        case 192000: return FOCUSRITE_CMD_SAMPLERATE_192K;
        default:
            debugWarning("Unsupported samplerate: %d\n", sr);
            return 0;
    }
}

// --- element implementation classes

BinaryControl::BinaryControl(FocusriteDevice& parent, int id)
: Control::Discrete()
, m_Parent(parent)
, m_cmd_id ( id )
{}
BinaryControl::BinaryControl(FocusriteDevice& parent, int id,
                std::string name, std::string label, std::string descr)
: Control::Discrete()
, m_Parent(parent)
, m_cmd_id ( id )
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

bool
BinaryControl::setValue(int v)
{
    uint32_t val=0;
    if (v) val=1;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for id %d to %d\n", 
                                     m_cmd_id, v);

    if ( !m_Parent.setSpecificValue(m_cmd_id, val) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    } else return true;
}

int
BinaryControl::getValue()
{
    uint32_t val=0;

    if ( !m_Parent.getSpecificValue(m_cmd_id, &val) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for %d = %d\n", 
                                         m_cmd_id, val);
        return val;
    }
}

// --- element implementation classes

VolumeControl::VolumeControl(FocusriteDevice& parent, int id)
: Control::Discrete()
, m_Parent(parent)
, m_cmd_id ( id )
{}
VolumeControl::VolumeControl(FocusriteDevice& parent, int id,
                std::string name, std::string label, std::string descr)
: Control::Discrete()
, m_Parent(parent)
, m_cmd_id ( id )
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}


bool
VolumeControl::setValue(int v)
{
    if (v>0x07FFF) v=0x07FFF;
    else if (v<0) v=0;

    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for id %d to %d\n", 
                                     m_cmd_id, v);

    if ( !m_Parent.setSpecificValue(m_cmd_id, v) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    } else return true;
}

int
VolumeControl::getValue()
{
    uint32_t val=0;

    if ( !m_Parent.getSpecificValue(m_cmd_id, &val) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for %d = %d\n", 
                                         m_cmd_id, val);
        return val;
    }
}

} // Focusrite
} // BeBoB
