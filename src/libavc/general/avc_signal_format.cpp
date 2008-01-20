/*
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

#include "avc_signal_format.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace AVC {

OutputPlugSignalFormatCmd::OutputPlugSignalFormatCmd(Ieee1394Service& ieee1394service)
    : AVCCommand( ieee1394service, AVC1394_CMD_OUTPUT_PLUG_SIGNAL_FORMAT )
    , m_plug ( 0 )
    , m_eoh  ( 1 )
    , m_form ( 0 )
    , m_fmt  ( 0 )
{
    m_fdf[0]=0xFF;
    m_fdf[1]=0xFF;
    m_fdf[2]=0xFF;
}

OutputPlugSignalFormatCmd::~OutputPlugSignalFormatCmd()
{
}

bool
OutputPlugSignalFormatCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= AVCCommand::serialize( se );
    
    result &= se.write(m_plug,"OutputPlugSignalFormatCmd plug");
    
    byte_t tmp = ((m_eoh & 0x01)<<7);
    tmp |= ((m_form & 0x01)<<6);
    tmp |= (m_fmt & 0x3f);
    
    result &= se.write(tmp,"OutputPlugSignalFormatCmd eoh,form,fmt");
    
    result &= se.write(m_fdf[0],"OutputPlugSignalFormatCmd fdf[0]");
    result &= se.write(m_fdf[1],"OutputPlugSignalFormatCmd fdf[1]");
    result &= se.write(m_fdf[2],"OutputPlugSignalFormatCmd fdf[2]");

    return result;
}

bool
OutputPlugSignalFormatCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= AVCCommand::deserialize( de );
    result &= de.read(&m_plug);
    
    byte_t tmp;
    result &= de.read(&tmp);
    
    m_eoh=((tmp & 0x80)>>7);
    m_form=((tmp & 0x40)>>6);
    m_fmt=tmp & 0x3f;
    
    result &= de.read(&m_fdf[0]);
    result &= de.read(&m_fdf[1]);
    result &= de.read(&m_fdf[2]);
    
    return result;
}

//------------------------

InputPlugSignalFormatCmd::InputPlugSignalFormatCmd(Ieee1394Service& ieee1394service)
    : AVCCommand( ieee1394service, AVC1394_CMD_INPUT_PLUG_SIGNAL_FORMAT )
    , m_plug ( 0 )
    , m_eoh  ( 1 )
    , m_form ( 0 )
    , m_fmt  ( 0 )
{
    m_fdf[0]=0xFF;
    m_fdf[1]=0xFF;
    m_fdf[2]=0xFF;
}

InputPlugSignalFormatCmd::~InputPlugSignalFormatCmd()
{
}

bool
InputPlugSignalFormatCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= AVCCommand::serialize( se );
    
    result &= se.write(m_plug,"InputPlugSignalFormatCmd plug");
    
    byte_t tmp = ((m_eoh& 0x01)<<7);
    tmp |= ((m_form& 0x01)<<6);
    tmp |= (m_fmt& 0x3f);
    
    result &= se.write(tmp,"InputPlugSignalFormatCmd eoh,form,fmt");
    
    result &= se.write(m_fdf[0],"InputPlugSignalFormatCmd fdf[0]");
    result &= se.write(m_fdf[1],"InputPlugSignalFormatCmd fdf[1]");
    result &= se.write(m_fdf[2],"InputPlugSignalFormatCmd fdf[2]");

    return result;
}

bool
InputPlugSignalFormatCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= AVCCommand::deserialize( de );
    result &= de.read(&m_plug);
    
    byte_t tmp;
    result &= de.read(&tmp);
    
    m_eoh=((tmp & 0x80)>>7);
    m_form=((tmp & 0x40)>>6);
    m_fmt=tmp & 0x3f;
    
    result &= de.read(&m_fdf[0]);
    result &= de.read(&m_fdf[1]);
    result &= de.read(&m_fdf[2]);
    
    return result;
}
}
