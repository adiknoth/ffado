/*
 * Copyright (C) 2009 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef GENERICAVC_STANTON_SCS_DEVICE_H
#define GENERICAVC_STANTON_SCS_DEVICE_H

#include "genericavc/avc_avdevice.h"
#include "libieee1394/ieee1394service.h"

namespace Util {
    class Functor;
}

namespace GenericAVC {
namespace Stanton {

#define HSS1394_BASE_ADDRESS            0x0000c007dedadadaULL
#define HSS1394_RESPONSE_ADDRESS        0x0000c007E0000000ULL
#define HSS1394_MAX_PACKET_SIZE         63
#define HSS1394_INVALID_NETWORK_ID      0xff
#define HSS1394_MAX_RETRIES             32

#define HSS1394_CMD_USER_DATA           0x00
#define HSS1394_CMD_DEBUG_DATA          0x01
#define HSS1394_CMD_USER_TAG_BASE       0x10
#define HSS1394_CMD_USER_TAG_TOP        0xEF
#define HSS1394_CMD_RESET               0xF0
#define HSS1394_CMD_CHANGE_ADDRESS      0xF1
#define HSS1394_CMD_PING                0xF2
#define HSS1394_CMD_PING_RESPONSE       0xF3
#define HSS1394_CMD_ECHO_AS_USER_DATA   0xF4
#define HSS1394_CMD_UNDEFINED           0xFF

class ScsDevice : public GenericAVC::Device {
public:
    class HSS1394Handler;

    enum eMessageType {
        eMT_UserData = HSS1394_CMD_USER_DATA,
        eMT_DebugData = HSS1394_CMD_DEBUG_DATA,
        eMT_UserTagBase = HSS1394_CMD_USER_TAG_BASE,
        eMT_UserTagTop = HSS1394_CMD_USER_TAG_TOP,
        eMT_Reset = HSS1394_CMD_RESET,
        eMT_ChangeAddress = HSS1394_CMD_CHANGE_ADDRESS,
        eMT_Ping = HSS1394_CMD_PING,
        eMT_PingResponse = HSS1394_CMD_PING_RESPONSE,
        eMT_EchoAsUserData = HSS1394_CMD_ECHO_AS_USER_DATA,
        eMT_Undefined = HSS1394_CMD_UNDEFINED,
    };

public:
    ScsDevice( DeviceManager& d,
               std::auto_ptr<ConfigRom>( configRom ));
    virtual ~ScsDevice();

    virtual void showDevice();

    virtual bool initMessageHandler();
    bool discover();

    bool writeHSS1394Message(enum eMessageType message_type, byte_t*, size_t);

// private: // FIXME: should be private!!!
    HSS1394Handler *m_hss1394handler;
private:

    bool writeRegBlock(fb_nodeaddr_t addr, fb_quadlet_t *data, size_t length, size_t blocksize_quads=32);
    bool readRegBlock(fb_nodeaddr_t addr, fb_quadlet_t *data, size_t length, size_t blocksize_quads=32);

public:
    class HSS1394Handler : public Ieee1394Service::ARMHandler
    {
    public:
        class MessageFunctor
        {
        public:
            MessageFunctor() {};
            virtual ~MessageFunctor() {}
            virtual void operator() (byte_t *, size_t len) = 0;
        };

        typedef std::vector<MessageFunctor*> MessageHandlerVector;
        typedef std::vector<MessageFunctor*>::iterator MessageHandlerVectorIterator;

    public:
        HSS1394Handler(Device &, nodeaddr_t start);
        virtual ~HSS1394Handler();

        virtual bool handleRead(struct raw1394_arm_request  *);
        virtual bool handleWrite(struct raw1394_arm_request  *);
        virtual bool handleLock(struct raw1394_arm_request  *);

        bool addMessageHandler(enum eMessageType message_type, MessageFunctor* functor);
        bool removeMessageHandler(enum eMessageType message_type, MessageFunctor* functor);
    private:
        Device &m_device;
        MessageHandlerVector m_userDataMessageHandlers;

    private:
        enum eMessageType byteToMessageType(byte_t);
    };
};

}
}

#endif // GENERICAVC_STANTON_SCS_DEVICE_H
