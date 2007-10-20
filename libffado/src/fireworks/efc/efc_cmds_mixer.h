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

#ifndef FIREWORKS_EFC_CMD_MIXER_H
#define FIREWORKS_EFC_CMD_MIXER_H

#include "efc_cmd.h"

namespace FireWorks {

enum eMixerTarget {
    eMT_PhysicalOutputMix,
    eMT_PhysicalInputMix,
    eMT_PlaybackMix,
    eMT_RecordMix,
};
enum eMixerCommand {
    eMC_Gain,
    eMC_Solo,
    eMC_Mute,
    eMC_Pan,
    eMC_Nominal,
};

class EfcGenericMixerCmd : public EfcCmd
{
public:
    enum eCmdType {
        eCT_Get,
        eCT_Set,
    };
public:
    EfcGenericMixerCmd(enum eCmdType, enum eMixerTarget, enum eMixerCommand);
    virtual ~EfcGenericMixerCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual void showEfcCmd();

    int32_t     m_channel;
    uint32_t    m_value;

private:
    enum eCmdType       m_type;
    enum eMixerTarget   m_target;
    enum eMixerCommand  m_command;
};

// --- Specific implementations
class EfcGetGainCmd : public EfcGenericMixerCmd
{
public:
    EfcGetGainCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Get, t, eMC_Gain) {};
    virtual ~EfcGetGainCmd() {};

    virtual const char* getCmdName() const
    { return "EfcGetGainCmd"; }
};
class EfcSetGainCmd : public EfcGenericMixerCmd
{
public:
    EfcSetGainCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Set, t, eMC_Gain) {};
    virtual ~EfcSetGainCmd() {};

    virtual const char* getCmdName() const
    { return "EfcSetGainCmd"; }
};

class EfcGetSoloCmd : public EfcGenericMixerCmd
{
public:
    EfcGetSoloCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Get, t, eMC_Solo) {};
    virtual ~EfcGetSoloCmd() {};

    virtual const char* getCmdName() const
    { return "EfcGetSoloCmd"; }
};
class EfcSetSoloCmd : public EfcGenericMixerCmd
{
public:
    EfcSetSoloCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Set, t, eMC_Solo) {};
    virtual ~EfcSetSoloCmd() {};

    virtual const char* getCmdName() const
    { return "EfcSetSoloCmd"; }
};

class EfcGetMuteCmd : public EfcGenericMixerCmd
{
public:
    EfcGetMuteCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Get, t, eMC_Mute) {};
    virtual ~EfcGetMuteCmd() {};

    virtual const char* getCmdName() const
    { return "EfcGetMuteCmd"; }
};
class EfcSetMuteCmd : public EfcGenericMixerCmd
{
public:
    EfcSetMuteCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Set, t, eMC_Mute) {};
    virtual ~EfcSetMuteCmd() {};

    virtual const char* getCmdName() const
    { return "EfcSetMuteCmd"; }
};

class EfcGetPanCmd : public EfcGenericMixerCmd
{
public:
    EfcGetPanCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Get, t, eMC_Pan) {};
    virtual ~EfcGetPanCmd() {};

    virtual const char* getCmdName() const
    { return "EfcGetPanCmd"; }
};
class EfcSetPanCmd : public EfcGenericMixerCmd
{
public:
    EfcSetPanCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Set, t, eMC_Pan) {};
    virtual ~EfcSetPanCmd() {};

    virtual const char* getCmdName() const
    { return "EfcSetPanCmd"; }
};

class EfcGetNominalCmd : public EfcGenericMixerCmd
{
public:
    EfcGetNominalCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Get, t, eMC_Nominal) {};
    virtual ~EfcGetNominalCmd() {};

    virtual const char* getCmdName() const
    { return "EfcGetNominalCmd"; }
};
class EfcSetNominalCmd : public EfcGenericMixerCmd
{
public:
    EfcSetNominalCmd(enum eMixerTarget t)
    : EfcGenericMixerCmd(eCT_Set, t, eMC_Nominal) {};
    virtual ~EfcSetNominalCmd() {};

    virtual const char* getCmdName() const
    { return "EfcSetNominalCmd"; }
};

} // namespace FireWorks

#endif // FIREWORKS_EFC_CMD_MIXER_H
