/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#ifndef AVCSIGNALFORMAT_H
#define AVCSIGNALFORMAT_H

#include "avc_generic.h"

#include <libavc1394/avc1394.h>

namespace AVC {

class OutputPlugSignalFormatCmd: public AVCCommand
{
public:
    OutputPlugSignalFormatCmd(Ieee1394Service& ieee1394service);
    virtual ~OutputPlugSignalFormatCmd();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "OutputPlugSignalFormatCmd"; }
    
    byte_t m_plug;
    byte_t m_eoh;
    byte_t m_form;
    byte_t m_fmt;
    byte_t m_fdf[3];
};

class InputPlugSignalFormatCmd: public AVCCommand
{
public:
    InputPlugSignalFormatCmd(Ieee1394Service& ieee1394service);
    virtual ~InputPlugSignalFormatCmd();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "InputPlugSignalFormatCmd"; }
    
    byte_t m_plug;
    byte_t m_eoh;
    byte_t m_form;
    byte_t m_fmt;
    byte_t m_fdf[3];
};


}

#endif // AVCSIGNALFORMAT_H
