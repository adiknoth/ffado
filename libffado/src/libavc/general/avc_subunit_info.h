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

#ifndef AVCSUBUNITINFO_H
#define AVCSUBUNITINFO_H

#include "avc_generic.h"

#include <libavc1394/avc1394.h>


namespace AVC {

// No extended subunit queries supported

class SubUnitInfoCmd: public AVCCommand
{
public:
    SubUnitInfoCmd( Ieee1394Service& ieee1349service );
    virtual ~SubUnitInfoCmd();

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "SubUnitInfoCmd"; }

    bool clear();

    page_t           m_page;
    extension_code_t m_extension_code;



    enum {
    eMaxSubunitsPerPage = 4,
    eMaxSubunitsPerUnit = 32,
    };

    struct TableEntry {
    subunit_type_t   m_subunit_type;
    max_subunit_id_t m_max_subunit_id;
    };

    struct TableEntry m_table[eMaxSubunitsPerPage];

    short getMaxNoOfPages()
    { return eMaxSubunitsPerUnit / eMaxSubunitsPerPage; }


    short            m_nrOfValidEntries;
    short getNrOfValidEntries()
    { return m_nrOfValidEntries; }

};

}

#endif // AVCSUBUNITINFO_H
