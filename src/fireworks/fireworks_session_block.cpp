/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#include "fireworks_session_block.h"
#include "fireworks_device.h"

#include "libutil/ByteSwap.h"

#include <fstream>
// These classes provide support for reading/writing session blocks on
// echo fireworks based devices

// does not support legacy (v1) sessions

using namespace std;

namespace FireWorks {

IMPL_DEBUG_MODULE( Session, Session, DEBUG_LEVEL_NORMAL );

Session::Session()
{
}

Session::~Session()
{
}

bool
Session::loadFromDevice(Device &d)
{
    size_t len = sizeof(SessionHeader) + sizeof(SubSession);
    size_t start = d.getSessionBase();
    if (start == 0) {
        debugError("Invalid session base\n");
        return false;
    }

    uint32_t data[len/4];
    if(!d.readFlash(start, len/4, data)) {
        debugError("Flash read failed\n");
        return false;
    }

    if(!loadFromMemory(data, len)) {
        debugError("Could not load session block from device memory dump\n");
        return false;
    }
    return true;
}

bool
Session::saveToDevice(Device &d)
{
    size_t len = sizeof(SessionHeader) + sizeof(SubSession);
    size_t start = d.getSessionBase();
    if (start == 0) {
        debugError("Invalid session base\n");
        return false;
    }

    debugWarning("CRC update not implemented, will fail\n");

    uint32_t data[len/4];
    if(!saveToMemory(data, len)) {
        debugError("Could not save session to memory block\n");
        return false;
    }

    if (!d.lockFlash(true)) {
        debugError("  Could not lock flash\n");
        return false;
    }

    if (!d.eraseFlashBlocks(start, len/4)) {
        debugError("  Could not erase memory\n");
        return false;
    }

    if(!d.writeFlash(start, len/4, data)) {
        debugError("Writing to flash failed.\n");
        return false;
    }

    if (!d.lockFlash(false)) {
        debugError("  Could not unlock flash\n");
        return false;
    }

    return true;
}

bool
Session::loadFromFile(std::string filename)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Loading session from file %s\n", filename.c_str());
    fstream sessfile;
    
    debugOutput(DEBUG_LEVEL_VERBOSE, " Loading file...\n");
    sessfile.open( filename.c_str(), ios::in | ios::ate | ios::binary);
    if ( !sessfile.is_open() ) {
        debugError("Could not open file.\n");
        return false;
    }
    // get file size
    int size;
    size = (int)sessfile.tellg() - ECHO_SESSION_FILE_START_OFFSET;

    sessfile.seekg(ECHO_SESSION_FILE_START_OFFSET, ios_base::beg);
    debugOutput(DEBUG_LEVEL_VERBOSE, " Reading data, size = %d bytes, %d quads...\n", size, size/4);
    char data[size];
    sessfile.read(data, size);
    sessfile.close();
    if (sessfile.eof()) {
        debugError("EOF while reading file\n");
        return false;
    }
    
    if(!loadFromMemory(data, size)) {
        debugError("Could not load session block from file\n");
        return false;
    }
    return true;
}

bool
Session::saveToFile(std::string filename)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Saving session to file %s\n", filename.c_str());
    fstream sessfile;

    debugOutput(DEBUG_LEVEL_VERBOSE, " Loading file...\n");
    sessfile.open( filename.c_str(), ios::out | ios::trunc | ios::binary);
    if ( !sessfile.is_open() ) {
        debugError("Could not open file.\n");
        return false;
    }

    // FIXME: figure out what the file header means
    debugOutput(DEBUG_LEVEL_VERBOSE, " Writing file header...\n");
    char header[ECHO_SESSION_FILE_START_OFFSET];
    sessfile.write(header, ECHO_SESSION_FILE_START_OFFSET);

    debugOutput(DEBUG_LEVEL_VERBOSE, " Writing session data...\n");
    int size = sizeof(SessionHeader) + sizeof(SubSession);
    char data[size];
    if(!saveToMemory(data, size)) {
        debugError("Could not save session to memory block\n");
        return false;
    }
    sessfile.write(data, size);
    sessfile.close();
    return true;
}

bool
Session::loadFromMemory(void *buff, size_t len)
{
    if (len != sizeof(SessionHeader) + sizeof(SubSession)) {
        debugError("Invalid session length\n");
        return false;
    }
    char *raw = (char *)buff;
    memcpy(&h, raw, sizeof(SessionHeader));
    memcpy(&s, raw+sizeof(SessionHeader), sizeof(SubSession));

    if (len != h.size_quads*4) {
        debugWarning("size not correct: got %d, should be %d according to data\n", len, h.size_quads*4);
    }

#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned int i=0;
    uint32_t *data = (uint32_t *)(&s);
    for(; i < sizeof(SubSession)/4; i++) {
        *data = ByteSwap32(*data);
        data++;
    }
#endif

    return true;
}

bool
Session::saveToMemory(void *buff, size_t max_len)
{
    if (max_len < sizeof(SessionHeader) + sizeof(SubSession)) {
        debugError("Max length too small\n");
        return false;
    }
    char *raw = (char *)buff;
    memcpy(raw, &h, sizeof(SessionHeader));
    memcpy(raw+sizeof(SessionHeader), &s, sizeof(SubSession));

#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned int i=0;
    uint32_t *data = (uint32_t *)(raw+sizeof(SessionHeader));
    for(; i < sizeof(SubSession)/4; i++) {
        *data = ByteSwap32(*data);
        data++;
    }
#endif

    return true;
}

uint32_t
Session::calculateCRC()
{
    debugWarning("unimplemented\n");
    return 0;
}

void
Session::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Session Block\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Size.............: %u (%08X)\n", h.size_quads, h.size_quads);
    debugOutput(DEBUG_LEVEL_NORMAL, " CRC..............: %u (%08X)\n", h.crc, h.crc);
    debugOutput(DEBUG_LEVEL_NORMAL, " Version..........: %u (%08X)\n", h.version, h.version);
    debugOutput(DEBUG_LEVEL_NORMAL, " Flags............: %u (%08X)\n", h.flags, h.flags);
    debugOutput(DEBUG_LEVEL_NORMAL, " Mirror Channel...: %d (%08X)\n", h.mirror_channel, h.mirror_channel);
    debugOutput(DEBUG_LEVEL_NORMAL, " Digital Mode.....: %d (%08X)\n", h.digital_mode, h.digital_mode);
    debugOutput(DEBUG_LEVEL_NORMAL, " Clock............: %d (%08X)\n", h.clock, h.clock);
    debugOutput(DEBUG_LEVEL_NORMAL, " Rate.............: %d (%08X)\n", h.rate, h.rate);

    debugOutput(DEBUG_LEVEL_NORMAL, " Gains:\n");
    for(unsigned int in = 0; in < ECHO_SESSION_MAX_PHY_AUDIO_IN; in++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "  MON %02u: ", in);
        for(unsigned int out = 0; out < ECHO_SESSION_MAX_PHY_AUDIO_OUT; out++) {
            debugOutputShort(DEBUG_LEVEL_NORMAL, "%08X ", h.monitorgains[in][out]);
            flushDebugOutput();
        }
        debugOutputShort(DEBUG_LEVEL_NORMAL, "\n");
    }

    debugOutput(DEBUG_LEVEL_NORMAL, "  PGAIN : ");
    for(unsigned int out = 0; out < ECHO_SESSION_MAX_PHY_AUDIO_OUT; out++) {
        debugOutputShort(DEBUG_LEVEL_NORMAL, "%08X ", h.playbackgains[out]);
        flushDebugOutput();
    }
    debugOutputShort(DEBUG_LEVEL_NORMAL, "\n");

    debugOutput(DEBUG_LEVEL_NORMAL, "  OGAIN : ");
    for(unsigned int out = 0; out < ECHO_SESSION_MAX_PHY_AUDIO_OUT; out++) {
        debugOutputShort(DEBUG_LEVEL_NORMAL, "%08X ", h.outputgains[out]);
        flushDebugOutput();
    }
    debugOutputShort(DEBUG_LEVEL_NORMAL, "\n");

    debugOutput(DEBUG_LEVEL_NORMAL, " Input settings:\n");
    for(unsigned int in = 0; in < ECHO_SESSION_MAX_PHY_AUDIO_IN; in++) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "  IN %02u: shift: %02X, pad: %02X, label: %s\n",
                    in, s.inputs[in].shift, s.inputs[in].pad, s.inputs[in].label);
        flushDebugOutput();
    }

    debugOutput(DEBUG_LEVEL_NORMAL, " Pans:\n");
    for(unsigned int in = 0; in < ECHO_SESSION_MAX_PHY_AUDIO_IN; in++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "  IN %02u: ", in);
        for(unsigned int out = 0; out < ECHO_SESSION_MAX_PHY_AUDIO_OUT; out++) {
            debugOutputShort(DEBUG_LEVEL_NORMAL, "%03u ", s.monitorpans[in][out]);
            flushDebugOutput();
        }
        debugOutputShort(DEBUG_LEVEL_NORMAL, "\n");
    }
    debugOutput(DEBUG_LEVEL_NORMAL, " Flags:\n");
    for(unsigned int in = 0; in < ECHO_SESSION_MAX_PHY_AUDIO_IN; in++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "  IN %02u: ", in);
        for(unsigned int out = 0; out < ECHO_SESSION_MAX_PHY_AUDIO_OUT; out++) {
            debugOutputShort(DEBUG_LEVEL_NORMAL, "%02X ", s.monitorflags[in][out]);
            flushDebugOutput();
        }
        debugOutputShort(DEBUG_LEVEL_NORMAL, "\n");
    }

    debugOutput(DEBUG_LEVEL_NORMAL, " Playback settings:\n");
    for(unsigned int out = 0; out < ECHO_SESSION_MAX_PHY_AUDIO_OUT; out++) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "  PBK %02u: mute: %02X, solo: %02X, label: %s\n",
                    out, s.playbacks[out].mute, s.playbacks[out].solo, s.playbacks[out].label);
    }
    debugOutput(DEBUG_LEVEL_NORMAL, " Output settings:\n");
    for(unsigned int out = 0; out < ECHO_SESSION_MAX_PHY_AUDIO_OUT; out++) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "  OUT %02u: mute: %02X, shift: %02X, label: %s\n",
                    out, s.outputs[out].mute, s.outputs[out].shift, s.outputs[out].label);
        flushDebugOutput();
    }

}

void
Session::dumpData()
{

}

} // FireWorks
