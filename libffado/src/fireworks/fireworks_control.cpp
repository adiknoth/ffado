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

#include "fireworks_device.h"
#include "fireworks_control.h"
#include "efc/efc_avc_cmd.h"
#include "efc/efc_cmd.h"
#include "efc/efc_cmds_mixer.h"
#include "efc/efc_cmds_monitor.h"
#include "efc/efc_cmds_hardware_ctrl.h"

#include <string>
#include <sstream>

using namespace std;

// These classes provide support for the controls on the echo devices
namespace FireWorks {

MonitorControl::MonitorControl(FireWorks::Device& p, enum eMonitorControl c)
: Control::MatrixMixer(&p, "MonitorControl")
, m_control(c)
, m_ParentDevice(p)
{
}

MonitorControl::MonitorControl(FireWorks::Device& p, enum eMonitorControl c, std::string n)
: Control::MatrixMixer(&p, n)
, m_control(c)
, m_ParentDevice(p)
{
}

void MonitorControl::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "MonitorControl\n");
}

std::string MonitorControl::getRowName( const int row )
{
    std::ostringstream rowname;
    rowname << "IN" << row;
    debugOutput(DEBUG_LEVEL_VERBOSE, "name for row %d is %s\n", 
                                     row, rowname.str().c_str());
    return rowname.str();
}

std::string MonitorControl::getColName( const int col )
{
    std::ostringstream colname;
    colname << "OUT" << col;
    debugOutput(DEBUG_LEVEL_VERBOSE, "name for col %d is %s\n", 
                                     col, colname.str().c_str());
    return colname.str();
}

int MonitorControl::canWrite( const int row, const int col )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "canWrite for row %d col %d is %d\n", 
                                     row, col, true);
    return true;
}

double MonitorControl::setValue( const int row, const int col, const double val )
{
    double retval=0.0;
    bool did_command=false;

    if(row >= (int)m_ParentDevice.getHwInfo().m_nb_phys_audio_in) {
        debugError("specified row (%u) larger than number of rows (%d)\n",
            row, m_ParentDevice.getHwInfo().m_nb_phys_audio_in);
        return 0.0;
    }
    if(col >= (int)m_ParentDevice.getHwInfo().m_nb_phys_audio_out) {
        debugError("specified col (%u) larger than number of cols (%d)\n",
            col, m_ParentDevice.getHwInfo().m_nb_phys_audio_out);
        return 0.0;
    }

    // not a switch since we create variables
    if(m_control==eMC_Gain) {
        EfcSetMonitorGainCmd setCmd;
        setCmd.m_input = row;
        setCmd.m_output = col;
        setCmd.m_value = (uint32_t)val;
        if (!m_ParentDevice.doEfcOverAVC(setCmd)) 
        {
            debugError("Cmd failed\n");
        }
        // update the session block
        m_ParentDevice.m_session.h.monitorgains[row][col] = setCmd.m_value;
        retval = setCmd.m_value;
        did_command = true;
    }
    if(m_control==eMC_Pan) {
        EfcSetMonitorPanCmd setCmd;
        setCmd.m_input = row;
        setCmd.m_output = col;
        setCmd.m_value = (uint32_t)val;
        if (!m_ParentDevice.doEfcOverAVC(setCmd)) 
        {
            debugError("Cmd failed\n");
        }
        // update the session block
        m_ParentDevice.m_session.s.monitorpans[row][col] = setCmd.m_value;
        retval = setCmd.m_value;
        did_command = true;
    }
    if(m_control==eMC_Mute) {
        EfcSetMonitorMuteCmd setCmd;
        setCmd.m_input = row;
        setCmd.m_output = col;
        setCmd.m_value = (uint32_t)val;
        if (!m_ParentDevice.doEfcOverAVC(setCmd))
        {
            debugError("Cmd failed\n");
        }
        // update the session block
        if(setCmd.m_value) {
            m_ParentDevice.m_session.s.monitorflags[row][col] |= ECHO_SESSION_MUTE_BIT;
        } else {
            m_ParentDevice.m_session.s.monitorflags[row][col] &= ~ECHO_SESSION_MUTE_BIT;
        }
        retval = setCmd.m_value;
        did_command = true;
    }
    if(m_control==eMC_Solo) {
        EfcSetMonitorSoloCmd setCmd;
        setCmd.m_input = row;
        setCmd.m_output = col;
        setCmd.m_value = (uint32_t)val;
        if (!m_ParentDevice.doEfcOverAVC(setCmd)) 
        {
            debugError("Cmd failed\n");
        }
        // update the session block
        if(setCmd.m_value) {
            m_ParentDevice.m_session.s.monitorflags[row][col] |= ECHO_SESSION_SOLO_BIT;
        } else {
            m_ParentDevice.m_session.s.monitorflags[row][col] &= ~ECHO_SESSION_SOLO_BIT;
        }
        retval = setCmd.m_value;
        did_command = true;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for row %d col %d = %lf\n", 
                                         row, col, retval);

    if (!did_command) {
        debugError("BUG: this should never happen due to enum\n");
    }
    return retval;
}

double MonitorControl::getValue( const int row, const int col )
{
    double retval=0.0;
    bool did_command=false;

    if(row >= (int)m_ParentDevice.getHwInfo().m_nb_phys_audio_in) {
        debugError("specified row (%u) larger than number of rows (%d)\n",
            row, m_ParentDevice.getHwInfo().m_nb_phys_audio_in);
        return 0.0;
    }
    if(col >= (int)m_ParentDevice.getHwInfo().m_nb_phys_audio_out) {
        debugError("specified col (%u) larger than number of cols (%d)\n",
            col, m_ParentDevice.getHwInfo().m_nb_phys_audio_out);
        return 0.0;
    }

    if(m_control==eMC_Gain) {
        EfcGetMonitorGainCmd getCmd;
        getCmd.m_input=row;
        getCmd.m_output=col;
        if (!m_ParentDevice.doEfcOverAVC(getCmd)) 
        {
            debugError("Cmd failed\n");
        }
        retval=getCmd.m_value;
        did_command=true;
    }
    if(m_control==eMC_Pan) {
        EfcGetMonitorPanCmd getCmd;
        getCmd.m_input=row;
        getCmd.m_output=col;
        if (!m_ParentDevice.doEfcOverAVC(getCmd)) 
        {
            debugError("Cmd failed\n");
        }
        retval=getCmd.m_value;
        did_command=true;
    }
    if(m_control==eMC_Mute) {
        EfcGetMonitorMuteCmd getCmd;
        getCmd.m_input=row;
        getCmd.m_output=col;
        if (!m_ParentDevice.doEfcOverAVC(getCmd)) 
        {
            debugError("Cmd failed\n");
        }
        retval=getCmd.m_value;
        did_command=true;
    }
    if(m_control==eMC_Solo) {
        EfcGetMonitorSoloCmd getCmd;
        getCmd.m_input=row;
        getCmd.m_output=col;
        if (!m_ParentDevice.doEfcOverAVC(getCmd)) 
        {
            debugError("Cmd failed\n");
        }
        retval=getCmd.m_value;
        did_command=true;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for row %d col %d = %lf\n", 
                                         row, col, retval);

    if (!did_command) {
        debugError("BUG: this should never happen due to enum\n");
    }
    return retval;
}

int MonitorControl::getRowCount( )
{
    return m_ParentDevice.getHwInfo().m_nb_phys_audio_in;
}

int MonitorControl::getColCount( )
{
    return m_ParentDevice.getHwInfo().m_nb_phys_audio_out;
}

// --- the generic control element for single-value controls

SimpleControl::SimpleControl(FireWorks::Device& p,
                             enum eMixerTarget t,
                             enum eMixerCommand c,
                             int channel)
: Control::Continuous(&p, "SimpleControl")
, m_Slave(new EfcGenericMixerCmd(t, c, channel))
, m_ParentDevice(p)
{
}

SimpleControl::SimpleControl(FireWorks::Device& p,
                             enum eMixerTarget t,
                             enum eMixerCommand c,
                             int channel,
                             std::string n)
: Control::Continuous(&p, n)
, m_Slave(new EfcGenericMixerCmd(t, c, channel))
, m_ParentDevice(p)
{
}

SimpleControl::~SimpleControl()
{
    delete m_Slave;
}

void SimpleControl::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "SimpleControl\n");
    if(m_Slave) m_Slave->showEfcCmd();
}

bool SimpleControl::setValue( const double val )
{
    if(m_Slave) {
        m_Slave->setType(eCT_Set);
        m_Slave->m_value = (uint32_t)val;
        if (!m_ParentDevice.doEfcOverAVC(*m_Slave)) 
        {
            debugError("Cmd failed\n");
            return 0.0;
        }

        // update the session block
        switch(m_Slave->getTarget()) {
        case eMT_PlaybackMix:
            switch(m_Slave->getCommand()) {
            case eMC_Gain:
                m_ParentDevice.m_session.h.playbackgains[m_Slave->m_channel] = m_Slave->m_value;
                break;
            default: // nothing
                break;
            }
            break;
        case eMT_PhysicalOutputMix:
            switch(m_Slave->getCommand()) {
            case eMC_Gain:
                m_ParentDevice.m_session.h.outputgains[m_Slave->m_channel] = m_Slave->m_value;
                break;
            default: // nothing
                break;
            }
            break;
        default: // nothing
            break;
        }
        debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for channel %d to %lf = %lf\n", 
                                            m_Slave->m_channel, val, m_Slave->m_value);
        return true;
    } else {
        debugError("No slave EFC command present\n");
        return false;
    }
}

double SimpleControl::getValue( )
{
    if(m_Slave) {
        m_Slave->setType(eCT_Get);
        if (!m_ParentDevice.doEfcOverAVC(*m_Slave)) 
        {
            debugError("Cmd failed\n");
            return 0.0;
        }
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for channel %d = %lf\n", 
                                            m_Slave->m_channel, m_Slave->m_value);
        return m_Slave->m_value;
    } else {
        debugError("No slave EFC command present\n");
        return 0.0;
    }
}

// --- the generic control element for on-off controls

BinaryControl::BinaryControl(FireWorks::Device& p,
                             enum eMixerTarget t,
                             enum eMixerCommand c,
                             int channel, int bit)
: Control::Discrete(&p, "BinaryControl")
, m_bit(bit)
, m_Slave(new EfcGenericMixerCmd(t, c, channel))
, m_ParentDevice(p)
{
}

BinaryControl::BinaryControl(FireWorks::Device& p,
                             enum eMixerTarget t,
                             enum eMixerCommand c,
                             int channel, int bit,
                             std::string n)
: Control::Discrete(&p, n)
, m_bit(bit)
, m_Slave(new EfcGenericMixerCmd(t, c, channel))
, m_ParentDevice(p)
{
}

BinaryControl::~BinaryControl()
{
    delete m_Slave;
}

void BinaryControl::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "BinaryControl\n");
    if(m_Slave) m_Slave->showEfcCmd();
}

bool BinaryControl::setValue( const int val )
{

    if(m_Slave) {
        uint32_t reg;
        uint32_t old_reg;
        
        m_Slave->setType(eCT_Get);
        reg=m_Slave->m_value;
        
        old_reg=reg;
        if (val) {
            reg |= (1<<m_bit);
        } else {
            reg &= ~(1<<m_bit);
        }
    
        m_Slave->setType(eCT_Set);
        m_Slave->m_value=reg;
        if (!m_ParentDevice.doEfcOverAVC(*m_Slave)) 
        {
            debugError("Cmd failed\n");
            return 0;
        }

        // update the session block
        switch(m_Slave->getTarget()) {
        case eMT_PlaybackMix:
            switch(m_Slave->getCommand()) {
            case eMC_Mute:
                m_ParentDevice.m_session.s.playbacks[m_Slave->m_channel].mute = m_Slave->m_value;
                break;
            case eMC_Solo:
                m_ParentDevice.m_session.s.playbacks[m_Slave->m_channel].solo = m_Slave->m_value;
                break;
            default: // nothing
                break;
            }
            break;
        case eMT_PhysicalOutputMix:
            switch(m_Slave->getCommand()) {
            case eMC_Mute:
                m_ParentDevice.m_session.s.outputs[m_Slave->m_channel].mute = m_Slave->m_value;
                break;
            case eMC_Nominal:
                m_ParentDevice.m_session.s.outputs[m_Slave->m_channel].shift = m_Slave->m_value;
                break;
            default: // nothing
                break;
            }
            break;
        case eMT_PhysicalInputMix:
            switch(m_Slave->getCommand()) {
            //case eMC_Pad:
            //    m_ParentDevice.m_session.s.inputs[m_Slave->m_channel].pad = m_Slave->m_value;
            //    break;
            case eMC_Nominal:
                m_ParentDevice.m_session.s.inputs[m_Slave->m_channel].shift = m_Slave->m_value;
                break;
            default: // nothing
                break;
            }
            break;
        default: // nothing
            break;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for channel %d to %ld (reg: 0x%08X => 0x%08X)\n", 
                                            m_Slave->m_channel, val, old_reg, reg);
        return true;
    } else {
        debugError("No slave EFC command present\n");
        return false;
    }
}

int BinaryControl::getValue( )
{
    if(m_Slave) {
        // workaround for the failing get nominal command for input channels
        // get it from the session block
        if ((m_Slave->getTarget() == eMT_PhysicalInputMix)
            && (m_Slave->getCommand() == eMC_Nominal)) {
            int val = m_ParentDevice.m_session.s.inputs[m_Slave->m_channel].shift;
            debugOutput(DEBUG_LEVEL_VERBOSE, "input pad workaround: %08X\n", val);
            return val;
        }
        m_Slave->setType(eCT_Get);
        if (!m_ParentDevice.doEfcOverAVC(*m_Slave)) 
        {
            debugError("Cmd failed\n");
            return 0;
        }
        bool val= (m_Slave->m_value & (1<<m_bit)) != 0;
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for channel %d: reg: 0x%08X, result=%d\n", 
                                         m_Slave->m_channel, m_Slave->m_value, val);
        return val;
    } else {
        debugError("No slave EFC command present\n");
        return 0;
    }
}

// --- control element for flags

SpdifModeControl::SpdifModeControl(FireWorks::Device& parent)
: Control::Discrete(&parent, "SpdifModeControl")
, m_ParentDevice(parent)
{
}

SpdifModeControl::SpdifModeControl(FireWorks::Device& parent,
                                   std::string n)
: Control::Discrete(&parent, n)
, m_ParentDevice(parent)
{
}

SpdifModeControl::~SpdifModeControl()
{
}

void SpdifModeControl::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "SpdifModeControl\n");
}

bool SpdifModeControl::setValue( const int val )
{
    EfcChangeFlagsCmd setCmd;
    if(val) {
        setCmd.m_setmask = FIREWORKS_EFC_FLAG_SPDIF_PRO;
    } else {
        setCmd.m_clearmask = FIREWORKS_EFC_FLAG_SPDIF_PRO;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue val: %d setmask: %08X, clear: %08X\n", 
                                      val, setCmd.m_setmask, setCmd.m_clearmask);
    if (!m_ParentDevice.doEfcOverAVC(setCmd))
    {
        debugError("Cmd failed\n");
        return false;
    }
    return true;
}

int SpdifModeControl::getValue( )
{
    EfcGetFlagsCmd getCmd;
    if (!m_ParentDevice.doEfcOverAVC(getCmd))
    {
        debugError("Cmd failed\n");
        return 0;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "got flags: %08X\n", 
                                      getCmd.m_flags);
    if(getCmd.m_flags & FIREWORKS_EFC_FLAG_SPDIF_PRO) return 1;
    else return 0;
}

// --- io config controls

IOConfigControl::IOConfigControl(FireWorks::Device& parent,
                                 enum eIOConfigRegister r)
: Control::Discrete(&parent, "IOConfigControl")
, m_Slave(new EfcGenericIOConfigCmd(r))
, m_ParentDevice(parent)
{
}

IOConfigControl::IOConfigControl(FireWorks::Device& parent,
                                 enum eIOConfigRegister r,
                                 std::string n)
: Control::Discrete(&parent, n)
, m_Slave(new EfcGenericIOConfigCmd(r))
, m_ParentDevice(parent)
{
}

IOConfigControl::~IOConfigControl()
{
    delete m_Slave;
}

void IOConfigControl::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "IOConfigControl\n");
    if(m_Slave) m_Slave->showEfcCmd();
}

bool IOConfigControl::setValue( const int val )
{
    if(m_Slave) {
        m_Slave->setType(eCT_Set);
        m_Slave->m_value=val;
        if (!m_ParentDevice.doEfcOverAVC(*m_Slave)) 
        {
            debugError("Cmd failed\n");
            return 0;
        }
        debugOutput(DEBUG_LEVEL_VERBOSE, "setValue to %ld \n", val);
        return true;
    } else {
        debugError("No slave EFC command present\n");
        return false;
    }
}

int IOConfigControl::getValue( )
{
    if(m_Slave) {
        m_Slave->setType(eCT_Get);
        if (!m_ParentDevice.doEfcOverAVC(*m_Slave)) 
        {
            debugError("Cmd failed\n");
            return 0;
        }
        debugOutput(DEBUG_LEVEL_VERBOSE, "getValue: result=%d\n",
                                          m_Slave->m_value);
        return m_Slave->m_value;
    } else {
        debugError("No slave EFC command present\n");
        return 0;
    }
}

// control to get hardware information
HwInfoControl::HwInfoControl(FireWorks::Device& p,
                             enum eHwInfoField f)
: Control::Discrete(&p, "HwInfoControl")
, m_ParentDevice(p)
, m_Field(f)
{
}

HwInfoControl::HwInfoControl(FireWorks::Device& p,
                             enum eHwInfoField f,
                             std::string n)
: Control::Discrete(&p, n)
, m_ParentDevice(p)
, m_Field(f)
{
}

HwInfoControl::~HwInfoControl()
{
}

int HwInfoControl::getValue()
{
    switch (m_Field) {
        case eHIF_PhysicalAudioOutCount:
            return m_ParentDevice.getHwInfo().m_nb_phys_audio_out;
        case eHIF_PhysicalAudioInCount:
            return m_ParentDevice.getHwInfo().m_nb_phys_audio_in;
        case eHIF_1394PlaybackCount:
            return m_ParentDevice.getHwInfo().m_nb_1394_playback_channels;
        case eHIF_1394RecordCount:
            return m_ParentDevice.getHwInfo().m_nb_1394_record_channels;
        case eHIF_GroupOutCount:
            return m_ParentDevice.getHwInfo().m_nb_out_groups;
        case eHIF_GroupInCount:
            return m_ParentDevice.getHwInfo().m_nb_in_groups;
        case eHIF_PhantomPower:
            return m_ParentDevice.getHwInfo().hasSoftwarePhantom();
        default:
            debugError("Bogus field\n");
            return 0;
    }
}

void HwInfoControl::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "HwInfoControl\n");
}


// control to save settings
MultiControl::MultiControl(FireWorks::Device& p, enum eType t)
: Control::Discrete(&p, "MultiControl")
, m_ParentDevice(p)
, m_Type(t)
{
}

MultiControl::MultiControl(FireWorks::Device& p,
                           enum eType t, std::string n)
: Control::Discrete(&p, n)
, m_ParentDevice(p)
, m_Type(t)
{
}

MultiControl::~MultiControl()
{
}

bool MultiControl::setValue(const int v)
{
    switch(m_Type) {
    case eT_SaveSession:
        debugOutput(DEBUG_LEVEL_VERBOSE, "saving session\n");
        return m_ParentDevice.saveSession();
    case eT_Identify:
        debugOutput(DEBUG_LEVEL_VERBOSE, "indentify device\n");
        {
            EfcIdentifyCmd cmd;
            if (!m_ParentDevice.doEfcOverAVC(cmd)) 
            {
                debugError("Cmd failed\n");
                return false;
            }
        }
        return true;
    default:
        debugError("Bad type\n");
        return false;
    }
}

void MultiControl::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "MultiControl\n");
    switch(m_Type) {
    case eT_SaveSession:
        debugOutput(DEBUG_LEVEL_NORMAL, "Type: SaveSession\n");
        break;
    case eT_Identify:
        debugOutput(DEBUG_LEVEL_NORMAL, "Type: Identify\n");
        break;
    default:
        debugError("Bad type\n");
    }
}

} // FireWorks
