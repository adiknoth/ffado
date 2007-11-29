/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#ifndef FIREWORKS_EFC_CMD_MONITOR_H
#define FIREWORKS_EFC_CMD_MONITOR_H

#include "efc_cmd.h"

namespace FireWorks {

enum eMonitorCommand {
    eMoC_Gain,
    eMoC_Solo,
    eMoC_Mute,
    eMoC_Pan,
};

class EfcGenericMonitorCmd : public EfcCmd
{
public:
    enum eCmdType {
        eCT_Get,
        eCT_Set,
    };
public:
    EfcGenericMonitorCmd(enum eCmdType, enum eMonitorCommand);
    virtual ~EfcGenericMonitorCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual void showEfcCmd();

    int32_t     m_input;
    int32_t     m_output;
    uint32_t    m_value;

private:
    enum eCmdType         m_type;
    enum eMonitorCommand  m_command;
};

// --- Specific implementations
class EfcGetMonitorGainCmd : public EfcGenericMonitorCmd
{
public:
    EfcGetMonitorGainCmd()
    : EfcGenericMonitorCmd(eCT_Get, eMoC_Gain) {};
    virtual ~EfcGetMonitorGainCmd() {};

    virtual const char* getCmdName() const
    { return "EfcGetMonitorGainCmd"; }
};
class EfcSetMonitorGainCmd : public EfcGenericMonitorCmd
{
public:
    EfcSetMonitorGainCmd()
    : EfcGenericMonitorCmd(eCT_Set, eMoC_Gain) {};
    virtual ~EfcSetMonitorGainCmd() {};

    virtual const char* getCmdName() const
    { return "EfcSetMonitorGainCmd"; }
};

class EfcGetMonitorSoloCmd : public EfcGenericMonitorCmd
{
public:
    EfcGetMonitorSoloCmd()
    : EfcGenericMonitorCmd(eCT_Get, eMoC_Solo) {};
    virtual ~EfcGetMonitorSoloCmd() {};

    virtual const char* getCmdName() const
    { return "EfcGetMonitorSoloCmd"; }
};
class EfcSetMonitorSoloCmd : public EfcGenericMonitorCmd
{
public:
    EfcSetMonitorSoloCmd()
    : EfcGenericMonitorCmd(eCT_Set, eMoC_Solo) {};
    virtual ~EfcSetMonitorSoloCmd() {};

    virtual const char* getCmdName() const
    { return "EfcSetMonitorSoloCmd"; }
};

class EfcGetMonitorMuteCmd : public EfcGenericMonitorCmd
{
public:
    EfcGetMonitorMuteCmd()
    : EfcGenericMonitorCmd(eCT_Get, eMoC_Mute) {};
    virtual ~EfcGetMonitorMuteCmd() {};

    virtual const char* getCmdName() const
    { return "EfcGetMonitorMuteCmd"; }
};
class EfcSetMonitorMuteCmd : public EfcGenericMonitorCmd
{
public:
    EfcSetMonitorMuteCmd()
    : EfcGenericMonitorCmd(eCT_Set, eMoC_Mute) {};
    virtual ~EfcSetMonitorMuteCmd() {};

    virtual const char* getCmdName() const
    { return "EfcSetMonitorMuteCmd"; }
};

class EfcGetMonitorPanCmd : public EfcGenericMonitorCmd
{
public:
    EfcGetMonitorPanCmd()
    : EfcGenericMonitorCmd(eCT_Get, eMoC_Pan) {};
    virtual ~EfcGetMonitorPanCmd() {};

    virtual const char* getCmdName() const
    { return "EfcGetMonitorPanCmd"; }
};
class EfcSetMonitorPanCmd : public EfcGenericMonitorCmd
{
public:
    EfcSetMonitorPanCmd()
    : EfcGenericMonitorCmd(eCT_Set, eMoC_Pan) {};
    virtual ~EfcSetMonitorPanCmd() {};

    virtual const char* getCmdName() const
    { return "EfcSetMonitorPanCmd"; }
};


} // namespace FireWorks

#endif // FIREWORKS_EFC_CMD_MONITOR_H
