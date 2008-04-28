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

#ifndef AVCGENERIC_H
#define AVCGENERIC_H

#include "../avc_definitions.h"
#include "debugmodule/debugmodule.h"

#include "fbtypes.h"

#include <libavc1394/avc1394.h>

class Ieee1394Service;

namespace Util {
        namespace Cmd {
                class IOSSerialize;
                class IISDeserialize;
        }
};

namespace AVC {

const int fcpFrameMaxLength = 512;
typedef unsigned char fcp_frame_t[fcpFrameMaxLength];

enum EAVCDiscoveryMode {
    eDM_BeBoB        = 0x00,
    eDM_GenericAVC   = 0x01,
    eDM_Invalid      = 0xFF,
};

class IBusData {
public:
    IBusData() {}
    virtual ~IBusData() {}

    virtual bool serialize( Util::Cmd::IOSSerialize& se ) = 0;
    virtual bool deserialize( Util::Cmd::IISDeserialize& de ) = 0;

    virtual IBusData* clone() const = 0;

protected:
    DECLARE_DEBUG_MODULE;
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

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual bool setCommandType( ECommandType commandType );
    virtual bool fire();

    EResponse getResponse();

    bool setNodeId( fb_nodeid_t nodeId );
    bool setSubunitType( ESubunitType subunitType );
    bool setSubunitId( subunit_id_t subunitId );

    ESubunitType getSubunitType();
    subunit_id_t getSubunitId();

    bool setVerbose( int verboseLevel );
    bool setVerboseLevel( int verboseLevel )
        { return setVerbose(verboseLevel);};

    int getVerboseLevel();

    virtual const char* getCmdName() const = 0;

    // workaround
    static void setSleepAfterAVCCommand( int time );
    static int getSleepAfterAVCCommand( ) {return m_time;};
protected:
    void showFcpFrame( const unsigned char* buf,
               unsigned short frameSize ) const;

protected:
    AVCCommand( Ieee1394Service& ieee1394service, opcode_t opcode );
    virtual ~AVCCommand() {}

    ECommandType getCommandType();

    Ieee1394Service* m_p1394Service;
    fb_nodeid_t      m_nodeId;

    fcp_frame_t      m_fcpFrame;

private:
    ctype_t      m_ctype;
    subunit_t    m_subunit;
    opcode_t     m_opcode;
    EResponse    m_eResponse;
    ECommandType m_commandType;
    static int   m_time;
    
protected:
    DECLARE_DEBUG_MODULE;
};


const char* subunitTypeToString( subunit_type_t subunitType );
const char* responseToString( AVCCommand::EResponse eResponse );

}

#endif // AVCGENERIC_H
