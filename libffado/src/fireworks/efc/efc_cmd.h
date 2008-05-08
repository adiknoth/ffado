/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#ifndef FIREWORKS_EFC_CMD_H
#define FIREWORKS_EFC_CMD_H

#include "debugmodule/debugmodule.h"

#include "libutil/cmd_serialize.h"

#define EFC_CAT_INVALID                 0xFFFFFFFF
#define EFC_CMD_INVALID                 0xFFFFFFFF

// Categories
#define	EFC_CAT_HARDWARE_INFO           0
#define EFC_CAT_FLASH                   1
#define EFC_CAT_TRANSPORT               2
#define EFC_CAT_HARDWARE_CONTROL        3
#define EFC_CAT_PHYSICAL_OUTPUT_MIX     4
#define EFC_CAT_PHYSICAL_INPUT_MIX      5
#define EFC_CAT_PLAYBACK_MIX            6
#define EFC_CAT_RECORD_MIX              7
#define EFC_CAT_MONITOR_MIX             8
#define EFC_CAT_IO_CONFIG               9
#define EFC_CAT_COUNT                   10

// Commands for the EFC_CAT_HARDWARE_INFO category
#define EFC_CMD_HW_HWINFO_GET_CAPS      0
#define EFC_CMD_HW_GET_POLLED           1
#define EFC_CMD_HW_SET_EFR_ADDRESS      2
#define EFC_CMD_HW_READ_SESSION_BLOCK   3
#define EFC_CMD_HW_GET_DEBUG_INFO       4
#define EFC_CMD_HW_SET_DEBUG_TRACKING   5
#define EFC_CMD_HW_COUNT                6

// Commands for the EFC_CAT_FLASH category
#define EFC_CMD_FLASH_ERASE             0
#define EFC_CMD_FLASH_READ              1
#define EFC_CMD_FLASH_WRITE             2
#define EFC_CMD_FLASH_GET_STATUS        3
#define EFC_CMD_FLASH_GET_SESSION_BASE  4
#define EFC_CMD_FLASH_LOCK              5

// Commands for the EFC_CAT_HARDWARE_CONTROL category
#define EFC_CMD_HWCTRL_SET_CLOCK        0
#define EFC_CMD_HWCTRL_GET_CLOCK        1
#define EFC_CMD_HWCTRL_BSX_HANDSHAKE    2
#define EFC_CMD_HWCTRL_CHANGE_FLAGS     3
#define EFC_CMD_HWCTRL_GET_FLAGS        4
#define EFC_CMD_HWCTRL_IDENTIFY         5
#define EFC_CMD_HWCTRL_RECONNECT_PHY    6

// Commands for the EFC_CAT_*_MIX categories
#define EFC_CMD_MIXER_SET_GAIN        0
#define EFC_CMD_MIXER_GET_GAIN        1
#define EFC_CMD_MIXER_SET_MUTE        2
#define EFC_CMD_MIXER_GET_MUTE        3
#define EFC_CMD_MIXER_SET_SOLO        4
#define EFC_CMD_MIXER_GET_SOLO        5
#define EFC_CMD_MIXER_SET_PAN         6
#define EFC_CMD_MIXER_GET_PAN         7
#define EFC_CMD_MIXER_SET_NOMINAL     8
#define EFC_CMD_MIXER_GET_NOMINAL     9

// Commands for the EFC_CAT_IO_CONFIG category
#define EFC_CMD_IO_CONFIG_SET_MIRROR        0
#define EFC_CMD_IO_CONFIG_GET_MIRROR        1
#define EFC_CMD_IO_CONFIG_SET_DIGITAL_MODE  2
#define EFC_CMD_IO_CONFIG_GET_DIGITAL_MODE  3
#define EFC_CMD_IO_CONFIG_SET_PHANTOM       4
#define EFC_CMD_IO_CONFIG_GET_PHANTOM       5
#define EFC_CMD_IO_CONFIG_SET_ISOC_MAP      6
#define EFC_CMD_IO_CONFIG_GET_ISOC_MAP      7


// size of the header
#define EFC_HEADER_LENGTH_QUADLETS      ((sizeof(uint32_t) + sizeof(struct EfcCmd::efc_header))/4)

// util macro to do deserialization and byteswap
#define EFC_DESERIALIZE_AND_SWAP(__de__, __value__, __result__) \
    { __result__ &= __de__.read(__value__); \
      *(__value__)=CondSwap32(*(__value__)); } \


// specifiers for the flags field
#define EFC_CMD_HW_DYNADDR_SUPPORTED                1
#define EFC_CMD_HW_MIRRORING_SUPPORTED              2
#define EFC_CMD_HW_SPDIF_COAX_SUPPORTED             3
#define EFC_CMD_HW_SPDIF_AESEBUXLR_SUPPORTED        4
#define EFC_CMD_HW_HAS_DSP                          5
#define EFC_CMD_HW_HAS_FPGA                         6
#define EFC_CMD_HW_HAS_PHANTOM                      7

#define EFC_CMD_HW_CHECK_FLAG(__val__,__flag__) \
    (((__val__) & (1<<(__flag__))) != 0)
#define EFC_CMD_HW_TO_FLAG(__val__) \
    (1<<(__val__))

// Clock sources
#define EFC_CMD_HW_CLOCK_INTERNAL                   0
#define EFC_CMD_HW_CLOCK_SYTMATCH                   1
#define EFC_CMD_HW_CLOCK_WORDCLOCK                  2
#define EFC_CMD_HW_CLOCK_SPDIF                      3
#define EFC_CMD_HW_CLOCK_ADAT_1                     4
#define EFC_CMD_HW_CLOCK_ADAT_2                     5
#define EFC_CMD_HW_CLOCK_COUNT                      6

#define EFC_CMD_HW_CLOCK_UNSPECIFIED       0xFFFFFFFF

// MIDI flags
#define EFC_CMD_HW_MIDI_IN_1                        8
#define EFC_CMD_HW_MIDI_OUT_1                       9
#define EFC_CMD_HW_MIDI_IN_2                       10
#define EFC_CMD_HW_MIDI_OUT_2                      11

// Channel types
#define EFC_CMD_HW_CHANNEL_TYPE_ANALOG              0
#define EFC_CMD_HW_CHANNEL_TYPE_SPDIF               1
#define EFC_CMD_HW_CHANNEL_TYPE_ADAT                2
#define EFC_CMD_HW_CHANNEL_TYPE_SPDIF_OR_ADAT       3
#define EFC_CMD_HW_CHANNEL_TYPE_ANALOG_MIRRORING    4
#define EFC_CMD_HW_CHANNEL_TYPE_HEADPHONES          5
#define EFC_CMD_HW_CHANNEL_TYPE_I2S                 6

namespace FireWorks {

enum eMixerTarget {
    eMT_PhysicalOutputMix,
    eMT_PhysicalInputMix,
    eMT_PlaybackMix,
    eMT_RecordMix,
};
enum eMixerCommand {
    eMC_Gain,
    eMC_Solo,
    eMC_Mute,
    eMC_Pan,
    eMC_Nominal,
};
enum eCmdType {
    eCT_Get,
    eCT_Set,
};

enum eIOConfigRegister {
    eCR_Mirror,
    eCR_DigitalMode,
    eCR_Phantom,
};

const char *eMixerTargetToString(const enum eMixerTarget target);
const char *eMixerCommandToString(const enum eMixerCommand command);
const char *eIOConfigRegisterToString(const enum eIOConfigRegister reg);

class EfcCmd
{
public:
    enum EfcReturnValue {
        eERV_Ok             = 0,
        eERV_Bad            = 1,
        eERV_BadCommand     = 2,
        eERV_CommErr        = 3,
        eERV_BadQuadCount   = 4,
        eERV_Unsupported    = 5,
        eERV_1394Timeout    = 6,
        eERV_DspTimeout     = 7,
        eERV_BadRate        = 8,
        eERV_BadClock       = 9,
        eERV_BadChannel     = 10,
        eERV_BadPan         = 11,
        eERV_FlashBusy      = 12,
        eERV_BadMirror      = 13,
        eERV_BadLed         = 14,
        eERV_BadParameter   = 15,
        eERV_Incomplete     = 0x80000000L,
        eERV_Unspecified    = 0xFFFFFFFFL,
    };

    typedef struct efc_header
    {
        uint32_t    version;
        uint32_t    seqnum;
        uint32_t    category;
        uint32_t    command;
        uint32_t    retval;
    };

protected: // this HAS to be overloaded
    EfcCmd(uint32_t cat, uint32_t cmd);
    EfcCmd();

public:
    virtual ~EfcCmd();
    
    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const = 0;

    virtual void showEfcCmd();
    virtual void setVerboseLevel(int l);

    uint32_t            m_length; // in quadlets, including length field and header.
    struct efc_header   m_header;

protected:
    uint32_t            m_category_id;
    uint32_t            m_command_id;

public:
    static uint32_t     m_seqnum;
protected:
    DECLARE_DEBUG_MODULE;
};

} // namespace FireWorks

#endif // FIREWORKS_EFC_CMD_H
