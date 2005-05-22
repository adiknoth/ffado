/* avc_generic.h
 * Copyright (C) 2005 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef AVCGeneric_h
#define AVCGeneric_h

#include "avc_definitions.h"

#include <libavc1394/avc1394.h>

class IOSSerialize;
class IISDeserialize;

class IBusData {
public:
    virtual bool serialize( IOSSerialize& se ) = 0;
    virtual bool deserialize( IISDeserialize& de ) = 0;

    virtual IBusData* clone() const = 0;
};

class AVCCommand
{
public:
    enum EResponse {
        eR_Unknown        = 0,
        eR_NotImplemented = AVC1394_RESP_NOT_IMPLEMENTED,
        eR_Accepted       = AVC1394_RESP_ACCEPTED,
        eR_Rejected       = AVC1394_RESP_REJECTED,
        eR_InTransition   = AVC1394_RESP_IN_TRANSITION,
        eR_Implemented    = AVC1394_RESP_IMPLEMENTED,
        eR_Changed        = AVC1394_RESP_CHANGED,
        eR_Interim        = AVC1394_RESP_INTERIM,
    };

    enum ECommandType {
        eCT_Control         = AVC1394_CTYP_CONTROL,
        eCT_Status          = AVC1394_CTYP_STATUS,
        eCT_SpecificInquiry = AVC1394_CTYP_SPECIFIC_INQUIRY,
        eCT_Notify          = AVC1394_CTYP_NOTIFY,
        eCT_GeneralInquiry  = AVC1394_CTYP_GENERAL_INQUIRY,
        eCT_Unknown         = 0xff,
    };
    typedef byte_t subfunction_t;
    typedef byte_t opcode_t;

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual bool fire(ECommandType commandType,
                      raw1394handle_t handle,
                      unsigned int node_id ) = 0;

    EResponse getResponse();

    typedef byte_t subunit_type_t;
    typedef byte_t subunit_id_t;

    bool setSubunitType(subunit_type_t subunitType);
    bool setSubunitId(subunit_id_t subunitId);

    bool setVerbose( bool enable );
    bool isVerbose();
protected:
    AVCCommand( opcode_t opcode );
    virtual ~AVCCommand() {}

    bool parseResponse( byte_t response );
    bool setCommandType( ECommandType commandType );

private:
    ctype_t   m_ctype;
    subunit_t m_subunit;
    opcode_t  m_opcode;
    EResponse m_eResponse;
    bool      m_verbose;
};

#endif // AVCGeneric_h
