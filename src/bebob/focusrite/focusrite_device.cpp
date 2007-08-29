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

#include "focusrite_device.h"
#include "focusrite_cmd.h"

namespace BeBoB {
namespace Focusrite {

SaffireProDevice::SaffireProDevice( Ieee1394Service& ieee1394Service,
                            std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::AvDevice( ieee1394Service, configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Focusrite::SaffireProDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );

    // create control objects for the saffire pro
    m_Phantom1 = new PhantomPowerControl(*this, FOCUSRITE_CMD_ID_PHANTOM14);
    if (m_Phantom1) addElement(m_Phantom1);
    
    m_Phantom2 = new PhantomPowerControl(*this, FOCUSRITE_CMD_ID_PHANTOM58);
    if (m_Phantom2) addElement(m_Phantom2);
    
}

SaffireProDevice::~SaffireProDevice()
{
    // remove and delete control elements
    deleteElement(m_Phantom1);
    if (m_Phantom1) delete m_Phantom1;
    
    deleteElement(m_Phantom2);
    if (m_Phantom2) delete m_Phantom2;
}

void
SaffireProDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::Focusrite::SaffireProDevice\n");
    BeBoB::AvDevice::showDevice();
}

void
SaffireProDevice::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );

    if (m_Phantom1) m_Phantom2->setVerboseLevel(l);
    if (m_Phantom2) m_Phantom2->setVerboseLevel(l);

    BeBoB::AvDevice::setVerboseLevel(l);
}

int
SaffireProDevice::getSamplingFrequency( ) {
    FocusriteVendorDependentCmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Status );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setVerbose( getDebugLevel() );
    cmd.setVerbose( DEBUG_LEVEL_VERY_VERBOSE );
    cmd.m_id=FOCUSRITE_CMD_ID_SAMPLERATE;

    if ( !cmd.fire() ) {
        debugError( "FocusriteVendorDependentCmd info command failed\n" );
        return 0;
    }
    
    debugOutput( DEBUG_LEVEL_NORMAL,
                     "getSampleRate: %d\n", cmd.m_value );

    switch(cmd.m_value) {
        case FOCUSRITE_CMD_SAMPLERATE_44K1:  return 44100;
        case FOCUSRITE_CMD_SAMPLERATE_48K:   return 48000;
        case FOCUSRITE_CMD_SAMPLERATE_88K2:  return 88200;
        case FOCUSRITE_CMD_SAMPLERATE_96K:   return 96000;
        case FOCUSRITE_CMD_SAMPLERATE_176K4: return 176400;
        case FOCUSRITE_CMD_SAMPLERATE_192K:  return 192000;
        default: return 0;
    }
}

bool
SaffireProDevice::setSamplingFrequency( int s )
{
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if(snoopMode) {
        int current_sr=getSamplingFrequency();
        if (current_sr != s ) {
            debugError("In snoop mode it is impossible to set the sample rate.\n");
            debugError("Please start the client with the correct setting.\n");
            return false;
        }
        return true;
    } else {

        FocusriteVendorDependentCmd cmd( get1394Service() );
        cmd.setCommandType( AVC::AVCCommand::eCT_Status );
        cmd.setNodeId( getConfigRom().getNodeId() );
        cmd.setVerbose( getDebugLevel() );
        cmd.setVerbose( DEBUG_LEVEL_VERY_VERBOSE );
        cmd.m_id=FOCUSRITE_CMD_ID_SAMPLERATE;

        switch(s) {
            case 44100:  cmd.m_value=FOCUSRITE_CMD_SAMPLERATE_44K1;break;
            case 48000:  cmd.m_value=FOCUSRITE_CMD_SAMPLERATE_48K;break;
            case 88200:  cmd.m_value=FOCUSRITE_CMD_SAMPLERATE_88K2;break;
            case 96000:  cmd.m_value=FOCUSRITE_CMD_SAMPLERATE_96K;break;
            case 176400: cmd.m_value=FOCUSRITE_CMD_SAMPLERATE_176K4;break;
            case 192000: cmd.m_value=FOCUSRITE_CMD_SAMPLERATE_192K;break;
            default:
                debugWarning("Unsupported samplerate: %d\n",s);
                return false;
        }

    
        if ( !cmd.fire() ) {
            debugError( "FocusriteVendorDependentCmd info command failed\n" );
            return false;
        }

        debugOutput( DEBUG_LEVEL_NORMAL,
                     "setSampleRate: Set sample rate to %d\n", s );
        return true;
    }
    // not executable
    return false;

}


// --- element implementation classes

PhantomPowerControl::PhantomPowerControl(SaffireProDevice& parent, int channel)
: Control::Discrete()
, m_Parent(parent)
, m_channel ( channel )
{
    std::ostringstream ostrm;
    ostrm << "PhantomPower_";
    if(channel==FOCUSRITE_CMD_ID_PHANTOM14) ostrm << "1to4";
    if(channel==FOCUSRITE_CMD_ID_PHANTOM58) ostrm << "5to8";
    Control::Discrete::setName(ostrm.str());
    
    ostrm.str("");
    ostrm << "Phantom ";
    if(channel==FOCUSRITE_CMD_ID_PHANTOM14) ostrm << "1-4";
    if(channel==FOCUSRITE_CMD_ID_PHANTOM58) ostrm << "5-8";
    setLabel(ostrm.str());
    
    ostrm.str("");
    ostrm << "Turn on/off Phantom Power on channels ";
    if(channel==FOCUSRITE_CMD_ID_PHANTOM14) ostrm << "1 to 4";
    if(channel==FOCUSRITE_CMD_ID_PHANTOM58) ostrm << "5 to 8";
    setDescription(ostrm.str());
}

bool
PhantomPowerControl::setValue(int v)
{
    if (v != 0 && v != 1) return false;
    
    FocusriteVendorDependentCmd cmd( m_Parent.get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    cmd.setNodeId( m_Parent.getConfigRom().getNodeId() );
    cmd.setVerbose( getDebugLevel() );
    cmd.m_id=m_channel;
    cmd.m_value=v;
    
    if ( !cmd.fire() ) {
        debugError( "FocusriteVendorDependentCmd info command failed\n" );
        // shouldn't this be an error situation?
        return false;
    }
    return true;
}

int
PhantomPowerControl::getValue()
{
    FocusriteVendorDependentCmd cmd( m_Parent.get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Status );
    cmd.setNodeId( m_Parent.getConfigRom().getNodeId() );
    cmd.setVerbose( getDebugLevel() );
    cmd.m_id=m_channel;
    cmd.m_value=0;

    if ( !cmd.fire() ) {
        debugError( "FocusriteVendorDependentCmd info command failed\n" );
        // shouldn't this be an error situation?
        return 0;
    }

    return cmd.m_value;
}


} // Focusrite
} // BeBoB
