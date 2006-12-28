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

    enum ESubunitType {
        eST_Monitor       = AVC1394_SUBUNIT_VIDEO_MONITOR,
        eST_Audio         = AVC1394_SUBUNIT_AUDIO,
        eST_Printer       = AVC1394_SUBUNIT_PRINTER,
        eST_Disc          = AVC1394_SUBUNIT_DISC_RECORDER,
        eST_VCR           = AVC1394_SUBUNIT_VCR,
        eST_Tuner         = AVC1394_SUBUNIT_TUNER,
        eST_CA            = AVC1394_SUBUNIT_CA,
        eST_Camera        = AVC1394_SUBUNIT_VIDEO_CAMERA,
        eST_Panel         = AVC1394_SUBUNIT_PANEL,
        eST_BulltinBoard  = AVC1394_SUBUNIT_BULLETIN_BOARD,
        eST_CameraStorage = AVC1394_SUBUNIT_CAMERA_STORAGE,
        eST_Music         = AVC1394_SUBUNIT_MUSIC,
        eST_VendorUnique  = AVC1394_SUBUNIT_VENDOR_UNIQUE,
        eST_Reserved      = AVC1394_SUBUNIT_RESERVED,
        eST_Extended      = AVC1394_SUBUNIT_EXTENDED,
        eST_Unit          = AVC1394_SUBUNIT_UNIT,
    };

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual bool setCommandType( ECommandType commandType );
    virtual bool fire(raw1394handle_t handle,
                      unsigned int node_id ) = 0;

    EResponse getResponse();

    bool setSubunitType(ESubunitType subunitType);
    bool setSubunitId(subunit_id_t subunitId);

    ESubunitType getSubunitType();
    subunit_id_t getSubunitId();

    bool setVerbose( bool enable );
    bool isVerbose();


protected:
    AVCCommand( opcode_t opcode );
    virtual ~AVCCommand() {}

    bool parseResponse( byte_t response );
    ECommandType getCommandType();

private:
    ctype_t      m_ctype;
    subunit_t    m_subunit;
    opcode_t     m_opcode;
    EResponse    m_eResponse;
    bool         m_verbose;
    ECommandType m_commandType;
};

#endif // AVCGeneric_h
