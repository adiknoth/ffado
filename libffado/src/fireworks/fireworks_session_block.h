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

#ifndef FIREWORKS_SESSION_BLOCK_H
#define FIREWORKS_SESSION_BLOCK_H

#include "debugmodule/debugmodule.h"
#include <string>

#include "efc/efc_cmds_ioconfig.h"

namespace FireWorks {

#define ECHO_SESSION_FILE_START_OFFSET 0x0040

// we only support version 2
#define SESSION_VERSION 0x00000200

#define ECHO_SESSION_MAX_PHY_AUDIO_IN    40
#define ECHO_SESSION_MAX_PHY_AUDIO_OUT   40
#define ECHO_SESSION_MAX_1394_REC_CHAN   40
#define ECHO_SESSION_MAX_1394_PLAY_CHAN  40
#define ECHO_SESSION_MAX_LABEL_SIZE      22

#define ECHO_SESSION_MUTE_BIT        1
#define ECHO_SESSION_SOLO_BIT        2
#define ECHO_SESSION_PAD_BIT         4

// Phantom power bit is set in the "flags"
#define ECHO_SESSION_FLAG_PHANTOM_POWER_BIT        31
#define ECHO_SESSION_FLAG_PHANTOM_POWER            (1 << ECHO_SESSION_FLAG_PHANTOM_POWER_BIT)

// CRC codes
#define ECHO_SESSION_CRC_START_OFFSET_BYTES    ( sizeof(uint32_t) * 2 )
#define ECHO_SESSION_CRC_START_OFFSET_QUADS    ( ECHO_SESSION_CRC_START_OFFSET_BYTES/sizeof(uint32_t) )
#define ECHO_SESSION_CRC_INITIAL_REMAINDER   0xFFFFFFFF
#define ECHO_SESSION_CRC_FINAL_XOR_VALUE     0xFFFFFFFF

class Device;

class Session
{
public:
    typedef struct
    {
        byte_t  shift;
        byte_t  pad;
        char    label[ECHO_SESSION_MAX_LABEL_SIZE];
    } InputSettings;

    typedef struct
    {
        byte_t  mute;
        byte_t  solo;
        char    label[ECHO_SESSION_MAX_LABEL_SIZE];
    } PlaybackSettings;

    typedef struct
    {
        byte_t  mute;
        byte_t  shift;
        char    label[ECHO_SESSION_MAX_LABEL_SIZE];
    } OutputSettings;

    typedef struct
    {
        InputSettings       inputs[ECHO_SESSION_MAX_PHY_AUDIO_IN];
        byte_t              monitorpans[ECHO_SESSION_MAX_PHY_AUDIO_IN][ECHO_SESSION_MAX_PHY_AUDIO_OUT];
        byte_t              monitorflags[ECHO_SESSION_MAX_PHY_AUDIO_IN][ECHO_SESSION_MAX_PHY_AUDIO_OUT];
        PlaybackSettings    playbacks[ECHO_SESSION_MAX_PHY_AUDIO_OUT];
        OutputSettings      outputs[ECHO_SESSION_MAX_PHY_AUDIO_OUT];
    } SubSession;

    typedef struct
    {
        uint32_t            size_quads;
        uint32_t            crc;
        uint32_t            version;
        uint32_t            flags;
        int32_t             mirror_channel;
        int32_t             digital_mode;
        int32_t             clock;
        int32_t             rate;

        uint32_t            monitorgains[ECHO_SESSION_MAX_PHY_AUDIO_IN][ECHO_SESSION_MAX_PHY_AUDIO_OUT];
        uint32_t            playbackgains[ECHO_SESSION_MAX_PHY_AUDIO_OUT];
        uint32_t            outputgains[ECHO_SESSION_MAX_PHY_AUDIO_OUT];

        IsoChannelMap       ChannelMap2X;
        IsoChannelMap       ChannelMap4X;

    } SessionHeader;

public:
    Session();
    virtual ~Session();

    bool loadFromDevice(Device &);
    bool saveToDevice(Device &);

    bool loadFromFile(std::string name);
    bool saveToFile(std::string name);

    bool loadFromMemory(void *buff, size_t len);
    bool saveToMemory(void *buff, size_t max_len);

    virtual void setVerboseLevel(int l)
        {setDebugLevel(l);};
    void dumpData();
    void show();

    uint32_t calculateCRC();
    uint32_t calculateCRC(void *memblock, size_t max_len);

    SessionHeader h;
    SubSession    s;

private:
    DECLARE_DEBUG_MODULE;
};

} // FireWorks

#endif //FIREWORKS_SESSION_BLOCK_H
