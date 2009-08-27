/*
 * Copyright (C) 2005-2009 by Pieter Palmers
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

#define DICE_EAP_BASE                  0x0000000000200000ULL
#define DICE_EAP_MAX_SIZE              0x0000000000F00000ULL

#define DICE_EAP_CAPABILITY_SPACE_OFF      0x0000
#define DICE_EAP_CAPABILITY_SPACE_SZ       0x0004
#define DICE_EAP_CMD_SPACE_OFF             0x0008
#define DICE_EAP_CMD_SPACE_SZ              0x000C
#define DICE_EAP_MIXER_SPACE_OFF           0x0010
#define DICE_EAP_MIXER_SPACE_SZ            0x0014
#define DICE_EAP_PEAK_SPACE_OFF            0x0018
#define DICE_EAP_PEAK_SPACE_SZ             0x001C
#define DICE_EAP_NEW_ROUTING_SPACE_OFF     0x0020
#define DICE_EAP_NEW_ROUTING_SPACE_SZ      0x0024
#define DICE_EAP_NEW_STREAM_CFG_SPACE_OFF  0x0028
#define DICE_EAP_NEW_STREAM_CFG_SPACE_SZ   0x002C
#define DICE_EAP_CURR_CFG_SPACE_OFF        0x0030
#define DICE_EAP_CURR_CFG_SPACE_SZ         0x0034
#define DICE_EAP_STAND_ALONE_CFG_SPACE_OFF 0x0038
#define DICE_EAP_STAND_ALONE_CFG_SPACE_SZ  0x003C
#define DICE_EAP_APP_SPACE_OFF             0x0040
#define DICE_EAP_APP_SPACE_SZ              0x0044
#define DICE_EAP_ZERO_MARKER_1             0x0048

// CAPABILITY registers
#define DICE_EAP_CAPABILITY_ROUTER         0x0000
#define DICE_EAP_CAPABILITY_MIXER          0x0004
#define DICE_EAP_CAPABILITY_GENERAL        0x0008
#define DICE_EAP_CAPABILITY_RESERVED       0x000C

// CAPABILITY bit definitions
#define DICE_EAP_CAP_ROUTER_EXPOSED         0
#define DICE_EAP_CAP_ROUTER_READONLY        1
#define DICE_EAP_CAP_ROUTER_FLASHSTORED     2
#define DICE_EAP_CAP_ROUTER_MAXROUTES      16

#define DICE_EAP_CAP_MIXER_EXPOSED          0
#define DICE_EAP_CAP_MIXER_READONLY         1
#define DICE_EAP_CAP_MIXER_FLASHSTORED      2
#define DICE_EAP_CAP_MIXER_IN_DEV           4
#define DICE_EAP_CAP_MIXER_OUT_DEV          8
#define DICE_EAP_CAP_MIXER_INPUTS          16
#define DICE_EAP_CAP_MIXER_OUTPUTS         24

#define DICE_EAP_CAP_GENERAL_STRM_CFG_EN    0
#define DICE_EAP_CAP_GENERAL_FLASH_EN       1
#define DICE_EAP_CAP_GENERAL_PEAK_EN        2
#define DICE_EAP_CAP_GENERAL_MAX_TX_STREAM  4
#define DICE_EAP_CAP_GENERAL_MAX_RX_STREAM  8
#define DICE_EAP_CAP_GENERAL_STRM_CFG_FLS  12
#define DICE_EAP_CAP_GENERAL_CHIP          16

#define DICE_EAP_CAP_GENERAL_CHIP_DICEII    0
#define DICE_EAP_CAP_GENERAL_CHIP_DICEMINI  1
#define DICE_EAP_CAP_GENERAL_CHIP_DICEJR    2

// COMMAND registers
#define DICE_EAP_COMMAND_OPCODE         0x0000
#define DICE_EAP_COMMAND_RETVAL         0x0004

// opcodes
#define DICE_EAP_CMD_OPCODE_NO_OP            0x0000
#define DICE_EAP_CMD_OPCODE_LD_ROUTER        0x0001
#define DICE_EAP_CMD_OPCODE_LD_STRM_CFG      0x0002
#define DICE_EAP_CMD_OPCODE_LD_RTR_STRM_CFG  0x0003
#define DICE_EAP_CMD_OPCODE_LD_FLASH_CFG     0x0004
#define DICE_EAP_CMD_OPCODE_ST_FLASH_CFG     0x0005

#define DICE_EAP_CMD_OPCODE_FLAG_LD_LOW      (1U<<16)
#define DICE_EAP_CMD_OPCODE_FLAG_LD_MID      (1U<<17)
#define DICE_EAP_CMD_OPCODE_FLAG_LD_HIGH     (1U<<18)
#define DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE  (1U<<31)


// MIXER registers
// TODO

// PEAK registers
// TODO

// NEW ROUTER registers
// TODO

// NEW STREAM CFG registers
// TODO

// CURRENT CFG registers
#define DICE_EAP_CURRCFG_LOW_ROUTER         0x0000
#define DICE_EAP_CURRCFG_LOW_STREAM         0x1000
#define DICE_EAP_CURRCFG_MID_ROUTER         0x2000
#define DICE_EAP_CURRCFG_MID_STREAM         0x3000
#define DICE_EAP_CURRCFG_HIGH_ROUTER        0x4000
#define DICE_EAP_CURRCFG_HIGH_STREAM        0x5000

#define DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_QUADS  (64)
#define DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_BYTES  (4*DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_QUADS)
