/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#ifndef DICEDEFINES_H
#define DICEDEFINES_H

#define DICE_VER_1_0_4_0
// #define DICE_VER_1_0_7_0

#define DICE_INVALID_OFFSET                  0xFFFFF00000000000ULL

/*
 * This header is based upon the DICE II driver specification
 * version 1.0.7.0 and:
 *  dicedriverExtStatus.h,v 1.2 2006/09/27 20:35:45
 *  dicedriverInterface.h,v 1.1.1.1 2006/08/10 20:00:57
 *
 */

// Register addresses & offsets
//  DICE_PRIVATE_SPACE registers
#define DICE_REGISTER_BASE                  0x0000FFFFE0000000ULL

#define DICE_REGISTER_GLOBAL_PAR_SPACE_OFF  0x0000
#define DICE_REGISTER_GLOBAL_PAR_SPACE_SZ   0x0004
#define DICE_REGISTER_TX_PAR_SPACE_OFF      0x0008
#define DICE_REGISTER_TX_PAR_SPACE_SZ       0x000C
#define DICE_REGISTER_RX_PAR_SPACE_OFF      0x0010
#define DICE_REGISTER_RX_PAR_SPACE_SZ       0x0014
#define DICE_REGISTER_UNUSED1_SPACE_OFF     0x0018
#define DICE_REGISTER_UNUSED1_SPACE_SZ      0x001C
#define DICE_REGISTER_UNUSED2_SPACE_OFF     0x0020
#define DICE_REGISTER_UNUSED2_SPACE_SZ      0x0024

//  GLOBAL_PAR_SPACE registers
#define DICE_REGISTER_GLOBAL_OWNER              0x0000
#define DICE_REGISTER_GLOBAL_NOTIFICATION       0x0008
#define DICE_REGISTER_GLOBAL_NICK_NAME          0x000C
#define DICE_REGISTER_GLOBAL_CLOCK_SELECT       0x004C
#define DICE_REGISTER_GLOBAL_ENABLE             0x0050
#define DICE_REGISTER_GLOBAL_STATUS             0x0054
#define DICE_REGISTER_GLOBAL_EXTENDED_STATUS    0x0058
#define DICE_REGISTER_GLOBAL_SAMPLE_RATE        0x005C
#define DICE_REGISTER_GLOBAL_VERSION            0x0060
#define DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES  0x0064
#define DICE_REGISTER_GLOBAL_CLOCKSOURCENAMES   0x0068

//  TX_PAR_SPACE registers
#define DICE_REGISTER_TX_NB_TX                  0x0000
#define DICE_REGISTER_TX_SZ_TX                  0x0004

#define DICE_REGISTER_TX_ISOC_BASE              0x0008
#define DICE_REGISTER_TX_NB_AUDIO_BASE          0x000C
#define DICE_REGISTER_TX_MIDI_BASE              0x0010
#define DICE_REGISTER_TX_SPEED_BASE             0x0014
#define DICE_REGISTER_TX_NAMES_BASE             0x0018
#define DICE_REGISTER_TX_AC3_CAPABILITIES_BASE  0x0118
#define DICE_REGISTER_TX_AC3_ENABLE_BASE        0x011C

#define DICE_REGISTER_TX_PARAM(size, i, offset) \
            ( ((i) * (size) ) + (offset) )

//  RX_PAR_SPACE registers
#define DICE_REGISTER_RX_NB_RX                  0x0000
#define DICE_REGISTER_RX_SZ_RX                  0x0004

#ifdef DICE_VER_1_0_4_0
    #define DICE_REGISTER_RX_ISOC_BASE              0x0008
    #define DICE_REGISTER_RX_SEQ_START_BASE         0x0014
    #define DICE_REGISTER_RX_NB_AUDIO_BASE          0x000C
    #define DICE_REGISTER_RX_MIDI_BASE              0x0010
    #define DICE_REGISTER_RX_NAMES_BASE             0x0018
    #define DICE_REGISTER_RX_AC3_CAPABILITIES_BASE  0x0118
    #define DICE_REGISTER_RX_AC3_ENABLE_BASE        0x011C
#endif

#ifdef DICE_VER_1_0_7_0
    #define DICE_REGISTER_RX_ISOC_BASE              0x0008
    #define DICE_REGISTER_RX_SEQ_START_BASE         0x000C
    #define DICE_REGISTER_RX_NB_AUDIO_BASE          0x0010
    #define DICE_REGISTER_RX_MIDI_BASE              0x0014
    #define DICE_REGISTER_RX_NAMES_BASE             0x0018
    #define DICE_REGISTER_RX_AC3_CAPABILITIES_BASE  0x0118
    #define DICE_REGISTER_RX_AC3_ENABLE_BASE        0x011C
#endif

#define DICE_REGISTER_RX_PARAM(size, i, offset) \
            ( ((i) * (size) ) + (offset) )

// Register Bitfields
//  GLOBAL_PAR_SPACE registers
//   OWNER register defines
#define DICE_OWNER_NO_OWNER 0xFFFF000000000000LLU

//   NOTIFICATION register defines
#define DICE_NOTIFY_RX_CFG_CHG_BIT      (1UL << 0)
#define DICE_NOTIFY_TX_CFG_CHG_BIT      (1UL << 1)
#define DICE_NOTIFY_DUP_ISOC_BIT        (1UL << 2)
#define DICE_NOTIFY_BW_ERR_BIT          (1UL << 3)
#define DICE_NOTIFY_LOCK_CHG_BIT        (1UL << 4)
#define DICE_NOTIFY_CLOCK_ACCEPTED      (1UL << 5)

// bits 6..15 are RESERVED

// FIXME:
// diceDriverInterface.h defines the following bitfield
// that is undocumented by spec 1.0.7.0
#define DICE_INTERFACE_CHG_BIT          (1UL << 6)

// FIXME:
// The spec 1.0.7.0 defines these as USER notifications
// however diceDriverInterface.h defines these as
// 'reserved bits for future system wide use'.
#define DICE_NOTIFY_RESERVED1           (1UL << 16)
#define DICE_NOTIFY_RESERVED2           (1UL << 17)
#define DICE_NOTIFY_RESERVED3           (1UL << 18)
#define DICE_NOTIFY_RESERVED4           (1UL << 19)

// FIXME:
// The spec 1.0.7.0 does not specify anything about
// the format of the user messages
// however diceDriverInterface.h indicates:
// "When DD_NOTIFY_MESSAGE is set DD_NOTIFY_USER4 through
//  DD_NOTIFY_USER11 are defined as an 8 bit message so
//  you can have 256 seperate messages (like gray encoder
//  movements)."

#define DICE_NOTIFY_MESSAGE             (1UL << 20)
#define DICE_NOTIFY_USER1               (1UL << 21)
#define DICE_NOTIFY_USER2               (1UL << 22)
#define DICE_NOTIFY_USER3               (1UL << 23)
#define DICE_NOTIFY_USER4               (1UL << 24)
#define DICE_NOTIFY_USER5               (1UL << 25)
#define DICE_NOTIFY_USER6               (1UL << 26)
#define DICE_NOTIFY_USER7               (1UL << 27)
#define DICE_NOTIFY_USER8               (1UL << 28)
#define DICE_NOTIFY_USER9               (1UL << 29)
#define DICE_NOTIFY_USER10              (1UL << 30)
#define DICE_NOTIFY_USER11              (1UL << 31)

#define DICE_NOTIFY_USER_IS_MESSAGE(x) \
                ( ((x) & DICE_NOTIFY_MESSAGE) != 0 )

#define DICE_NOTIFY_USER_GET_MESSAGE(x) \
                ( ((x) >> 24 ) & 0xFF )

//   NICK_NAME register

// NOTE: in bytes
#define DICE_NICK_NAME_SIZE             64

//   CLOCK_SELECT register
// Clock sources supported
#define DICE_CLOCKSOURCE_AES1           0x00
#define DICE_CLOCKSOURCE_AES2           0x01
#define DICE_CLOCKSOURCE_AES3           0x02
#define DICE_CLOCKSOURCE_AES4           0x03
#define DICE_CLOCKSOURCE_AES_ANY        0x04
#define DICE_CLOCKSOURCE_ADAT           0x05
#define DICE_CLOCKSOURCE_TDIF           0x06
#define DICE_CLOCKSOURCE_WC             0x07
#define DICE_CLOCKSOURCE_ARX1           0x08
#define DICE_CLOCKSOURCE_ARX2           0x09
#define DICE_CLOCKSOURCE_ARX3           0x0A
#define DICE_CLOCKSOURCE_ARX4           0x0B
#define DICE_CLOCKSOURCE_INTERNAL       0x0C
#define DICE_CLOCKSOURCE_COUNT (DICE_CLOCKSOURCE_INTERNAL+1)

#define DICE_CLOCKSOURCE_MASK           0x0000FFFFLU
#define DICE_GET_CLOCKSOURCE(reg)       (((reg) & DICE_CLOCKSOURCE_MASK))
#define DICE_SET_CLOCKSOURCE(reg,clk)   (((reg) & ~DICE_CLOCKSOURCE_MASK) | ((clk) & DICE_CLOCKSOURCE_MASK))

// Supported rates
#define DICE_RATE_32K                   0x00
#define DICE_RATE_44K1                  0x01
#define DICE_RATE_48K                   0x02
#define DICE_RATE_88K2                  0x03
#define DICE_RATE_96K                   0x04
#define DICE_RATE_176K4                 0x05
#define DICE_RATE_192K                  0x06
#define DICE_RATE_ANY_LOW               0x07
#define DICE_RATE_ANY_MID               0x08
#define DICE_RATE_ANY_HIGH              0x09
#define DICE_RATE_NONE                  0x0A

#define DICE_RATE_MASK                  0x0000FF00LU
#define DICE_GET_RATE(reg)              (((reg) & DICE_RATE_MASK) >> 8)
#define DICE_SET_RATE(reg,rate)         (((reg) & ~DICE_RATE_MASK) | (((rate) << 8) & DICE_RATE_MASK) )

//   ENABLE register
#define DICE_ISOSTREAMING_ENABLE        (1UL << 0)
#define DICE_ISOSTREAMING_DISABLE       (0)


//   CLOCK_STATUS register
#define DICE_STATUS_SOURCE_LOCKED       (1UL << 0)
#define DICE_STATUS_RATE_CONFLICT       (1UL << 1)

#define DICE_STATUS_GET_NOMINAL_RATE(x) ( ((x) >> 8 ) & 0xFF )

//   EXTENDED_STATUS register
#define DICE_EXT_STATUS_AES0_LOCKED         (1UL << 0)
#define DICE_EXT_STATUS_AES1_LOCKED         (1UL << 1)
#define DICE_EXT_STATUS_AES2_LOCKED         (1UL << 2)
#define DICE_EXT_STATUS_AES3_LOCKED         (1UL << 3)
#define DICE_EXT_STATUS_AES_ANY_LOCKED      (    0x0F)
#define DICE_EXT_STATUS_ADAT_LOCKED         (1UL << 4)
#define DICE_EXT_STATUS_TDIF_LOCKED         (1UL << 5)
#define DICE_EXT_STATUS_ARX1_LOCKED         (1UL << 6)
#define DICE_EXT_STATUS_ARX2_LOCKED         (1UL << 7)
#define DICE_EXT_STATUS_ARX3_LOCKED         (1UL << 8)
#define DICE_EXT_STATUS_ARX4_LOCKED         (1UL << 9)

// FIXME: this one is missing in dicedriverExtStatus.h
#define DICE_EXT_STATUS_WC_LOCKED          (1UL << 10)

#define DICE_EXT_STATUS_AES0_SLIP           (1UL << 16)
#define DICE_EXT_STATUS_AES1_SLIP           (1UL << 17)
#define DICE_EXT_STATUS_AES2_SLIP           (1UL << 18)
#define DICE_EXT_STATUS_AES3_SLIP           (1UL << 19)
#define DICE_EXT_STATUS_ADAT_SLIP           (1UL << 20)
#define DICE_EXT_STATUS_TDIF_SLIP           (1UL << 21)
#define DICE_EXT_STATUS_ARX1_SLIP           (1UL << 22)
#define DICE_EXT_STATUS_ARX2_SLIP           (1UL << 23)
#define DICE_EXT_STATUS_ARX3_SLIP           (1UL << 24)
#define DICE_EXT_STATUS_ARX4_SLIP           (1UL << 25)

// FIXME: does this even exist?
#define DICE_EXT_STATUS_WC_SLIP             (1UL << 26)

//   SAMPLE_RATE register
// nothing here

//   VERSION register
#define DICE_DRIVER_SPEC_VERSION_NUMBER_GET(x,y) \
            ( ( (x) >> (y)) & 0xFF )

#define DICE_DRIVER_SPEC_VERSION_NUMBER_GET_A(x) \
            DICE_DRIVER_SPEC_VERSION_NUMBER_GET(x,24)

#define DICE_DRIVER_SPEC_VERSION_NUMBER_GET_B(x) \
            DICE_DRIVER_SPEC_VERSION_NUMBER_GET(x,16)

#define DICE_DRIVER_SPEC_VERSION_NUMBER_GET_C(x) \
            DICE_DRIVER_SPEC_VERSION_NUMBER_GET(x,8)

#define DICE_DRIVER_SPEC_VERSION_NUMBER_GET_D(x) \
            DICE_DRIVER_SPEC_VERSION_NUMBER_GET(x,0)

//   CLOCKCAPABILITIES register
#define DICE_CLOCKCAP_RATE_32K          (1UL << 0)
#define DICE_CLOCKCAP_RATE_44K1         (1UL << 1)
#define DICE_CLOCKCAP_RATE_48K          (1UL << 2)
#define DICE_CLOCKCAP_RATE_88K2         (1UL << 3)
#define DICE_CLOCKCAP_RATE_96K          (1UL << 4)
#define DICE_CLOCKCAP_RATE_176K4        (1UL << 5)
#define DICE_CLOCKCAP_RATE_192K         (1UL << 6)
#define DICE_CLOCKCAP_SOURCE_AES1       (1UL << 16)
#define DICE_CLOCKCAP_SOURCE_AES2       (1UL << 17)
#define DICE_CLOCKCAP_SOURCE_AES3       (1UL << 18)
#define DICE_CLOCKCAP_SOURCE_AES4       (1UL << 19)
#define DICE_CLOCKCAP_SOURCE_AES_ANY    (1UL << 20)
#define DICE_CLOCKCAP_SOURCE_ADAT       (1UL << 21)
#define DICE_CLOCKCAP_SOURCE_TDIF       (1UL << 22)
#define DICE_CLOCKCAP_SOURCE_WORDCLOCK  (1UL << 23)
#define DICE_CLOCKCAP_SOURCE_ARX1       (1UL << 24)
#define DICE_CLOCKCAP_SOURCE_ARX2       (1UL << 25)
#define DICE_CLOCKCAP_SOURCE_ARX3       (1UL << 26)
#define DICE_CLOCKCAP_SOURCE_ARX4       (1UL << 27)
#define DICE_CLOCKCAP_SOURCE_INTERNAL   (1UL << 28)

//   CLOCKSOURCENAMES
// note: in bytes
#define DICE_CLOCKSOURCENAMES_SIZE      256

//  TX_PAR_SPACE registers
// note: in bytes
#define DICE_TX_NAMES_SIZE              256

//  RX_PAR_SPACE registers
// note: in bytes
#define DICE_RX_NAMES_SIZE              256

#endif // DICEDEFINES_H
