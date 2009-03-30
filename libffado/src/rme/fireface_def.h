/*
 * Copyright (C) 2009 by Jonathan Woithe
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

/* This file contains definitions relating to the RME Fireface interfaces
 * (Fireface 400 and Fireface 800).  Naming convention:
 *   RME_FF_     identifier applies to both FF400 and FF800
 *   RME_FF400_  identifier specific to the FF400
 *   RME_FF800_  identifier specific to the FF800
 */

#ifndef _FIREFACE_DEF
#define _FIREFACE_DEF

/* Identifiers for the Fireface models */
#define RME_FF400               0
#define RME_FF800               1

/* The Command Buffer Address (CBA) is different for the two interfaces */
#define RME_FF400_CMD_BUFFER    0xfc88f000
#define RME_FF800_CMD_BUFFER    0x80100500

/* Offsets for registers at fixed offsets from the device's command buffer address */
#define RME_FF_SRATE_CTRL_OFS   (0*4)
#define RME_FF_CONF1_OFS        (5*4)
#define RME_FF_CONF2_OFS        (6*4)
#define RME_FF_CONF3_OFS        (7*4)
#define RME_FF400_FLASH_CMD_OFS (8*4)

/* General register definitions */
#define RME_FF400_STREAM_START_REG   (RME_FF400_CMD_BUFFER + 0x001c) 
#define RME_FF800_STREAM_START_REG  0x200000028LL
#define RME_FF400_STREAM_END_REG     (RME_FF400_CMD_BUFFER + 0x0004)  // 4 quadlets wide
#define RME_FF800_STREAM_END_REG    0x200000034LL                     // 3 quadlets wide

#define RME_FF_HOST_LED_REG         0x200000324LL

#define RME_FF_CHANNEL_MUTE_MASK     0x801c0000    /* Write only */
#define RME_FF_STATUS_REG            0x801c0000    /* Read only */

/* Addresses of various blocks in memory-mapped flash */
#define RME_FF400_FLASH_SETTINGS_ADDR       0x00060000
#define RME_FF400_FLASH_MIXER_VOLUME_ADDR   0x00070000
#define RME_FF400_FLASH_MIXER_PAN_ADDR      0x00070800
#define RME_FF400_FLASH_MIXER_HW_ADDR       0x00071000  /* Hardware volume settings, MIDI enable, submix */
#define RME_FF800_FLASH_MIXER_SHADOW_ADDR  0x3000e0000LL
#define RME_FF800_FLASH_MIXER_VOLUME_ADDR  0x3000e2000LL
#define RME_FF800_FLASH_MIXER_PAN_ADDR     0x3000e2800LL
#define RME_FF800_FLASH_MIXER_HW_ADDR      0x3000e3000LL  /* H/w volume settings, MIDI enable, submix */
#define RME_FF800_FLASH_SETTINGS_ADDR      0x3000f0000LL

/* Flash control registers */
#define RME_FF400_FLASH_BLOCK_ADDR_REG      0x80100288
#define RME_FF400_FLASH_BLOCK_SIZE_REG      0x8010028c
#define RME_FF400_FLASH_CMD_REG             (RME_FF400_CMD_BUFFER+RME_FF400_FLASH_CMD_OFS)
#define RME_FF400_FLASH_WRITE_BUFFER        0x80100290

/* Erase control registers on the FF800 */
#define RME_FF800_FLASH_ERASE_VOLUME_REG    0x3fffffff4LL
#define RME_FF800_FLASH_ERASE_SETTINGS_REG  0x3fffffff0LL
#define RME_FF800_FLASH_ERASE_FIRMWARE_REG  0x3fffffff8LL
#define RME_FF800_FLASH_ERASE_CONFIG_REG    0x3fffffffcLL

/* Flags and special values */
#define RME_FF400_FLASH_CMD_WRITE           0x1
#define RME_FF400_FLASH_CMD_READ            0x2
#define RME_FF400_FLASH_CMD_ERASE_VOLUME    0xe
#define RME_FF400_FLASH_CMD_ERASE_SETTINGS  0xd
#define RME_FF400_FLASH_CMD_ERASE_CONFIG    0xc

#endif
