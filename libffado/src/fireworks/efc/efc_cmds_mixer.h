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
enum eCmdType {
    eCT_Get,
    eCT_Set,
};

class EfcGenericMixerCmd : public EfcCmd
{
public:
    EfcGenericMixerCmd(enum eMixerTarget, enum eMixerCommand);
    EfcGenericMixerCmd(enum eMixerTarget, enum eMixerCommand, int channel);
    virtual ~EfcGenericMixerCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual void showEfcCmd();
    
    bool setType( enum eCmdType type );
    enum eCmdType getType() {return m_type;};
    bool setTarget( enum eMixerTarget target );
    enum eMixerTarget getTarget() {return m_target;};
    bool setCommand( enum eMixerCommand cmd );
    enum eMixerCommand getCommand() {return m_command;};
    
    virtual const char* getCmdName() const
        { return "EfcGenericMixerCmd"; }

    int32_t     m_channel;
    uint32_t    m_value;

private:
    enum eCmdType       m_type;
    enum eMixerTarget   m_target;
    enum eMixerCommand  m_command;
};

} // namespace FireWorks

#endif // FIREWORKS_EFC_CMD_MIXER_H
