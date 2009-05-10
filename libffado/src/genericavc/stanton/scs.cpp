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

#include "scs.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libutil/ByteSwap.h"
#include "libutil/Functors.h"

namespace GenericAVC {
namespace Stanton {

ScsDevice::ScsDevice( DeviceManager& d,
                      std::auto_ptr<ConfigRom>( configRom ))
: GenericAVC::Device( d , configRom)
, m_hss1394handler( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created GenericAVC::Stanton::ScsDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

ScsDevice::~ScsDevice()
{
    if (m_hss1394handler) {
        get1394Service().unregisterARMHandler(m_hss1394handler);
        delete m_hss1394handler;
        m_hss1394handler = NULL;
    }
}

bool
ScsDevice::discover()
{
    if(!initMessageHandler()) {
        debugError("Could not initialize HSS1394 message handler\n");
        return false;
    }
    return Device::discover();
}

bool
ScsDevice::initMessageHandler() {
    fb_nodeaddr_t addr = HSS1394_BASE_ADDRESS;
    quadlet_t cmdBuffer[2];

    memset(cmdBuffer, 0, sizeof(cmdBuffer));
    // read the current value present in the register, i.e. read-ping
    if(!readRegBlock(addr, (quadlet_t *)cmdBuffer, 1) ) {
        debugError("Could not read from addr 0x%012llX\n",  addr);
    } else {
        int version = cmdBuffer[0] & 0xFFFF;
        debugOutput(DEBUG_LEVEL_VERBOSE, "Read Ping response: %08X, Version: %d\n", cmdBuffer[0], version);
        if((cmdBuffer[0] >> 24 & 0xFF) != HSS1394_CMD_PING_RESPONSE) {
            debugWarning("Bogus device response to ping! (%08X)\n", cmdBuffer[0]);
        }
    }

    // do a write ping
    memset(cmdBuffer, 0, sizeof(cmdBuffer));
    cmdBuffer[0] |= HSS1394_CMD_PING << 24;

    // execute the command
    if(!writeRegBlock(addr, (quadlet_t *)cmdBuffer, 1)) {
        debugError("Could not write to addr 0x%012llX\n",  addr);
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Write Ping succeeded\n");
    }

    // get a notifier to handle device notifications
    nodeaddr_t notify_address;
    notify_address = get1394Service().findFreeARMBlock(
                        HSS1394_RESPONSE_ADDRESS,
                        HSS1394_MAX_PACKET_SIZE+1,
                        HSS1394_MAX_PACKET_SIZE+1);

    if (notify_address == 0xFFFFFFFFFFFFFFFFLLU) {
        debugError("Could not find free ARM block for notification\n");
        return false;
    }

    m_hss1394handler = new ScsDevice::HSS1394Handler(*this, notify_address);

    if(!m_hss1394handler) {
        debugError("Could not allocate notifier\n");
        return false;
    }

    if (!get1394Service().registerARMHandler(m_hss1394handler)) {
        debugError("Could not register HSS1394 handler\n");
        delete m_hss1394handler;
        m_hss1394handler = NULL;
        return false;
    }

    // configure device to use the allocated ARM address for communication
    // the address change command
    cmdBuffer[0] = 0;
    cmdBuffer[0] |= HSS1394_CMD_CHANGE_ADDRESS << 24;

    // the address is the argument
    cmdBuffer[0] |= (notify_address >> 32) & 0xFFFF;
    cmdBuffer[1] = notify_address & 0xFFFFFFFF;

    // execute the command
    if(!writeRegBlock(addr, (quadlet_t *)cmdBuffer, 2)) {
        debugError("Could not write to addr 0x%012llX\n", addr);
        return false;
    }

    // request the device to echo something
    cmdBuffer[0] = 0;
    cmdBuffer[0] |= HSS1394_CMD_ECHO_AS_USER_DATA << 24;

    // the address is the argument
    cmdBuffer[0] |= 0x1234;
    cmdBuffer[1] = 0x56789ABC;

    // execute the command
    if(!writeRegBlock(addr, (quadlet_t *)cmdBuffer, 2)) {
        debugError("Could not write to addr 0x%012llX\n", addr);
        return false;
    }

    return true;
}

bool
ScsDevice::writeHSS1394Message(enum eMessageType message_type, byte_t* buffer, size_t len) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing message type: %02X, length: %u bytes\n",
        message_type, len);
    size_t len_quadlets = len/4 + 1;

    fb_nodeaddr_t addr = HSS1394_BASE_ADDRESS;

    unsigned char tmpbuffer[len_quadlets*4];
    tmpbuffer[0] = message_type;
    memcpy(tmpbuffer+1, buffer, len);
//     memcpy(tmpbuffer, buffer, len);

    quadlet_t *cmdBuffer = (quadlet_t *)tmpbuffer;

    hexDump(tmpbuffer, len_quadlets*4);
    // we have to cond-byte-swap this because the memory stuff is assumed to be
    // in big-endian
    byteSwapFromBus(cmdBuffer, len_quadlets);

    // execute the command
    if(!writeRegBlock(addr, (quadlet_t *)cmdBuffer, len_quadlets)) {
        debugError("Could not write to addr 0x%012llX\n", addr);
        return false;
    }
    return true;
}

bool
ScsDevice::readRegBlock(fb_nodeaddr_t addr, fb_quadlet_t *data, size_t length_quads, size_t blocksize_quads) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"Reading register 0x%016llX, length %u quadlets, to %p\n",
        addr, length_quads, data);

    fb_nodeid_t nodeId = getNodeId() | 0xFFC0;
    int quads_done = 0;
    while(quads_done < (int)length_quads) {
        fb_nodeaddr_t curr_addr = addr + quads_done*4;
        fb_quadlet_t *curr_data = data + quads_done;
        int quads_todo = length_quads - quads_done;
        if (quads_todo > (int)blocksize_quads) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Truncating read from %d to %d quadlets\n", quads_todo, blocksize_quads);
            quads_todo = blocksize_quads;
        }
        #ifdef DEBUG
        if (quads_todo < 0) {
            debugError("BUG: quads_todo < 0: %d\n", quads_todo);
        }
        #endif

        debugOutput(DEBUG_LEVEL_VERBOSE, "reading addr: 0x%016llX, %d quads to %p\n", curr_addr, quads_todo, curr_data);
        if(!get1394Service().read( nodeId, curr_addr, quads_todo, curr_data ) ) {
            debugError("Could not read %d quadlets from node 0x%04X addr 0x%012llX\n", quads_todo, nodeId, curr_addr);
            return false;
        }

        quads_done += quads_todo;
    }
    byteSwapFromBus(data, length_quads);
    return true;
}

bool
ScsDevice::writeRegBlock(fb_nodeaddr_t addr, fb_quadlet_t *data, size_t length_quads, size_t blocksize_quads) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing register 0x%016llX, length: %u quadlets, from %p\n",
        addr, length_quads, addr);

    fb_quadlet_t data_out[length_quads];
    memcpy(data_out, data, length_quads*4);
    byteSwapToBus(data_out, length_quads);

    fb_nodeid_t nodeId = getNodeId() | 0xFFC0;
    int quads_done = 0;
    while(quads_done < (int)length_quads) {
        fb_nodeaddr_t curr_addr = addr + quads_done*4;
        fb_quadlet_t *curr_data = data_out + quads_done;
        int quads_todo = length_quads - quads_done;
        if (quads_todo > (int)blocksize_quads) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Truncating write from %d to %d quadlets\n", quads_todo, blocksize_quads);
            quads_todo = blocksize_quads;
        }
        #ifdef DEBUG
        if (quads_todo < 0) {
            debugError("BUG: quads_todo < 0: %d\n", quads_todo);
        }
        #endif

        debugOutput(DEBUG_LEVEL_VERBOSE, "writing addr: 0x%016llX, %d quads from %p\n", curr_addr, quads_todo, curr_data);
        if(!get1394Service().write( nodeId, addr, quads_todo, curr_data ) ) {
            debugError("Could not write %d quadlets to node 0x%04X addr 0x%012llX\n", quads_todo, nodeId, curr_addr);
            return false;
        }
        quads_done += quads_todo;
    }

    return true;
}

void
ScsDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a GenericAVC::Stanton::ScsDevice\n");
    GenericAVC::Device::showDevice();
}

// the incoming HSS1394 handler

ScsDevice::HSS1394Handler::HSS1394Handler(Device &d, nodeaddr_t start)
 : ARMHandler(d.get1394Service(), start, HSS1394_MAX_PACKET_SIZE+1,
              RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
              RAW1394_ARM_WRITE, 0)
 , m_device(d)
{
    // switch over the debug module to that of this device instead of the 1394 service
    m_debugModule = d.m_debugModule;

}

ScsDevice::HSS1394Handler::~HSS1394Handler()
{
}

bool
ScsDevice::HSS1394Handler::handleRead(struct raw1394_arm_request *arm_req) {
    debugWarning("Unexpected Read transaction received\n");
    printRequest(arm_req);
    return true;
}

bool
ScsDevice::HSS1394Handler::handleWrite(struct raw1394_arm_request *arm_req) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "HSS Write\n");
    printRequest(arm_req);
    size_t payload_len = 0;
    enum eMessageType message_type = eMT_Undefined;

    // extract the data
    if(arm_req->buffer_length > 1) {
        message_type = byteToMessageType(arm_req->buffer[0]);
        payload_len = arm_req->buffer_length - 1;
    } else {
        debugWarning("Received empty message\n");
        return false;
    }

    // figure out what handler to call
    switch(message_type) {
        case eMT_UserData:
            for ( MessageHandlerVectorIterator it = m_userDataMessageHandlers.begin();
                it != m_userDataMessageHandlers.end();
                ++it )
            {
                MessageFunctor* func = *it;
                debugOutput(DEBUG_LEVEL_VERBOSE, "Calling functor %p\n", func);
                ( *func )(arm_req->buffer + 1, payload_len);
            }
            break;
        case eMT_DebugData:
        case eMT_UserTagBase:
        case eMT_UserTagTop:
        case eMT_Reset:
        case eMT_ChangeAddress:
        case eMT_Ping:
        case eMT_PingResponse:
        case eMT_EchoAsUserData:
        case eMT_Undefined:
        default:
            debugWarning("Unexpected Message of type: %02X\n", message_type);
    }
    return true;
}

bool
ScsDevice::HSS1394Handler::handleLock(struct raw1394_arm_request *arm_req) {
    debugWarning("Unexpected Lock transaction received\n");
    printRequest(arm_req);
    return true;
}

bool
ScsDevice::HSS1394Handler::addMessageHandler(enum eMessageType message_type, MessageFunctor* functor)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Adding Message handler (%p) for message type %02X\n", functor, message_type);
    // figure out what handler to call
    switch(message_type) {
        case eMT_UserData:
            // FIXME: one handler can be added multiple times, is this what we want?
            m_userDataMessageHandlers.push_back( functor );
            return true;
        case eMT_DebugData:
        case eMT_UserTagBase:
        case eMT_UserTagTop:
        case eMT_Reset:
        case eMT_ChangeAddress:
        case eMT_Ping:
        case eMT_PingResponse:
        case eMT_EchoAsUserData:
        case eMT_Undefined:
        default:
            debugError("Handlers not supported for messages of type: %02X\n", message_type);
            return false;
    }
}

bool
ScsDevice::HSS1394Handler::removeMessageHandler(enum eMessageType message_type, MessageFunctor* functor)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Removing Message handler (%p) for message type %02X\n", functor, message_type);
    // figure out what handler to call
    switch(message_type) {
        case eMT_UserData:
            // FIXME: one handler can be present multiple times, how do we handle this?
            for ( MessageHandlerVectorIterator it = m_userDataMessageHandlers.begin();
                it != m_userDataMessageHandlers.end();
                ++it )
            {
                if ( *it == functor ) {
                    debugOutput(DEBUG_LEVEL_VERBOSE, " found\n");
                    m_userDataMessageHandlers.erase( it );
                    return true;
                }
            }
            debugOutput(DEBUG_LEVEL_VERBOSE, " not found\n");
            return false;
        case eMT_DebugData:
        case eMT_UserTagBase:
        case eMT_UserTagTop:
        case eMT_Reset:
        case eMT_ChangeAddress:
        case eMT_Ping:
        case eMT_PingResponse:
        case eMT_EchoAsUserData:
        case eMT_Undefined:
        default:
            debugError("Handlers not supported for messages of type: %02X\n", message_type);
            return false;
    }
}

enum ScsDevice::eMessageType
ScsDevice::HSS1394Handler::byteToMessageType(byte_t tag)
{
    switch(tag) {
        case HSS1394_CMD_USER_DATA:
            return eMT_UserData;
        case HSS1394_CMD_DEBUG_DATA:
            return eMT_DebugData;
        case HSS1394_CMD_USER_TAG_BASE:
            return eMT_UserTagBase;
        case HSS1394_CMD_USER_TAG_TOP:
            return eMT_UserTagTop;
        case HSS1394_CMD_RESET:
            return eMT_Reset;
        case HSS1394_CMD_CHANGE_ADDRESS:
            return eMT_ChangeAddress;
        case HSS1394_CMD_PING:
            return eMT_Ping;
        case HSS1394_CMD_PING_RESPONSE:
            return eMT_PingResponse;
        case HSS1394_CMD_ECHO_AS_USER_DATA:
            return eMT_EchoAsUserData;
        case HSS1394_CMD_UNDEFINED:
        default:
            return eMT_Undefined;
    }
}

}
}
