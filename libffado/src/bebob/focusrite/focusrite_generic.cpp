/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#include "focusrite_saffirepro.h"
#include "focusrite_cmd.h"

#include "libutil/ByteSwap.h"

namespace BeBoB {
namespace Focusrite {

FocusriteDevice::FocusriteDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::Device( d, configRom)
    , m_cmd_time_interval( 0 )
    , m_earliest_next_cmd_time( 0 )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Focusrite::FocusriteDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
    addOption(Util::OptionContainer::Option("useAvcForParameters", false));
}

void
FocusriteDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::Focusrite::FocusriteDevice\n");
    BeBoB::Device::showDevice();
}

void
FocusriteDevice::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );

    BeBoB::Device::setVerboseLevel(l);
}

bool
FocusriteDevice::setSpecificValue(uint32_t id, uint32_t v)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Writing parameter address space id 0x%08X (%u), data: 0x%08X\n",
        id, id, v);
    bool use_avc = false;
    if(!getOption("useAvcForParameters", use_avc)) {
        debugWarning("Could not retrieve useAvcForParameters parameter, defaulting to false\n");
    }

    // rate control
    ffado_microsecs_t now = Util::SystemTimeSource::getCurrentTimeAsUsecs();
    if(m_cmd_time_interval && (m_earliest_next_cmd_time > now)) {
        ffado_microsecs_t wait = m_earliest_next_cmd_time - now;
        debugOutput( DEBUG_LEVEL_VERBOSE, "Rate control... %"PRIu64"\n", wait );
        Util::SystemTimeSource::SleepUsecRelative(wait);
    }
    m_earliest_next_cmd_time = now + m_cmd_time_interval;

    if (use_avc) {
        return setSpecificValueAvc(id, v);
    } else {
        return setSpecificValueARM(id, v);
    }
}

bool
FocusriteDevice::getSpecificValue(uint32_t id, uint32_t *v)
{
    bool retval;
    bool use_avc = false;
    if(!getOption("useAvcForParameters", use_avc)) {
        debugWarning("Could not retrieve useAvcForParameters parameter, defaulting to false\n");
    }

    // rate control
    ffado_microsecs_t now = Util::SystemTimeSource::getCurrentTimeAsUsecs();
    if(m_cmd_time_interval && (m_earliest_next_cmd_time > now)) {
        ffado_microsecs_t wait = m_earliest_next_cmd_time - now;
        debugOutput( DEBUG_LEVEL_VERBOSE, "Rate control... %"PRIu64"\n", wait );
        Util::SystemTimeSource::SleepUsecRelative(wait);
    }
    m_earliest_next_cmd_time = now + m_cmd_time_interval;

    // execute
    if (use_avc) {
        retval = getSpecificValueAvc(id, v);
    } else {
        retval = getSpecificValueARM(id, v);
    }
    debugOutput(DEBUG_LEVEL_VERBOSE,"Read parameter address space id 0x%08X (%u): %08X\n", id, id, *v);
    return retval;
}

// The AV/C methods to set parameters
bool
FocusriteDevice::setSpecificValueAvc(uint32_t id, uint32_t v)
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
FocusriteDevice::getSpecificValueAvc(uint32_t id, uint32_t *v)
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

// The ARM methods to set parameters
bool
FocusriteDevice::setSpecificValueARM(uint32_t id, uint32_t v)
{
    fb_quadlet_t data = v;
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing parameter address space id 0x%08X (%u), data: 0x%08X\n",
        id, id, data);

    fb_nodeaddr_t addr = FR_PARAM_SPACE_START + (id * 4);
    fb_nodeid_t nodeId = getNodeId() | 0xFFC0;

    if(!get1394Service().write_quadlet( nodeId, addr, CondSwapToBus32(data) ) ) {
        debugError("Could not write to node 0x%04X addr 0x%012"PRIX64"\n", nodeId, addr);
        return false;
    }
    return true;
}

bool
FocusriteDevice::getSpecificValueARM(uint32_t id, uint32_t *v)
{
    fb_quadlet_t result;
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading parameter address space id 0x%08X\n", id);

    fb_nodeaddr_t addr = FR_PARAM_SPACE_START + (id * 4);
    fb_nodeid_t nodeId = getNodeId() | 0xFFC0;

    if(!get1394Service().read_quadlet( nodeId, addr, &result ) ) {
        debugError("Could not read from node 0x%04X addr 0x%012"PRIX64"\n", nodeId, addr);
        return false;
    }

    result = CondSwapFromBus32(result);
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Read result: 0x%08X\n", result);

    *v = result;
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
        default:
            debugWarning("Unsupported samplerate def: %08X\n", def);
            return 0;
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

BinaryControl::BinaryControl(FocusriteDevice& parent, int id, int bit)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_cmd_id ( id )
, m_cmd_bit ( bit )
{}
BinaryControl::BinaryControl(FocusriteDevice& parent, int id, int bit,
                std::string name, std::string label, std::string descr)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_cmd_id ( id )
, m_cmd_bit ( bit )
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

bool
BinaryControl::setValue(int v)
{
    uint32_t reg;
    uint32_t old_reg;

    if ( !m_Parent.getSpecificValue(m_cmd_id, &reg) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    }
    
    old_reg=reg;
    if (v) {
        reg |= (1<<m_cmd_bit);
    } else {
        reg &= ~(1<<m_cmd_bit);
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for id %d to %d (reg: 0x%08X => 0x%08X)\n", 
                                     m_cmd_id, v, old_reg, reg);

    if ( !m_Parent.setSpecificValue(m_cmd_id, reg) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    } else return true;
}

int
BinaryControl::getValue()
{
    uint32_t reg;

    if ( !m_Parent.getSpecificValue(m_cmd_id, &reg) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    } else {
        bool val= (reg & (1<<m_cmd_bit)) != 0;
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for %d: reg: 0x%08X, result=%d\n", 
                                         m_cmd_id, reg, val);
        return val;
    }
}

// --- element implementation classes

VolumeControl::VolumeControl(FocusriteDevice& parent, int id)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_cmd_id ( id )
{}
VolumeControl::VolumeControl(FocusriteDevice& parent, int id,
                std::string name, std::string label, std::string descr)
: Control::Discrete(&parent)
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

MeteringControl::MeteringControl(FocusriteDevice& parent, int id)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_cmd_id ( id )
{}
MeteringControl::MeteringControl(FocusriteDevice& parent, int id,
                std::string name, std::string label, std::string descr)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_cmd_id ( id )
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

int
MeteringControl::getValue()
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

// reg control
RegisterControl::RegisterControl(FocusriteDevice& parent)
: Control::Register(&parent)
, m_Parent(parent)
{}
RegisterControl::RegisterControl(FocusriteDevice& parent,
                 std::string name, std::string label, std::string descr)
: Control::Register(&parent)
, m_Parent(parent)
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

bool
RegisterControl::setValue(uint64_t addr, uint64_t v)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for addr %"PRIu64" to %"PRIu64"\n",
                                     addr, v);

    if ( !m_Parent.setSpecificValue(addr, v) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    } else return true;
}

uint64_t
RegisterControl::getValue(uint64_t addr)
{
    uint32_t val=0;

    if ( !m_Parent.getSpecificValue(addr, &val) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for %"PRIu64" = %u\n", 
                                         addr, val);
        return val;
    }
}

// low resolution volume control
VolumeControlLowRes::VolumeControlLowRes(FocusriteDevice& parent, int id, int shift)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_cmd_id ( id )
, m_bit_shift( shift )
{}
VolumeControlLowRes::VolumeControlLowRes(FocusriteDevice& parent, int id, int shift,
                std::string name, std::string label, std::string descr)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_cmd_id ( id )
, m_bit_shift( shift )
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

bool
VolumeControlLowRes::setValue(int v)
{
    uint32_t reg;
    uint32_t old_reg;
    
    if (v>0xFF) v=0xFF;
    else if (v<0) v=0;

    if ( !m_Parent.getSpecificValue(m_cmd_id, &reg) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    }
    
    old_reg=reg;
    reg &= ~(0xFF<<m_bit_shift);
    reg |= (v<<m_bit_shift);

    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for id %d to %d, shift %d (reg: 0x%08X => 0x%08X)\n", 
                                     m_cmd_id, v, m_bit_shift, old_reg, reg);

    if ( !m_Parent.setSpecificValue(m_cmd_id, reg) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    } else return true;
}

int
VolumeControlLowRes::getValue()
{
    uint32_t val, reg;

    if ( !m_Parent.getSpecificValue(m_cmd_id, &reg) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    } else {
        val = (reg & 0xFF)>>m_bit_shift;
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for %d: reg: 0x%08X, result=%d\n", 
                                         m_cmd_id, reg, val);
        return val;
    }
}

// hardware dial control
DialPositionControl::DialPositionControl(FocusriteDevice& parent, int id, int shift)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_cmd_id ( id )
, m_shift ( shift )
{}
DialPositionControl::DialPositionControl(FocusriteDevice& parent, int id, int shift,
                std::string name, std::string label, std::string descr)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_cmd_id ( id )
, m_shift ( shift )
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

int
DialPositionControl::getValue()
{
    uint32_t val=0;

    if ( !m_Parent.getSpecificValue(m_cmd_id, &val) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    } else {
        if (m_shift > 0) {
            val = val >> m_shift;
        } else if (m_shift < 0) {
            val = val << -m_shift;
        }
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for %d = %d\n", 
                                         m_cmd_id, val);
        return val;
    }
}

// Saffire pro matrix mixer element
FocusriteMatrixMixer::FocusriteMatrixMixer(FocusriteDevice& p)
: Control::MatrixMixer(&p, "MatrixMixer")
, m_Parent(p)
{
}

FocusriteMatrixMixer::FocusriteMatrixMixer(FocusriteDevice& p,std::string n)
: Control::MatrixMixer(&p, n)
, m_Parent(p)
{
}

void FocusriteMatrixMixer::addSignalInfo(std::vector<struct sSignalInfo> &target,
    std::string name, std::string label, std::string descr)
{
    struct sSignalInfo s;
    s.name=name;
    s.label=label;
    s.description=descr;

    target.push_back(s);
}

void FocusriteMatrixMixer::setCellInfo(int row, int col, int addr, bool valid)
{
    struct sCellInfo c;
    c.row=row;
    c.col=col;
    c.valid=valid;
    c.address=addr;

    m_CellInfo.at(row).at(col) = c;
}

void FocusriteMatrixMixer::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Focusrite Matrix mixer\n");
}

std::string FocusriteMatrixMixer::getRowName( const int row )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "name for row %d is %s\n", 
                                     row, m_RowInfo.at(row).name.c_str());
    return m_RowInfo.at(row).name;
}

std::string FocusriteMatrixMixer::getColName( const int col )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "name for col %d is %s\n", 
                                     col, m_ColInfo.at(col).name.c_str());
    return m_ColInfo.at(col).name;
}

int FocusriteMatrixMixer::canWrite( const int row, const int col )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "canWrite for row %d col %d is %d\n", 
                                     row, col, m_CellInfo.at(row).at(col).valid);
    return m_CellInfo.at(row).at(col).valid;
}

double FocusriteMatrixMixer::setValue( const int row, const int col, const double val )
{
    int32_t v = (int32_t)val;
    struct sCellInfo c = m_CellInfo.at(row).at(col);

    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for id %d row %d col %d to %lf (%d)\n", 
                                     c.address, row, col, val, v);

    if (v>0x07FFF) v=0x07FFF;
    else if (v<0) v=0;

    if ( !m_Parent.setSpecificValue(c.address, v) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    } else return true;
}

double FocusriteMatrixMixer::getValue( const int row, const int col )
{
    struct sCellInfo c=m_CellInfo.at(row).at(col);
    uint32_t val=0;

    if ( !m_Parent.getSpecificValue(c.address, &val) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for id %d row %d col %d = %u\n", 
                                         c.address, row, col, val);
        return val;
    }
}

int FocusriteMatrixMixer::getRowCount( )
{
    return m_RowInfo.size();
}

int FocusriteMatrixMixer::getColCount( )
{
    return m_ColInfo.size();
}


} // Focusrite
} // BeBoB
