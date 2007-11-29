/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
#ifndef BEBOB_DL_CODES_H
#define BEBOB_DL_CODES_H

#include "fbtypes.h"

#include "libutil/cmd_serialize.h"

namespace BeBoB {
    enum EBootloaderProtocolVersion {
        eBPV_Unknown = 0,
        eBPV_V1 = 1,
        eBPV_V2 = 2,
        eBPV_V3 = 3,
    };

    enum EBootloaderCommandCodes {
        eCmdC_Halt                       = 0x01,
        eCmdC_Reset                      = 0x02,
        eCmdC_ReadImageCRC               = 0x03,
        eCmdC_DownloadStart              = 0x04,
        eCmdC_DownloadBlock              = 0x05,
        eCmdC_DownloadEnd                = 0x06,
        eCmdC_SwitchTo1394Shell          = 0x07,
        eCmdC_ReadShellChars             = 0x08,
        eCmdC_WriteShellChars            = 0x09,
        eCmdC_ProgramGUID                = 0x0a,
        eCmdC_ProgramMAC                 = 0x0b,
        eCmdC_InitPersParams             = 0x0c,
        eCmdC_InitConfigToFactorySetting = 0x0d,
        eCmdC_SetDebugGUID               = 0x0f,
        eCmdC_ProgramHWIdVersion         = 0x10,
        eCmdC_Go                         = 0x11,
    };

    /////////////////////////

    class CommandCodes {
    public:
        CommandCodes( fb_quadlet_t protocolVersion,
                      fb_byte_t commandCode,
                      size_t    msgSize,
                      fb_byte_t operandSizeRequestField,
                      fb_byte_t operandSizeResponseField );
    virtual ~CommandCodes();

        virtual bool serialize( Util::IOSSerialize& se );
        virtual bool deserialize( Util::IISDeserialize& de );

        virtual size_t getMaxSize();

        EBootloaderCommandCodes getCommandCode() const
            { return static_cast<EBootloaderCommandCodes>( m_commandCode ); }

    fb_byte_t getProtocolVersion() const
        { return m_protocolVersion; }
        size_t getMsgSize() const
            { return m_msgSize; }
        fb_byte_t getOperandSizeRequest() const
            { return m_operandSizeRequest; }
        fb_byte_t getOperandSizeResponse() const
            { return m_operandSizeResponse; }
        unsigned short getCommandId() const
            { return m_commandId; }

        fb_quadlet_t getRespProtocolVersion() const
            { return m_resp_protocolVersion; }
        unsigned short getRespCommandId() const
            { return m_resp_commandId; }
        fb_byte_t getRespCommandCode() const
            { return m_resp_commandCode; }
        fb_byte_t getRespOperandSize() const
            { return m_resp_operandSize; }
        fb_byte_t getRespSizeInQuadlets() const
            { return 2 + m_operandSizeResponse; }

    protected:
        static unsigned short m_gCommandId;
        unsigned short m_commandId;
        fb_quadlet_t m_protocolVersion;
        fb_byte_t m_commandCode;
        size_t    m_msgSize;
        fb_byte_t m_operandSizeRequest;
        fb_byte_t m_operandSizeResponse;

        fb_quadlet_t   m_resp_protocolVersion;
        unsigned short m_resp_commandId;
        fb_byte_t      m_resp_commandCode;
        fb_byte_t      m_resp_operandSize;
    };

    /////////////////////////

    class CommandCodesReset : public CommandCodes {
    public:
        enum EStartMode {
            eSM_Application = 0,
            eSM_Bootloader,
            eSM_Debugger,
        };

        CommandCodesReset( fb_quadlet_t protocolVersion, EStartMode startMode );
        virtual ~CommandCodesReset();

        virtual bool serialize( Util::IOSSerialize& se );
        virtual bool deserialize( Util::IISDeserialize& de );

        EStartMode getStartMode() const
            { return static_cast<EStartMode>( m_startMode ); }
        bool setStartMode( EStartMode startMode )
            { m_startMode = startMode; return true; }

    private:
        fb_byte_t m_startMode;
    };

    /////////////////////////

    class CommandCodesProgramGUID : public CommandCodes {
    public:
        CommandCodesProgramGUID( fb_quadlet_t protocolVersion,
                           fb_octlet_t guid );
        virtual ~CommandCodesProgramGUID();

        virtual bool serialize( Util::IOSSerialize& se );
        virtual bool deserialize( Util::IISDeserialize& de );

        fb_octlet_t getGUID() const
            { return m_guid; }
        bool setGUID( fb_octlet_t guid )
            { m_guid = guid; return true; }

    private:
        fb_octlet_t m_guid;
    };


    /////////////////////////

    class CommandCodesDownloadStart : public CommandCodes {
    public:
        enum EObject {
            eO_Application    = 0,
        eO_Config         = 1,
        eO_Debugger       = 2,
            eO_Bootloader     = 3,
        eO_WarpImage      = 4,
        eO_SerialBootCode = 5,
        };

        CommandCodesDownloadStart( fb_quadlet_t protocolVersion,
                     EObject object );
        virtual ~CommandCodesDownloadStart();

        virtual bool serialize( Util::IOSSerialize& se );
        virtual bool deserialize( Util::IISDeserialize& de );

        bool setDate( fb_octlet_t date )
            { m_date = date; return true; }
        bool setTime( fb_octlet_t time )
            { m_time = time; return true; }
        bool setId( fb_quadlet_t id )
            { m_id = id; return true; }
        bool setVersion( fb_quadlet_t version )
            { m_version = version; return true; }
        bool setBaseAddress( fb_quadlet_t address )
            { m_address = address; return true; }
        bool setLength( fb_quadlet_t length )
            { m_length = length; return true; }
        bool setCRC( fb_quadlet_t crc )
            { m_crc = crc; return true; }

        int getMaxBlockSize() const
            { return m_resp_max_block_size; }

    private:
        fb_quadlet_t m_object;
        fb_octlet_t  m_date;
        fb_octlet_t  m_time;
        fb_quadlet_t m_id;
        fb_quadlet_t m_version;
        fb_quadlet_t m_address;
        fb_quadlet_t m_length;
        fb_quadlet_t m_crc;

        int m_resp_max_block_size;
    };

    /////////////////////////

    class CommandCodesDownloadBlock : public CommandCodes {
    public:
        CommandCodesDownloadBlock( fb_quadlet_t protocolVersion );
        virtual ~CommandCodesDownloadBlock();

        virtual bool serialize( Util::IOSSerialize& se );
        virtual bool deserialize( Util::IISDeserialize& de );

    bool setSeqNumber( fb_quadlet_t seqNumber )
        { m_seqNumber = seqNumber; return true; }
    bool setAddress( fb_quadlet_t address )
            { m_address = address; return true; }
        bool setNumberBytes( fb_quadlet_t numByte )
            { m_numBytes = numByte; return true; }
        fb_quadlet_t getRespSeqNumber() const
            { return m_resp_seqNumber; }
        fb_quadlet_t getRespErrorCode() const
            { return m_resp_errorCode; }
    private:
        fb_quadlet_t m_seqNumber;
        fb_quadlet_t m_address;
        fb_quadlet_t m_numBytes;

    fb_quadlet_t m_resp_seqNumber;
    fb_quadlet_t m_resp_errorCode;
    };

    /////////////////////////

    class CommandCodesDownloadEnd : public CommandCodes {
    public:
        CommandCodesDownloadEnd( fb_quadlet_t protocolVersion );
        virtual ~CommandCodesDownloadEnd();

        virtual bool serialize( Util::IOSSerialize& se );
        virtual bool deserialize( Util::IISDeserialize& de );

        fb_quadlet_t getRespCrc32() const
            { return m_resp_crc32; }
        bool getRespIsValid() const
            { return m_resp_valid == 0; }

    private:
        quadlet_t m_resp_crc32;
        quadlet_t m_resp_valid;
    };


    /////////////////////////

    class CommandCodesInitializePersParam : public CommandCodes {
    public:
        CommandCodesInitializePersParam( fb_quadlet_t protocolVersion );
        virtual ~CommandCodesInitializePersParam();

        virtual bool serialize( Util::IOSSerialize& se );
        virtual bool deserialize( Util::IISDeserialize& de );
    };

    /////////////////////////

    class CommandCodesInitializeConfigToFactorySetting : public CommandCodes {
    public:
        CommandCodesInitializeConfigToFactorySetting(
            fb_quadlet_t protocolVersion );
        virtual ~CommandCodesInitializeConfigToFactorySetting();

        virtual bool serialize( Util::IOSSerialize& se );
        virtual bool deserialize( Util::IISDeserialize& de );
    };

    /////////////////////////

    class CommandCodesGo : public CommandCodes {
    public:
        enum EStartMode {
            eSM_Application = 0,
        eSM_Debugger = 2,
        };

        CommandCodesGo( fb_quadlet_t protocolVersion, EStartMode startMode );
        virtual ~CommandCodesGo();

        virtual bool serialize( Util::IOSSerialize& se );
        virtual bool deserialize( Util::IISDeserialize& de );

        EStartMode getStartMode() const
            { return static_cast<EStartMode>( m_startMode ); }
        bool setStartMode( EStartMode startMode )
            { m_startMode = startMode; return true; }

        fb_quadlet_t getRespIsValidCRC() const
            { return m_resp_validCRC; }

    private:
        fb_quadlet_t m_startMode;
    fb_quadlet_t m_resp_validCRC;
    };



};
#endif
