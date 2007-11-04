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

#include "fireworks_device.h"
#include "fireworks_control.h"
#include "efc/efc_avc_cmd.h"
#include "efc/efc_cmd.h"
#include "efc/efc_cmds_mixer.h"
#include "efc/efc_cmds_monitor.h"

#include <string>
#include <sstream>

using namespace std;

// These classes provide support for the controls on the echo devices
namespace FireWorks {

MonitorControl::MonitorControl(FireWorks::Device& p, enum eMonitorControl c)
: Control::MatrixMixer("MonitorControl")
, m_control(c)
, m_Parent(p)
{
}

MonitorControl::MonitorControl(FireWorks::Device& p, enum eMonitorControl c, std::string n)
: Control::MatrixMixer(n)
, m_control(c)
, m_Parent(p)
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

    if(row >= (int)m_Parent.getHwInfo().m_nb_phys_audio_in) {
        debugError("specified row (%u) larger than number of rows (%d)\n",
            row, m_Parent.getHwInfo().m_nb_phys_audio_in);
        return 0.0;
    }
    if(col >= (int)m_Parent.getHwInfo().m_nb_phys_audio_out) {
        debugError("specified col (%u) larger than number of cols (%d)\n",
            col, m_Parent.getHwInfo().m_nb_phys_audio_out);
        return 0.0;
    }

    if(m_control==eMC_Gain) {
        EfcSetMonitorGainCmd setCmd;
        setCmd.m_input=row;
        setCmd.m_output=col;
        setCmd.m_value=(uint32_t)val;
        if (!m_Parent.doEfcOverAVC(setCmd)) 
        {
            debugFatal("Cmd failed\n");
        }
        retval=setCmd.m_value;
        did_command=true;
    }
    if(m_control==eMC_Pan) {
        EfcSetMonitorPanCmd setCmd;
        setCmd.m_input=row;
        setCmd.m_output=col;
        setCmd.m_value=(uint32_t)val;
        if (!m_Parent.doEfcOverAVC(setCmd)) 
        {
            debugFatal("Cmd failed\n");
        }
        retval=setCmd.m_value;
        did_command=true;
    }
    if(m_control==eMC_Mute) {
        EfcSetMonitorMuteCmd setCmd;
        setCmd.m_input=row;
        setCmd.m_output=col;
        setCmd.m_value=(uint32_t)val;
        if (!m_Parent.doEfcOverAVC(setCmd)) 
        {
            debugFatal("Cmd failed\n");
        }
        retval=setCmd.m_value;
        did_command=true;
    }
    if(m_control==eMC_Solo) {
        EfcSetMonitorSoloCmd setCmd;
        setCmd.m_input=row;
        setCmd.m_output=col;
        setCmd.m_value=(uint32_t)val;
        if (!m_Parent.doEfcOverAVC(setCmd)) 
        {
            debugFatal("Cmd failed\n");
        }
        retval=setCmd.m_value;
        did_command=true;
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

    if(row >= (int)m_Parent.getHwInfo().m_nb_phys_audio_in) {
        debugError("specified row (%u) larger than number of rows (%d)\n",
            row, m_Parent.getHwInfo().m_nb_phys_audio_in);
        return 0.0;
    }
    if(col >= (int)m_Parent.getHwInfo().m_nb_phys_audio_out) {
        debugError("specified col (%u) larger than number of cols (%d)\n",
            col, m_Parent.getHwInfo().m_nb_phys_audio_out);
        return 0.0;
    }

    if(m_control==eMC_Gain) {
        EfcGetMonitorGainCmd getCmd;
        getCmd.m_input=row;
        getCmd.m_output=col;
        if (!m_Parent.doEfcOverAVC(getCmd)) 
        {
            debugFatal("Cmd failed\n");
        }
        retval=getCmd.m_value;
        did_command=true;
    }
    if(m_control==eMC_Pan) {
        EfcGetMonitorPanCmd getCmd;
        getCmd.m_input=row;
        getCmd.m_output=col;
        if (!m_Parent.doEfcOverAVC(getCmd)) 
        {
            debugFatal("Cmd failed\n");
        }
        retval=getCmd.m_value;
        did_command=true;
    }
    if(m_control==eMC_Mute) {
        EfcGetMonitorMuteCmd getCmd;
        getCmd.m_input=row;
        getCmd.m_output=col;
        if (!m_Parent.doEfcOverAVC(getCmd)) 
        {
            debugFatal("Cmd failed\n");
        }
        retval=getCmd.m_value;
        did_command=true;
    }
    if(m_control==eMC_Solo) {
        EfcGetMonitorSoloCmd getCmd;
        getCmd.m_input=row;
        getCmd.m_output=col;
        if (!m_Parent.doEfcOverAVC(getCmd)) 
        {
            debugFatal("Cmd failed\n");
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
    return m_Parent.getHwInfo().m_nb_phys_audio_in;
}

int MonitorControl::getColCount( )
{
    return m_Parent.getHwInfo().m_nb_phys_audio_out;
}

// --- the generic control element for single-value controls

SimpleControl::SimpleControl(FireWorks::Device& p,
                             enum eMixerTarget t,
                             enum eMixerCommand c,
                             int channel)
: Control::Continuous("SimpleControl")
, m_Slave(new EfcGenericMixerCmd(t, c, channel))
, m_Parent(p)
{
}

SimpleControl::SimpleControl(FireWorks::Device& p,
                             enum eMixerTarget t,
                             enum eMixerCommand c,
                             int channel,
                             std::string n)
: Control::Continuous(n)
, m_Slave(new EfcGenericMixerCmd(t, c, channel))
, m_Parent(p)
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
        m_Slave->m_value=(uint32_t)val;
        if (!m_Parent.doEfcOverAVC(*m_Slave)) 
        {
            debugFatal("Cmd failed\n");
            return 0.0;
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
        if (!m_Parent.doEfcOverAVC(*m_Slave)) 
        {
            debugFatal("Cmd failed\n");
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
: Control::Discrete("BinaryControl")
, m_bit(bit)
, m_Slave(new EfcGenericMixerCmd(t, c, channel))
, m_Parent(p)
{
}

BinaryControl::BinaryControl(FireWorks::Device& p,
                             enum eMixerTarget t,
                             enum eMixerCommand c,
                             int channel, int bit,
                             std::string n)
: Control::Discrete(n)
, m_bit(bit)
, m_Slave(new EfcGenericMixerCmd(t, c, channel))
, m_Parent(p)
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
        if (!m_Parent.doEfcOverAVC(*m_Slave)) 
        {
            debugFatal("Cmd failed\n");
            return 0;
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
        m_Slave->setType(eCT_Get);
        if (!m_Parent.doEfcOverAVC(*m_Slave)) 
        {
            debugFatal("Cmd failed\n");
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

// --- io config controls

IOConfigControl::IOConfigControl(FireWorks::Device& parent,
                                 enum eIOConfigRegister r)
: Control::Discrete("IOConfigControl")
, m_Slave(new EfcGenericIOConfigCmd(r))
, m_Parent(parent)
{
}

IOConfigControl::IOConfigControl(FireWorks::Device& parent,
                                 enum eIOConfigRegister r,
                                 std::string n)
: Control::Discrete(n)
, m_Slave(new EfcGenericIOConfigCmd(r))
, m_Parent(parent)
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
        if (!m_Parent.doEfcOverAVC(*m_Slave)) 
        {
            debugFatal("Cmd failed\n");
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
        if (!m_Parent.doEfcOverAVC(*m_Slave)) 
        {
            debugFatal("Cmd failed\n");
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



} // FireWorks
