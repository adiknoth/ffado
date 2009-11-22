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

const uint32_t ECHO_SESSION_CRC_TABLE[256] =
{
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

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

    // update the CRC
    h.crc = calculateCRC();

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
        debugWarning("size not correct: got %zd, should be %d according to data\n", len, h.size_quads*4);
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
    size_t len = sizeof(SessionHeader) + sizeof(SubSession);
    char data[len];
    memcpy(data, &h, sizeof(SessionHeader));
    memcpy(data+sizeof(SessionHeader), &s, sizeof(SubSession));
    return calculateCRC(data, len);
}

uint32_t
Session::calculateCRC(void *memblock, size_t max_len)
{
    if (max_len < sizeof(SessionHeader) + sizeof(SubSession)) {
        debugError("block too small\n");
        return 0;
    }

    // based upon CRC code provided by ECHO
    byte_t *data = (byte_t*)memblock;
    uint32_t b;
    int bytecount;
    uint32_t remainder = ECHO_SESSION_CRC_INITIAL_REMAINDER;

    // skip the first two fields (length and CRC)
    data += ECHO_SESSION_CRC_START_OFFSET_BYTES;

    // total bytecount
    bytecount = sizeof(SessionHeader) + sizeof(SubSession)
                - ECHO_SESSION_CRC_START_OFFSET_BYTES;

#if __BYTE_ORDER == __BIG_ENDIAN
    //
    // This is a big-endian machine; calculate the CRC for the first part
    // of the data structure using big-endian mode
    //
    int bebytes, offset;

    offset = 3;
    bebytes = bytecount - sizeof(SubSession);
    while (bebytes > 0)
    {
        b = data[offset];
        b = ( remainder ^ b) & 0xFF;
        remainder = ECHO_SESSION_CRC_TABLE[b] ^ ( remainder >> 8 );

        offset--;
        if (offset < 0)
        {
            offset = 3;
            bebytes -= sizeof(uint32_t);
            data += sizeof(uint32_t);
        }
    }
#endif

    //
    // Do the little-endian part of the session
    //
    // Compute the CRC a byte at time, starting with the LSB of each quad
    //
    while (bytecount > 0)
    {
        b = ( remainder ^ *data ) & 0xFF;
        remainder = ECHO_SESSION_CRC_TABLE[b] ^ ( remainder >> 8 );

        data++;
        bytecount--;
    }

    //
    // The final remainder is the CRC result.
    //
    return (remainder ^ ECHO_SESSION_CRC_FINAL_XOR_VALUE);
}

void
Session::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Session Block\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Size.............: %u (%08X)\n", h.size_quads, h.size_quads);
    debugOutput(DEBUG_LEVEL_NORMAL, " CRC read.........: %12u (%08X)\n", h.crc, h.crc);
    uint32_t crc = calculateCRC();
    debugOutput(DEBUG_LEVEL_NORMAL, " CRC calculated...: %12u (%08X)\n", crc, crc);
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
