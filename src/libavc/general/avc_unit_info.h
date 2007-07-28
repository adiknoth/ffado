/*
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

#ifndef AVCUNITINFO_H
#define AVCUNITINFO_H

#include "avc_generic.h"

#include <libavc1394/avc1394.h>

class UnitInfoCmd: public AVCCommand
{
public:

    enum EUnitType {
        eUT_Monitor       = AVC1394_SUBUNIT_VIDEO_MONITOR,
        eUT_Audio         = AVC1394_SUBUNIT_AUDIO,
        eUT_Printer       = AVC1394_SUBUNIT_PRINTER,
        eUT_Disc          = AVC1394_SUBUNIT_DISC_RECORDER,
        eUT_VCR           = AVC1394_SUBUNIT_VCR,
        eUT_Tuner         = AVC1394_SUBUNIT_TUNER,
        eUT_CA            = AVC1394_SUBUNIT_CA,
        eUT_Camera        = AVC1394_SUBUNIT_VIDEO_CAMERA,
        eUT_Panel         = AVC1394_SUBUNIT_PANEL,
        eUT_BulltinBoard  = AVC1394_SUBUNIT_BULLETIN_BOARD,
        eUT_CameraStorage = AVC1394_SUBUNIT_CAMERA_STORAGE,
        eUT_Music         = AVC1394_SUBUNIT_MUSIC,
        eUT_VendorUnique  = AVC1394_SUBUNIT_VENDOR_UNIQUE,
        eUT_Reserved      = AVC1394_SUBUNIT_RESERVED,
        eUT_Extended      = AVC1394_SUBUNIT_EXTENDED,
        eUT_Unit          = AVC1394_SUBUNIT_UNIT,
    };

    UnitInfoCmd( Ieee1394Service& ieee1349service );
    virtual ~UnitInfoCmd();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "UnitInfoCmd"; }

    reserved_t  m_reserved;
    unit_type_t m_unit_type;
    unit_t      m_unit;

    company_id_t m_company_id;
};


#endif // AVCUNITINFO_H
