/*
 * Copyright (C)      2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
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

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

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

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

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
