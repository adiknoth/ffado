/* avc_subunit_info.h
 * Copyright (C) 2005 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef AVCSubUnitInfo_h
#define AVCSubUnitInfo_h

#include "avc_generic.h"

#include <libavc1394/avc1394.h>


// No extended subunit queries supported

class SubUnitInfoCmd: public AVCCommand
{
public:
    SubUnitInfoCmd( Ieee1394Service& ieee1349service );
    virtual ~SubUnitInfoCmd();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

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


#endif // AVCSubUnitInfo_h
