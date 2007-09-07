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

    // the saffire pro doesn't seem to like it if the commands are too fast
    if (AVC::AVCCommand::getSleepAfterAVCCommand() < 200) {
        AVC::AVCCommand::setSleepAfterAVCCommand( 200 );
    }

    // create control objects for the saffire pro
    m_Phantom1 = new BinaryControl(*this, FOCUSRITE_CMD_ID_PHANTOM14,
                 "Phantom_1to4", "Phantom 1-4", "Switch Phantom Power on channels 1-4");
    if (m_Phantom1) addElement(m_Phantom1);
    
    m_Phantom2 = new BinaryControl(*this, FOCUSRITE_CMD_ID_PHANTOM58,
                 "Phantom_5to8", "Phantom 5-8", "Switch Phantom Power on channels 5-8");
    if (m_Phantom2) addElement(m_Phantom2);
    
    m_Insert1 = new BinaryControl(*this, FOCUSRITE_CMD_ID_INSERT1,
                "Insert1", "Insert 1", "Switch Insert on Channel 1");
    if (m_Insert1) addElement(m_Insert1);
    
    m_Insert2 = new BinaryControl(*this, FOCUSRITE_CMD_ID_INSERT2,
                "Insert2", "Insert 2", "Switch Insert on Channel 2");
    if (m_Insert2) addElement(m_Insert2);
    
    m_AC3pass = new BinaryControl(*this, FOCUSRITE_CMD_ID_AC3_PASSTHROUGH,
                "AC3pass", "AC3 Passtrough", "Enable AC3 Passthrough");
    if (m_AC3pass) addElement(m_AC3pass);
    
    m_MidiTru = new BinaryControl(*this, FOCUSRITE_CMD_ID_MIDI_TRU,
                "MidiTru", "Midi Tru", "Enable Midi Tru");
    if (m_MidiTru) addElement(m_MidiTru);

    // output mix controls
    //  OUT12
    m_Output12[0] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC1_TO_OUT1,
                "PC1_OUT1", "PC1_OUT1", "Volume from PC Channel 1 to Output Channel 1");
    if (m_Output12[0]) addElement(m_Output12[0]);
    m_Output12[1] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC2_TO_OUT2,
                "PC2_OUT2", "PC2_OUT2", "Volume from PC Channel 2 to Output Channel 2");
    if (m_Output12[1]) addElement(m_Output12[1]);
    m_Output12[2] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX1_TO_OUT1,
                "MIX_OUT1", "MIX_OUT1", "Volume from Input Mix Left to Output Channel 1");
    if (m_Output12[2]) addElement(m_Output12[2]);
    m_Output12[3] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX2_TO_OUT2,
                "MIX_OUT2", "MIX_OUT2", "Volume from Input Mix Right to Output Channel 2");
    if (m_Output12[3]) addElement(m_Output12[3]);

    //  OUT34
    m_Output34[0] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC1_TO_OUT3,
                "PC1_OUT3", "PC1_OUT3", "Volume from PC Channel 1 to Output Channel 3");
    if (m_Output34[0]) addElement(m_Output34[0]);
    m_Output34[1] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC2_TO_OUT4,
                "PC2_OUT4", "PC2_OUT4", "Volume from PC Channel 2 to Output Channel 4");
    if (m_Output34[1]) addElement(m_Output34[1]);
    m_Output34[2] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC3_TO_OUT3,
                "PC3_OUT3", "PC3_OUT3", "Volume from PC Channel 3 to Output Channel 3");
    if (m_Output34[2]) addElement(m_Output34[2]);
    m_Output34[3] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC4_TO_OUT4,
                "PC4_OUT4", "PC4_OUT4", "Volume from PC Channel 4 to Output Channel 4");
    if (m_Output34[3]) addElement(m_Output34[3]);
    m_Output34[4] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX1_TO_OUT3,
                "MIX_OUT3", "MIX_OUT3", "Volume from Input Mix Left to Output Channel 3");
    if (m_Output34[4]) addElement(m_Output34[4]);
    m_Output34[5] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX2_TO_OUT4,
                "MIX_OUT4", "MIX_OUT4", "Volume from Input Mix Right to Output Channel 4");
    if (m_Output34[5]) addElement(m_Output34[5]);
    
    //  OUT56
    m_Output56[0] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC1_TO_OUT5,
                "PC1_OUT5", "PC1_OUT5", "Volume from PC Channel 1 to Output Channel 5");
    if (m_Output56[0]) addElement(m_Output56[0]);
    m_Output56[1] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC2_TO_OUT6,
                "PC2_OUT6", "PC2_OUT6", "Volume from PC Channel 2 to Output Channel 6");
    if (m_Output56[1]) addElement(m_Output56[1]);
    m_Output56[2] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC5_TO_OUT5,
                "PC5_OUT5", "PC5_OUT5", "Volume from PC Channel 5 to Output Channel 5");
    if (m_Output56[2]) addElement(m_Output56[2]);
    m_Output56[3] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC6_TO_OUT6,
                "PC6_OUT6", "PC6_OUT6", "Volume from PC Channel 6 to Output Channel 6");
    if (m_Output56[3]) addElement(m_Output56[3]);
    m_Output56[4] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX1_TO_OUT5,
                "MIX_OUT5", "MIX_OUT5", "Volume from Input Mix Left to Output Channel 5");
    if (m_Output56[4]) addElement(m_Output56[4]);
    m_Output56[5] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX2_TO_OUT6,
                "MIX_OUT6", "MIX_OUT6", "Volume from Input Mix Right to Output Channel 6");
    if (m_Output56[5]) addElement(m_Output56[5]);

    //  OUT78
    m_Output78[0] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC1_TO_OUT7,
                "PC1_OUT7", "PC1_OUT7", "Volume from PC Channel 1 to Output Channel 7");
    if (m_Output78[0]) addElement(m_Output78[0]);
    m_Output78[1] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC2_TO_OUT8,
                "PC2_OUT8", "PC2_OUT8", "Volume from PC Channel 2 to Output Channel 8");
    if (m_Output78[1]) addElement(m_Output78[1]);
    m_Output78[2] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC7_TO_OUT7,
                "PC7_OUT7", "PC7_OUT7", "Volume from PC Channel 7 to Output Channel 7");
    if (m_Output78[2]) addElement(m_Output78[2]);
    m_Output78[3] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC8_TO_OUT8,
                "PC8_OUT8", "PC8_OUT8", "Volume from PC Channel 8 to Output Channel 8");
    if (m_Output78[3]) addElement(m_Output78[3]);
    m_Output78[4] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX1_TO_OUT7,
                "MIX_OUT7", "MIX_OUT7", "Volume from Input Mix Left to Output Channel 7");
    if (m_Output78[4]) addElement(m_Output78[4]);
    m_Output78[5] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX2_TO_OUT8,
                "MIX_OUT8", "MIX_OUT8", "Volume from Input Mix Right to Output Channel 8");
    if (m_Output78[5]) addElement(m_Output78[5]);

    //  OUT910
    m_Output910[0] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC1_TO_OUT9,
                "PC1_OUT9", "PC1_OUT9", "Volume from PC Channel 1 to Output Channel 9");
    if (m_Output910[0]) addElement(m_Output910[0]);
    m_Output910[1] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC2_TO_OUT10,
                "PC2_OUT10", "PC2_OUT10", "Volume from PC Channel 2 to Output Channel 10");
    if (m_Output910[1]) addElement(m_Output910[1]);
    m_Output910[2] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC9_TO_OUT9,
                "PC9_OUT9", "PC9_OUT9", "Volume from PC Channel 9 to Output Channel 9");
    if (m_Output910[2]) addElement(m_Output910[2]);
    m_Output910[3] = new VolumeControl(*this, FOCUSRITE_CMD_ID_PC10_TO_OUT10,
                "PC10_OUT10", "PC10_OUT10", "Volume from PC Channel 10 to Output Channel 10");
    if (m_Output910[3]) addElement(m_Output910[3]);
    m_Output910[4] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX1_TO_OUT9,
                "MIX_OUT9", "MIX_OUT9", "Volume from Input Mix Left to Output Channel 9");
    if (m_Output910[4]) addElement(m_Output910[4]);
    m_Output910[5] = new VolumeControl(*this, FOCUSRITE_CMD_ID_MIX2_TO_OUT10,
                "MIX_OUT10", "MIX_OUT10", "Volume from Input Mix Right to Output Channel 10");
    if (m_Output910[5]) addElement(m_Output910[5]);

}

SaffireProDevice::~SaffireProDevice()
{
    // remove and delete control elements
    deleteElement(m_Phantom1);
    if (m_Phantom1) delete m_Phantom1;
    
    deleteElement(m_Phantom2);
    if (m_Phantom2) delete m_Phantom2;
    
    deleteElement(m_Insert1);
    if (m_Insert1) delete m_Insert1;
    
    deleteElement(m_Insert2);
    if (m_Insert2) delete m_Insert2;
    
    deleteElement(m_AC3pass);
    if (m_AC3pass) delete m_AC3pass;
    
    deleteElement(m_MidiTru);
    if (m_MidiTru) delete m_MidiTru;
    
    int i=0;
    for (i=0;i<4;i++) {
        deleteElement(m_Output12[i]);
        if (m_Output12[i]) delete m_Output12[i];
    }
    for (i=0;i<6;i++) {
        deleteElement(m_Output34[i]);
        if (m_Output34[i]) delete m_Output34[i];
    }
    for (i=0;i<6;i++) {
        deleteElement(m_Output56[i]);
        if (m_Output56[i]) delete m_Output56[i];
    }
    for (i=0;i<6;i++) {
        deleteElement(m_Output78[i]);
        if (m_Output78[i]) delete m_Output78[i];
    }
    for (i=0;i<6;i++) {
        deleteElement(m_Output910[i]);
        if (m_Output910[i]) delete m_Output910[i];
    }
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

bool
SaffireProDevice::setSpecificValue(uint32_t id, uint32_t v)
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
SaffireProDevice::getSpecificValue(uint32_t id, uint32_t *v)
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
SaffireProDevice::convertDefToSr( uint32_t def ) {
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
SaffireProDevice::convertSrToDef( int sr ) {
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

int
SaffireProDevice::getSamplingFrequency( ) {
    uint32_t sr;
    if ( !getSpecificValue(FOCUSRITE_CMD_ID_SAMPLERATE, &sr ) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    }
    
    debugOutput( DEBUG_LEVEL_NORMAL,
                     "getSampleRate: %d\n", sr );

    return convertDefToSr(sr);
}

bool
SaffireProDevice::setSamplingFrequencyDo( int s )
{
    uint32_t value=convertSrToDef(s);
    if ( !setSpecificValue(FOCUSRITE_CMD_ID_SAMPLERATE, value) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    }
    return true;
}

// FIXME: this is not really documented, and is an assumtion
int
SaffireProDevice::getSamplingFrequencyMirror( ) {
    uint32_t sr;
    if ( !getSpecificValue(FOCUSRITE_CMD_ID_SAMPLERATE_MIRROR, &sr ) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    }
    
    debugOutput( DEBUG_LEVEL_NORMAL,
                     "getSampleRateMirror: %d\n", sr );

    return convertDefToSr(sr);
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
        const int max_tries=2;
        int ntries=max_tries+1;
        while(--ntries) {
            if(!setSamplingFrequencyDo( s )) {
                debugWarning("setSamplingFrequencyDo failed\n");
            }
            
            int timeout=10; // multiples of 1s
//             while(timeout--) {
//                 // wait for a while
//                 usleep(1000 * 1000);
//                 
//                 // we should figure out how long to wait before the device
//                 // becomes available again
//                 
//                 // rediscover the device
//                 if (discover()) break;
// 
//             }
            
            if(timeout) {
                int verify=getSamplingFrequency();
                
                debugOutput( DEBUG_LEVEL_NORMAL,
                            "setSampleRate (try %d): requested samplerate %d, device now has %d\n", 
                            max_tries-ntries, s, verify );
                            
                if (s == verify) break;
            }
        }
        
        if (ntries==0) {
            debugError("Setting samplerate failed...\n");
            return false;
        }

        return true;
    }
    // not executable
    return false;

}

// --- element implementation classes

BinaryControl::BinaryControl(SaffireProDevice& parent, int id)
: Control::Discrete()
, m_Parent(parent)
, m_cmd_id ( id )
{}
BinaryControl::BinaryControl(SaffireProDevice& parent, int id,
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
    } else return val;
}

// --- element implementation classes

VolumeControl::VolumeControl(SaffireProDevice& parent, int id)
: Control::Discrete()
, m_Parent(parent)
, m_cmd_id ( id )
{}
VolumeControl::VolumeControl(SaffireProDevice& parent, int id,
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
    } else return val;
}

} // Focusrite
} // BeBoB
