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

/* The maximum number of channels supported by each device */
#define RME_FF400_MAX_CHANNELS  18
#define RME_FF800_MAX_CHANNELS  28

/* Boundaries between the speed multipliers */
#define MIN_SPEED               30000
#define MIN_DOUBLE_SPEED        56000
#define MIN_QUAD_SPEED          112000
#define MAX_SPEED               210000

// A flag used to indicate the use of a 800 Mbps bus speed to various
// streaming registers of the FF800.
#define RME_FF800_STREAMING_SPEED_800 0x800

/* The Command Buffer Address (CBA) is different for the two interfaces */
#define RME_FF400_CMD_BUFFER    0x80100500
#define RME_FF800_CMD_BUFFER    0xfc88f000

// Offsets for registers at fixed offsets from the device's command buffer 
// address 
#define RME_FF_DDS_SRATE_OFS      (0*4)
#define RME_FF_CONF1_OFS          (5*4)
#define RME_FF_CONF2_OFS          (6*4)
#define RME_FF_CONF3_OFS          (7*4)
#define RME_FF400_FLASH_CMD_OFS   (8*4)       // Write only
#define RME_FF400_FLASH_STAT_OFS  (8*4)       // Read only

/* General register definitions */
#define RME_FF400_CONF_REG          (RME_FF400_CMD_BUFFER + RME_FF_CONF1_OFS)
#define RME_FF800_CONF_REG          (RME_FF800_CMD_BUFFER + RME_FF_CONF1_OFS)

#define RME_FF400_STREAM_INIT_REG   (RME_FF400_CMD_BUFFER)  // 5 quadlets wide
#define RME_FF400_STREAM_INIT_SIZE  5                       // Size in quadlets
#define RME_FF400_STREAM_SRATE      (RME_FF400_CMD_BUFFER)
#define RME_FF400_STREAM_CONF0      (RME_FF400_CMD_BUFFER+4)
#define RME_FF400_STREAM_CONF1      (RME_FF400_CMD_BUFFER+8)
#define RME_FF800_STREAM_INIT_REG   0x20000001cLL           // 3 quadlets wide
#define RME_FF800_STREAM_INIT_SIZE  3                       // Size in quadlets
#define RME_FF800_STREAM_SRATE      0x20000001cLL
#define RME_FF800_STREAM_CONF0      (0x20000001cLL+4)
#define RME_FF800_STREAM_CONF1      (0x20000001cLL+8)
#define RME_FF400_STREAM_START_REG  (RME_FF400_CMD_BUFFER + 0x000c)  // 1 quadlet
#define RME_FF800_STREAM_START_REG  0x200000028LL                    // 1 quadlet
#define RME_FF400_STREAM_END_REG    (RME_FF400_CMD_BUFFER + 0x0004)  // 4 quadlets wide
#define RME_FF400_STREAM_END_SIZE   4              // Size in quadlets
#define RME_FF800_STREAM_END_REG    0x200000034LL  // 3 quadlets wide
#define RME_FF800_STREAM_END_SIZE   3              // Size in quadlets

#define RME_FF800_HOST_LED_REG      0x200000324LL

#define RME_FF800_REVISION_REG      0x200000100LL

#define RME_FF_CHANNEL_MUTE_MASK     0x801c0000    // Write only
#define RME_FF_STATUS_REG0           0x801c0000    // Read only
#define RME_FF_STATUS_REG1           0x801c0004    // Read only
#define RME_FF_STATUS_REG2           0x801c0008
#define RME_FF_STATUS_REG3           0x801c001c
#define RME_FF_OUTPUT_REC_MASK       0x801c0080    // Write only

#define RME_FF_MIXER_RAM             0x80080000

#define RME_FF_TCO_READ_REG          0x801f0000    // FF800 only
#define RME_FF_TCO_WRITE_REG         0x810f0020    // FF800 only

#define RME_FF400_GAIN_REG           0x801c0180

#define RME_FF400_MIDI_HIGH_ADDR     0x801003f4

/* Types of controls in the matrix mixer */
#define RME_FF_MM_INPUT              0x0000
#define RME_FF_MM_PLAYBACK           0x0001
#define RME_FF_MM_OUTPUT             0x0002

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
#define RME_FF400_FLASH_CMD_REG             (RME_FF400_CMD_BUFFER + RME_FF400_FLASH_CMD_OFS)
#define RME_FF400_FLASH_STAT_REG            (RME_FF400_CMD_BUFFER + RME_FF400_FLASH_STAT_OFS)
#define RME_FF400_FLASH_WRITE_BUFFER        0x80100290
#define RME_FF400_FLASH_READ_BUFFER         0x80100290

/* Flash erase control registers on the FF800 */
#define RME_FF800_FLASH_ERASE_VOLUME_REG    0x3fffffff4LL
#define RME_FF800_FLASH_ERASE_SETTINGS_REG  0x3fffffff0LL
#define RME_FF800_FLASH_ERASE_FIRMWARE_REG  0x3fffffff8LL
#define RME_FF800_FLASH_ERASE_CONFIG_REG    0x3fffffffcLL

/* Flash erase command values for the FF400 */
#define RME_FF400_FLASH_CMD_WRITE           0x00000001
#define RME_FF400_FLASH_CMD_READ            0x00000002
#define RME_FF400_FLASH_CMD_ERASE_VOLUME    0x0000000e
#define RME_FF400_FLASH_CMD_ERASE_SETTINGS  0x0000000d
#define RME_FF400_FLASH_CMD_ERASE_CONFIG    0x0000000c
#define RME_FF400_FLASH_CMD_GET_REVISION    0x0000000f

/* Flags for use with erase_flash() */
#define RME_FF_FLASH_ERASE_VOLUME           1
#define RME_FF_FLASH_ERASE_SETTINGS         2
#define RME_FF_FLASH_ERASE_CONFIG           3

/* Defines for components of the control registers */
// Configuration register 0
#define CR0_PHANTOM_MIC0        0x00000001

#define CR0_PHANTOM_MIC2        0x00000002
#define CR0_FILTER_FPGA         0x00000004
#define CR0_BIT01               0x00000002  // Use depends on model - see below
#define CR0_BIT02               0x00000004  // Use depends on model - see below
#define CR0_ILEVEL_FPGA_CTRL0   0x00000008
#define CR0_ILEVEL_FPGA_CTRL1   0x00000010
#define CR0_ILEVEL_FPGA_CTRL2   0x00000020
#define CR0_ZEROBIT06           0x00000040
#define CR0_PHANTOM_MIC1        0x00000080
#define CR0_BIT08               0x00000100  // Use depends on model - see below
#define CR0_BIT09               0x00000200  // Use depends on model - see below
#define CRO_OLEVEL_FPGA_CTRL_0  0x00000400
#define CRO_OLEVEL_FPGA_CTRL_1  0x00000800
#define CRO_OLEVEL_FPGA_CTRL_2  0x00001000
#define CR0_ZEROBIT13           0x00002000
#define CRO_ZEROBIT14           0x00004000
#define CRO_ZEROBIT15           0x00008000
#define CRO_PHLEVEL_CTRL0       0x00010000
#define CRO_PHLEVEL_CTRL1       0x00020000

#define CR0_FF400_PHANTOM_MIC0  CR0_PHANTOM_MIC0
#define CR0_FF400_PHANTOM_MIC1  CR0_PHANTOM_MIC1
#define CR0_FF400_CH3_PAD       CR0_BIT08
#define CR0_FF400_CH3_INSTR     CR0_BIT09
#define CR0_FF400_CH4_PAD       CR0_BIT01
#define CR0_FF400_CH4_INSTR     CR0_BIT02
#define CR0_FF800_PHANTOM_MIC7  CR0_PHANTOM_MIC0
#define CR0_FF800_PHANTOM_MIC8  CR0_PHANTOM_MIC1
#define CR0_FF800_PHANTOM_MIC9  CR0_BIT01
#define CR0_FF800_PHANTOM_MIC10 CR0_BIT08
#define CR0_FF800_FILTER_FPGA   CR0_BIT02
#define CR0_FF800_DRIVE_FPGA    CR0_BIT09
#define CR0_ILEVEL_FPGA_LOGAIN  CR0_ILEVEL_FPGA_CTRL0
#define CR0_ILEVEL_FPGA_4dBU    CR0_ILEVEL_FPGA_CTRL1
#define CR0_ILEVEL_FPGA_m10dBV  CR0_ILEVEL_FPGA_CTRL2
#define CR0_OLEVEL_FPGA_HIGAIN  CRO_OLEVEL_FPGA_CTRL_0
#define CR0_OLEVEL_FPGA_4dBU    CRO_OLEVEL_FPGA_CTRL_1
#define CR0_OLEVEL_FPGA_m10dBV  CRO_OLEVEL_FPGA_CTRL_2
#define CR0_PHLEVEL_4dBU        0
#define CRO_PHLEVEL_m10dBV      CRO_PHLEVEL_CTRL0
#define CRO_PHLEVEL_HIGAIN      CRO_PHLEVEL_CTRL1

// Configuration register 1
#define CR1_ILEVEL_CPLD_CTRL0   0x00000001
#define CR1_ILEVEL_CPLD_CTRL1   0x00000002
#define CR1_INPUT_OPT0_B        0x00000004    // Input optionset 0, option B
#define CR1_OLEVEL_CPLD_CTRL0   0x00000008
#define CR1_OLEVEL_CPLD_CTRL1   0x00000010
#define CR1_INPUT_OPT1_A        0x00000020    // Input optionset 1, option A
#define CR1_INPUT_OPT1_B        0x00000040    // Input optionset 1, option B
#define CR1_INPUT_OPT2_A        0x00000080    // Input optionset 2, option A
#define CR1_INPUT_OPT2_B        0x00000100    // Input optionset 2, option B
#define CR1_INSTR_DRIVE         0x00000200
#define CR1_INPUT_OPT0_A1       0x00000400    // Input optionset 0, option A bit 1
#define CR1_INPUT_OPT0_A0       0x00000800    // Input optionset 0, option A bit 0

#define CR1_ILEVEL_CPLD_LOGAIN  0
#define CR1_ILEVEL_CPLD_4dBU    CR1_ILEVEL_CPLD_CTRL1
#define CR1_ILEVEL_CPLD_m10dBV  (CR1_ILEVEL_CPLD_CTRL0 | CR1_ILEVEL_CPLD_CTRL1)
#define CR1_OLEVEL_CPLD_m10dBV  CR1_OLEVEL_CPLD_CTRL0
#define CR1_OLEVEL_CPLD_HIGAIN  CR1_OLEVEL_CPLD_CTRL1
#define CR1_OLEVEL_CPLD_4dBU    (CR1_OLEVEL_CPLD_CTRL0 | CR1_OLEVEL_CPLD_CTRL1)
#define CR1_FF800_INPUT7_FRONT  CR1_INPUT_OPT1_A
#define CR1_FF800_INPUT7_REAR   CR1_INPUT_OPT1_B
#define CR1_FF800_INPUT8_FRONT  CR1_INPUT_OPT2_A
#define CR1_FF800_INPUT8_REAR   CR1_INPUT_OPT2_B
#define CR1_FF400_INPUT3_INSTR  CR1_INPUT_OPT1_B   // To be confirmed
#define CR1_FF400_INPUT3_PAD    CR1_INPUT_OPT1_A   // To be confirmed
#define CR1_FF400_INPUT4_INSTR  CR1_INPUT_OPT2_B   // To be confirmed
#define CR1_FF400_INPUT4_PAD    CR1_INPUT_OPT2_A   // To be confirmed

// The input 1 "front" option is strange on the FF800 in that it is
// indicated using two bits.  The actual bit set depends, curiously enough,
// on the "speaker emulation" (aka "filter") setting.  How odd.
#define CR1_FF800_INPUT1_FRONT              CR1_INPUT_OPT0_A0
#define CR1_FF800_INPUT1_FRONT_WITH_FILTER  CR1_INPUT_OPT0_A1
#define CR1_FF800_INPUT1_REAR               CR1_INPUT_OPT0_B

// Configuration register 2
#define CR2_CLOCKMODE_AUTOSYNC  0x00000000
#define CR2_CLOCKMODE_MASTER    0x00000001
#define CR2_FREQ0               0x00000002
#define CR2_FREQ1               0x00000004
#define CR2_DSPEED              0x00000008
#define CR2_QSSPEED             0x00000010
#define CR2_SPDIF_OUT_PRO       0x00000020
#define CR2_SPDIF_OUT_EMP       0x00000040
#define CR2_SPDIF_OUT_NONAUDIO  0x00000080
#define CR2_SPDIF_OUT_ADAT2     0x00000100  // Optical SPDIF on ADAT2 port
#define CR2_SPDIF_IN_COAX       0x00000000
#define CR2_SPDIF_IN_ADAT2      0x00000200  // Optical SPDIF on ADAT2 port
#define CR2_SYNC_REF0           0x00000400
#define CR2_SYNC_REF1           0x00000800
#define CR2_SYNC_REF2           0x00001000
#define CR2_WORD_CLOCK_1x       0x00002000
#define CR2_TOGGLE_TCO          0x00004000  // Normally set to 0
#define CR2_P12DB_AN0           0x00010000  // Disable soft-limiter.  Normally set to 0
#define CR2_FF400_BIT           0x04000000  // Set on FF400, clear on FF800
#define CR2_TMS                 0x40000000  // Unit option, normally 0
#define CR2_DROP_AND_STOP       0x80000000  // Normally set to 1

#define CR2_SYNC_ADAT1          0x0
#define CR2_SYNC_ADAT2          (CR2_SYNC_REF0)
#define CR2_SYNC_SPDIF          (CR2_SYNC_REF0 | CR2_SYNC_REF1)
#define CR2_SYNC_WORDCLOCK      (CR2_SYNC_REF2)
#define CR2_SYNC_TCO            (CR2_SYNC_REF0 | CR2_SYNC_REF2)
#define CR2_DISABLE_LIMITER     CR2_P12DB_AN0

/* Defines for the status registers */
// Status register 0
#define SR0_ADAT1_LOCK          0x00000400
#define SR0_ADAT2_LOCK          0x00000800
#define SR0_ADAT1_SYNC          0x00001000
#define SR0_ADAT2_SYNC          0x00002000
#define SR0_SPDIF_F0            0x00004000
#define SR0_SPDIF_F1            0x00008000
#define SR0_SPDIF_F2            0x00010000
#define SR0_SPDIF_F3            0x00020000
#define SR0_SPDIF_SYNC          0x00040000
#define SR0_OVER                0x00080000
#define SR0_SPDIF_LOCK          0x00100000
#define SR0_SEL_SYNC_REF0       0x00400000
#define SR0_SEL_SYNC_REF1       0x00800000
#define SR0_SEL_SYNC_REF2       0x01000000
#define SR0_INP_FREQ0           0x02000000
#define SR0_INP_FREQ1           0x04000000
#define SR0_INP_FREQ2           0x08000000
#define SR0_INP_FREQ3           0x10000000
#define SR0_WCLK_SYNC           0x20000000
#define SR0_WCLK_LOCK           0x40000000

// The lowest 10 bits of SR0 represent sample_rate/250 if locked to an
// external clock source.
#define SR0_STREAMING_FREQ_MASK 0x000003ff

#define SR0_ADAT1_STATUS_MASK   (SR0_ADAT1_LOCK|SR0_ADAT1_SYNC)
#define SR0_ADAT1_STATUS_NOLOCK 0
#define SR0_ADAT1_STATUS_LOCK   SR0_ADAT1_LOCK
#define SR0_ADAT1_STATUS_SYNC   (SR0_ADAT1_LOCK|SR0_ADAT1_SYNC)
#define SR0_ADAT2_STATUS_MASK   (SR0_ADAT2_LOCK|SR0_ADAT2_SYNC)
#define SR0_ADAT2_STATUS_NOLOCK 0
#define SR0_ADAT2_STATUS_LOCK   SR0_ADAT2_LOCK
#define SR0_ADAT2_STATUS_SYNC   (SR0_ADAT2_LOCK|SR0_ADAT2_SYNC)
#define SR0_SPDIF_STATUS_MASK   (SR0_SPDIF_LOCK|SR0_SPDIF_SYNC)
#define SR0_SPDIF_STATUS_NOLOCK 0
#define SR0_SPDIF_STATUS_LOCK   SR0_SPDIF_LOCK
#define SR0_SPDIF_STATUS_SYNC   (SR0_SPDIF_LOCK|SR0_SPDIF_SYNC)
#define SR0_WCLK_STATUS_MASK    (SR0_WCLK_LOCK|SR0_WCLK_SYNC)
#define SR0_WCLK_STATUS_NOLOCK  0
#define SR0_WCLK_STATUS_LOCK    SR0_WCLK_LOCK
#define SR0_WCLK_STATUS_SYNC    (SR0_WCLK_LOCK|SR0_WCLK_SYNC)

#define SR0_SPDIF_FREQ_MASK     (SR0_SPDIF_F0|SR0_SPDIF_F1|SR0_SPDIF_F2|SR0_SPDIF_F3)
#define SR0_SPDIF_FREQ_32k      SR0_SPDIF_F0
#define SR0_SPDIF_FREQ_44k1     SR0_SPDIF_F1
#define SR0_SPDIF_FREQ_48k      (SR0_SPDIF_F0|SR0_SPDIF_F1)
#define SR0_SPDIF_FREQ_64k      SR0_SPDIF_F2
#define SR0_SPDIF_FREQ_88k2     (SR0_SPDIF_F0|SR0_SPDIF_F2)
#define SR0_SPDIF_FREQ_96k      (SR0_SPDIF_F1|SR0_SPDIF_F2)
#define SR0_SPDIF_FREQ_128k     (SR0_SPDIF_F0|SR0_SPDIF_F1|SR0_SPDIF_F2)
#define SR0_SPDIF_FREQ_176k4    SR0_SPDIF_F3
#define SR0_SPDIF_FREQ_192k     (SR0_SPDIF_F0|SR0_SPDIF_F3)

#define SR0_AUTOSYNC_SRC_MASK   (SR0_SEL_SYNC_REF0|SR0_SEL_SYNC_REF1|SR0_SEL_SYNC_REF2)
#define SR0_AUTOSYNC_SRC_ADAT1  0
#define SR0_AUTOSYNC_SRC_ADAT2  SR0_SEL_SYNC_REF0
#define SR0_AUTOSYNC_SRC_SPDIF  (SR0_SEL_SYNC_REF0|SR0_SEL_SYNC_REF1)
#define SR0_AUTOSYNC_SRC_WCLK   SR0_SEL_SYNC_REF2
#define SR0_AUTOSYNC_SRC_TCO    (SR0_SEL_SYNC_REF0|SR0_SEL_SYNC_REF2)
#define SR0_AUTOSYNC_SRC_NONE   (SR0_SEL_SYNC_REF1|SR0_SEL_SYNC_REF2)

#define SR0_AUTOSYNC_FREQ_MASK  (SR0_INP_FREQ0|SR0_INP_FREQ1|SR0_INP_FREQ2|SR0_INP_FREQ3)
#define SR0_AUTOSYNC_FREQ_32k   SR0_INP_FREQ0
#define SR0_AUTOSYNC_FREQ_44k1  SR0_INP_FREQ1
#define SR0_AUTOSYNC_FREQ_48k   (SR0_INP_FREQ0|SR0_INP_FREQ1)
#define SR0_AUTOSYNC_FREQ_64k   SR0_INP_FREQ2
#define SR0_AUTOSYNC_FREQ_88k2  (SR0_INP_FREQ0|SR0_INP_FREQ2)
#define SR0_AUTOSYNC_FREQ_96k   (SR0_INP_FREQ1|SR0_INP_FREQ2)
#define SR0_AUTOSYNC_FREQ_128k  (SR0_INP_FREQ0|SR0_INP_FREQ1|SR0_INP_FREQ2)
#define SR0_AUTOSYNC_FREQ_176k4 SR0_INP_FREQ3
#define SR0_AUTOSYNC_FREQ_192k  (SR0_INP_FREQ0|SR0_INP_FREQ3)
#define SR0_AUTOSYNC_FREQ_NONE  0

// Status register 1
#define SR1_CLOCK_MODE_MASTER   0x00000001
#define SR1_TCO_SYNC            0x00400000
#define SR1_TCO_LOCK            0x00800000

#define SR1_TCO_STATUS_MASK    (SR1_TCO_LOCK|SR1_TCO_SYNC)
#define SR1_TCO_STATUS_NOLOCK  0
#define SR1_TCO_STATUS_LOCK    SR1_TCO_LOCK
#define SR1_TCO_STATUS_SYNC    (SR1_TCO_LOCK|SR1_TCO_SYNC)

/* Structure used to store device settings in the device flash RAM.  This
 * structure mirrors the layout in the Fireface's flash, so it cannot be
 * altered.  Fields named as unused_* are not utilised at present.
 */
typedef struct {
    uint32_t unused_device_id;
    uint32_t unused_device_rev;
    uint32_t unused_asio_latency;
    uint32_t unused_samples_per_frame;
    uint32_t spdif_input_mode;
    uint32_t spdif_output_emphasis;
    uint32_t spdif_output_pro;
    uint32_t clock_mode;
    uint32_t spdif_output_nonaudio;
    uint32_t sync_ref;
    uint32_t spdif_output_mode;
    uint32_t unused_check_input;
    uint32_t unused_status;
    uint32_t unused_register[4];
    uint32_t unused_iso_rx_channel;
    uint32_t unused_iso_tx_channel;
    uint32_t unused_timecode;
    uint32_t unused_device_type;
    uint32_t unused_number_of_devices;
    uint32_t tms;
    uint32_t unused_speed;
    uint32_t unused_channels_avail_hi;
    uint32_t unused_channels_avail_lo;
    uint32_t limit_bandwidth;
    uint32_t unused_bandwidth_allocated;
    uint32_t stop_on_dropout;
    uint32_t input_level;
    uint32_t output_level;
    uint32_t mic_plug_select[2];     // Front/rear select for FF800 ch 7/8
                                     // [0] = phones level on FF400
    uint32_t mic_phantom[4];
    uint32_t instrument_plug_select; // Front/rear select for FF800 ch 1
    uint32_t filter;
    uint32_t fuzz;
    uint32_t unused_sync_align;
    uint32_t unused_device_index;
    uint32_t unused_advanced_dialog;
    uint32_t sample_rate;
    uint32_t unused_interleaved;
    uint32_t unused_sn;
    uint32_t word_clock_single_speed;
    uint32_t unused_num_channels;
    uint32_t unused_dropped_samples;
    uint32_t p12db_an[10];
} FF_device_flash_settings_t;

// Defines used to interpret device flash settings.  The driver can read 
// these and use them to infer the appropriate values for the configuration
// registers.  The flash settings are also used directly by the device
// at power up to define its initial state.  Therefore it's important that
// these settings correspond to the values expected by the device.
#define FF_DEV_FLASH_INVALID                   0xffffffff
#define FF_DEV_FLASH_SPDIF_INPUT_COAX          0x00000001
#define FF_DEV_FLASH_SPDIF_INPUT_OPTICAL       0x00000000
#define FF_DEV_FLASH_SPDIF_OUTPUT_COAX         0x00000000
#define FF_DEV_FLASH_SPDIF_OUTPUT_OPTICAL      0x00000001
#define FF_DEV_FLASH_SPDIF_OUTPUT_EMPHASIS_ON  0x00000001
#define FF_DEV_FLASH_SPDIF_OUTPUT_PRO_ON       0x00000001
#define FF_DEV_FLASH_SPDIF_OUTPUT_NONAUDIO_ON  0x00000001
#define FF_DEV_FLASH_BWLIMIT_SEND_ALL_CHANNELS 0x00000001
#define FF_DEV_FLASH_BWLIMIT_NO_ADAT2          0x00000002  // FF800 only
#define FF_DEV_FLASH_BWLIMIT_ANALOG_SPDIF_ONLY 0x00000003
#define FF_DEV_FLASH_BWLIMIT_ANALOG_ONLY       0x00000004
#define FF_DEV_FLASH_CLOCK_MODE_MASTER         0x00000000
#define FF_DEV_FLASH_CLOCK_MODE_AUTOSYNC       0x00000001
#define FF_DEV_FLASH_CLOCK_MODE_SLAVE          0x00000001
#define FF_DEV_FLASH_SYNCREF_WORDCLOCK         0x00000001
#define FF_DEV_FLASH_SYNCREF_ADAT1             0x00000002
#define FF_DEV_FLASH_SYNCREF_ADAT2             0x00000003
#define FF_DEV_FLASH_SYNCREF_SPDIF             0x00000004
#define FF_DEV_FLASH_SYNCREC_TCO               0x00000005
#define FF_DEV_FLASH_ILEVEL_LOGAIN             0x00000000
#define FF_DEV_FLASH_ILEVEL_4dBU               0x00000002
#define FF_DEV_FLASH_ILEVEL_m10dBV             0x00000001
#define FF_DEV_FLASH_OLEVEL_HIGAIN             0x00000002
#define FF_DEV_FLASH_OLEVEL_4dBU               0x00000001
#define FF_DEV_FLASH_OLEVEL_m10dBV             0x00000000
#define FF_DEV_FLASH_MIC_PHANTOM_ON            0x00000001
#define FF_DEV_FLASH_SRATE_DDS_INACTIVE        0x00000000
#define FF_DEV_FLASH_WORD_CLOCK_1x             0x00000001
#define FF_DEV_FLASH_PLUG_SELECT_FRONT_REAR    0x00000002
#define FF_DEV_FLASH_PLUG_SELECT_FRONT         0x00000001
#define FF_DEV_FLASH_PLUG_SELECT_REAR          0x00000000

#define FF_MATRIXMIXER_SIZE (RME_FF800_MAX_CHANNELS*RME_FF800_MAX_CHANNELS)

// Structure used by FFADO to keep track of the device status.  This is
// decoupled from any structures used directly by the device, so it can be
// added to and ordered freely.  When making changes to the device the
// configuration registers must be all written to, so any function changing
// a parameter must have access to the complete device status.
typedef struct {
    uint32_t mic_phantom[4];
    uint32_t spdif_input_mode;
    uint32_t spdif_output_emphasis;
    uint32_t spdif_output_pro;
    uint32_t spdif_output_nonaudio;
    uint32_t spdif_output_mode;
    uint32_t clock_mode;
    uint32_t sync_ref;
    uint32_t tms;
    uint32_t limit_bandwidth;
    uint32_t stop_on_dropout;
    uint32_t input_level;
    uint32_t output_level;
    uint32_t filter;
    uint32_t fuzz;
    uint32_t limiter;
    uint32_t sample_rate;
    uint32_t word_clock_single_speed;
    uint32_t ff400_input_pad[2];       // Channels 3/4, FF400 only
    uint32_t ff400_instr_input[2];     // Channels 3/4, FF400 only
    uint32_t phones_level;             // Derived from fields in device flash
    uint32_t input_opt[3];             // Derived from fields in device flash

    // Other "settings" fields which are not necessarily stored in device flash
    int32_t amp_gains[22];             // FF400: gains of input/output amps
    int32_t input_faders[FF_MATRIXMIXER_SIZE];
    int32_t playback_faders[FF_MATRIXMIXER_SIZE];
    int32_t output_faders[RME_FF800_MAX_CHANNELS];
    unsigned char input_mixerflags[FF_MATRIXMIXER_SIZE];
    unsigned char playback_mixerflags[FF_MATRIXMIXER_SIZE];
    unsigned char output_mixerflags[FF_MATRIXMIXER_SIZE];
} FF_software_settings_t;

// Defines used to interpret the software settings structure.  For now we
// use the same values as used by the device flash settings to remove the
// need for translation between reading the flash and copying it to the
// software settings structure, but in principle different values could be
// used given translation code.
#define FF_SWPARAM_INVALID                     FF_DEV_FLASH_INVALID
#define FF_SWPARAM_SPDIF_INPUT_COAX            FF_DEV_FLASH_SPDIF_INPUT_COAX
#define FF_SWPARAM_SPDIF_INPUT_OPTICAL         FF_DEV_FLASH_SPDIF_INPUT_OPTICAL
#define FF_SWPARAM_SPDIF_OUTPUT_COAX           FF_DEV_FLASH_SPDIF_OUTPUT_COAX
#define FF_SWPARAM_SPDIF_OUTPUT_OPTICAL        FF_DEV_FLASH_SPDIF_OUTPUT_OPTICAL
#define FF_SWPARAM_SPDIF_OUTPUT_EMPHASIS_ON    FF_DEV_FLASH_SPDIF_OUTPUT_EMPHASIS_ON
#define FF_SWPARAM_SPDIF_OUTPUT_PRO_ON         FF_DEV_FLASH_SPDIF_OUTPUT_PRO_ON
#define FF_SWPARAM_SPDIF_OUTPUT_NONAUDIO_ON    FF_DEV_FLASH_SPDIF_OUTPUT_NONAUDIO_ON
#define FF_SWPARAM_BWLIMIT_SEND_ALL_CHANNELS   FF_DEV_FLASH_BWLIMIT_SEND_ALL_CHANNELS
#define FF_SWPARAM_BWLIMIT_NO_ADAT2            FF_DEV_FLASH_BWLIMIT_NO_ADAT2
#define FF_SWPARAM_BWLIMIT_ANALOG_SPDIF_ONLY   FF_DEV_FLASH_BWLIMIT_ANALOG_SPDIF_ONLY
#define FF_SWPARAM_BWLIMIT_ANALOG_ONLY         FF_DEV_FLASH_BWLIMIT_ANALOG_ONLY
#define FF_SWPARAM_CLOCK_MODE_MASTER           FF_DEV_FLASH_CLOCK_MODE_MASTER
#define FF_SWPARAM_CLOCK_MODE_AUTOSYNC         FF_DEV_FLASH_CLOCK_MODE_AUTOSYNC
#define FF_SWPARAM_CLOCK_MODE_SLAVE            FF_DEV_FLASH_CLOCK_MODE_SLAVE
#define FF_SWPARAM_SYNCREF_WORDCLOCK           FF_DEV_FLASH_SYNCREF_WORDCLOCK
#define FF_SWPARAM_SYNCREF_ADAT1               FF_DEV_FLASH_SYNCREF_ADAT1
#define FF_SWPARAM_SYNCREF_ADAT2               FF_DEV_FLASH_SYNCREF_ADAT2
#define FF_SWPARAM_SYNCREF_SPDIF               FF_DEV_FLASH_SYNCREF_SPDIF
#define FF_SWPARAM_SYNCREC_TCO                 FF_DEV_FLASH_SYNCREC_TCO
#define FF_SWPARAM_ILEVEL_LOGAIN               FF_DEV_FLASH_ILEVEL_LOGAIN
#define FF_SWPARAM_ILEVEL_4dBU                 FF_DEV_FLASH_ILEVEL_4dBU
#define FF_SWPARAM_ILEVEL_m10dBV               FF_DEV_FLASH_ILEVEL_m10dBV
#define FF_SWPARAM_OLEVEL_HIGAIN               FF_DEV_FLASH_OLEVEL_HIGAIN
#define FF_SWPARAM_OLEVEL_4dBU                 FF_DEV_FLASH_OLEVEL_4dBU
#define FF_SWPARAM_OLEVEL_m10dBV               FF_DEV_FLASH_OLEVEL_m10dBV
#define FF_SWPARAM_MIC_PHANTOM_ON              FF_DEV_FLASH_MIC_PHANTOM_ON
#define FF_SWPARAM_WORD_CLOCK_1x               FF_DEV_FLASH_WORD_CLOCK_1x
#define FF_SWPARAM_SRATE_DDS_INACTIVE          FF_DEV_FLASH_SRATE_DDS_INACTIVE
//
// The following defines refer to fields in the software parameter record
// which are derived from one or more fields in device flash.
#define FF_SWPARAM_PHONESLEVEL_HIGAIN          0x00000000
#define FF_SWPARAM_PHONESLEVEL_4dBU            0x00000001
#define FF_SWPARAM_PHONESLEVEL_m10dBV          0x00000002
#define FF_SWPARAM_INPUT_OPT_B                 0x00000001
#define FF_SWPARAM_INPUT_OPT_A                 0x00000002

#define FF_SWPARAM_FF800_INPUT_OPT_FRONT       FF_SWPARAM_INPUT_OPT_A
#define FF_SWPARAM_FF800_INPUT_OPT_REAR        FF_SWPARAM_INPUT_OPT_B

// Flags for the "status" parameter of setInputInstrOpt()
#define FF400_INSTR_OPT_ACTIVE  0x01
#define FF800_INSTR_OPT_FILTER  0x02
#define FF800_INSTR_OPT_FUZZ    0x04
#define FF800_INSTR_OPT_LIMITER 0x08

// Flags for the *_mixerflags fields
#define FF_SWPARAM_MF_NORMAL    0x00
#define FF_SWPARAM_MF_MUTED     0x01
#define FF_SWPARAM_MF_INVERTED  0x02    // Inputs/playbacks only
#define FF_SWPARAM_MF_REC       0x04    // Outputs only

// Indices into the amp_gains array
#define FF400_AMPGAIN_MIC1      0
#define FF400_AMPGAIN_MIC2      1
#define FF400_AMPGAIN_INPUT3    2
#define FF400_AMPGAIN_INPUT4    3
#define FF400_AMPGAIN_OUTPUT1   4
#define FF400_AMPGAIN_OUTPUT2   5
#define FF400_AMPGAIN_OUTPUT3   6
#define FF400_AMPGAIN_OUTPUT4   7
#define FF400_AMPGAIN_OUTPUT5   8
#define FF400_AMPGAIN_OUTPUT6   9
#define FF400_AMPGAIN_PHONES_L 10
#define FF400_AMPGAIN_PHONES_R 11
#define FF400_AMPGAIN_SPDIF1   12
#define FF400_AMPGAIN_SPDIF2   13
#define FF400_AMPGAIN_ADAT1_1  14
#define FF400_AMPGAIN_ADAT1_2  15
#define FF400_AMPGAIN_ADAT1_3  16
#define FF400_AMPGAIN_ADAT1_4  17
#define FF400_AMPGAIN_ADAT1_5  18
#define FF400_AMPGAIN_ADAT1_6  19
#define FF400_AMPGAIN_ADAT1_7  20
#define FF400_AMPGAIN_ADAT1_8  21
#define FF400_AMPGAIN_NUM      22

// The general Fireface state
typedef struct {
    uint32_t is_streaming;
    uint32_t clock_mode;
    uint32_t autosync_source;
    uint32_t autosync_freq;
    uint32_t spdif_freq;
    uint32_t adat1_sync_status, adat2_sync_status;
    uint32_t spdif_sync_status;
    uint32_t wclk_sync_status, tco_sync_status;
} FF_state_t;

#define FF_STATE_CLOCKMODE_MASTER              0
#define FF_STATE_CLOCKMODE_AUTOSYNC            1
#define FF_STATE_AUTOSYNC_SRC_NOLOCK           0
#define FF_STATE_AUTOSYNC_SRC_ADAT1            1
#define FF_STATE_AUTOSYNC_SRC_ADAT2            2
#define FF_STATE_AUTOSYNC_SRC_SPDIF            3
#define FF_STATE_AUTOSYNC_SRC_WCLK             4
#define FF_STATE_AUTOSYNC_SRC_TCO              5
#define FF_STATE_SYNC_NOLOCK                   0
#define FF_STATE_SYNC_LOCKED                   1
#define FF_STATE_SYNC_SYNCED                   2

// Data structure for the TCO (Time Code Option) state
typedef struct {
    uint32_t input;
    uint32_t frame_rate;
    uint32_t word_clock;
    uint32_t sample_rate;
    uint32_t pull;
    uint32_t termination;
    uint32_t MTC;
} FF_TCO_settings_t;

// Defines used to configure selected quadlets of the TCO write space.  Some
// of these are also used when configuring the TCO.  The byte indices
// referenced in the define names are 0-based.

// TCO quadlet 0
#define FF_TCO0_MTC                           0x80000000

// TCO quadlet 1
#define FF_TCO1_TCO_lock                      0x00000001
#define FF_TCO1_WORD_CLOCK_INPUT_RATE0        0x00000002
#define FF_TCO1_WORD_CLOCK_INPUT_RATE1        0x00000004
#define FF_TCO1_LTC_INPUT_VALID               0x00000008
#define FF_TCO1_WORD_CLOCK_INPUT_VALID        0x00000010
#define FF_TCO1_VIDEO_INPUT_NTSC              0x00000020
#define FF_TCO1_VIDEO_INPUT_PAL               0x00000040
#define FF_TCO1_SET_TC                        0x00000100
#define FF_TCO1_SET_DROPFRAME                 0x00000200
#define FF_TCO1_LTC_FORMAT0                   0x00000400
#define FF_TCO1_LTC_FORMAT1                   0x00000800

#define FF_TCO1_WORD_CLOCK_INPUT_1x           0
#define FF_TCO1_WORD_CLOCK_INPUT_2x           FF_TCO1_WORD_CLOCK_INPUT_RATE0
#define FF_TCO1_WORD_CLOCK_INPUT_4x           FF_TCO1_WORD_CLOCK_INPUT_RATE1
#define FF_TCO1_WORD_CLOCK_INPUT_MASK         (FF_TCO1_WORD_CLOCK_INPUT_RATE0|FF_TCO1_WORD_CLOCK_INPUT_RATE1)
#define FF_TCO1_VIDEO_INPUT_MASK              (FF_TCO1_VIDEO_INPUT_NTSC|FF_TCO1_VIDEO_INPUT_PAL)
#define FF_TC01_LTC_FORMAT_24fps              0
#define FF_TCO1_LTC_FORMAT_25fps              FF_TCO1_LTC_FORMAT0
#define FF_TC01_LTC_FORMAT_29_97fps           FF_TCO1_LTC_FORMAT1
#define FF_TCO1_LTC_FORMAT_29_97dpfs          (FF_TCO1_LTC_FORMAT1|FF_TCO1_SET_DROPFRAME)
#define FF_TCO1_LTC_FORMAT_30fps              (FF_TCO1_LTC_FORMAT0|FF_TCO1_LTC_FORMAT1)
#define FF_TCO1_LTC_FORMAT_30dfps             (FF_TCO1_LTC_FORMAT0|FF_TCO1_LTC_FORMAT1|FF_TCO1_SET_DROPFRAME)
#define FF_TCO1_LTC_FORMAT_MASK               (FF_TCO1_LTC_FORMAT0|FF_TCO1_LTC_FORMAT1)

// TCO quadlet 2
#define FF_TCO2_TC_RUN                        0x00010000
#define FF_TCO2_WORD_CLOCK_CONV0              0x00020000
#define FF_TCO2_WORD_CLOCK_CONV1              0x00040000
#define FF_TCO2_NUM_DROPFRAMES0               0x00080000 // Unused
#define FF_TCO2_NUM_DROPFRAMES1               0x00100000 // Unused
#define FF_TCO2_SET_JAM_SYNC                  0x00200000
#define FF_TCO2_SET_FLYWHEEL                  0x00400000
#define FF_TCO2_SET_01_4                      0x01000000
#define FF_TCO2_SET_PULLDOWN                  0x02000000
#define FF_TCO2_SET_PULLUP                    0x04000000
#define FF_TCO2_SET_FREQ                      0x08000000
#define FF_TCO2_SET_TERMINATION               0x10000000
#define FF_TCO2_SET_INPUT0                    0x20000000
#define FF_TCO2_SET_INPUT1                    0x40000000
#define FF_TCO2_SET_FREQ_FROM_APP             0x80000000

#define FF_TCO2_WORD_CLOCK_CONV_1_1           0
#define FF_TCO2_WORD_CLOCK_CONV_44_48         FF_TCO2_WORD_CLOCK_CONV0
#define FF_TCO2_WORD_CLOCK_CONV_48_44         FF_TCO2_WORD_CLOCK_CONV1
#define FF_TCO2_PULL_0                        0
#define FF_TCO2_PULL_UP_01                    FF_TCO2_SET_PULLUP
#define FF_TCO2_PULL_DOWN_01                  FF_TCO2_SET_PULLDOWN
#define FF_TCO2_PULL_UP_40                    (FF_TCO2_SET_PULLUP|FF_TCO2_SET_01_4)
#define FF_TCO2_PULL_DOWN_40                  (FF_TCO2_SET_PULLDOWN|FF_TCO2_SET_01_4)
#define FF_TCO2_INPUT_LTC                     FF_TCO2_SET_INPUT1
#define FF_TCO2_INPUT_VIDEO                   FF_TCO2_SET_INPUT0
#define FF_TCO2_INPUT_WORD_CLOCK              0
#define FF_TCO2_SRATE_44_1                    0
#define FF_TCO2_SRATE_48                      FF_TCO2_SET_FREQ
#define FF_TCO2_SRATE_FROM_APP                FF_TCO2_SET_FREQ_FROM_APP

// Interpretation of the TCO software settings fields
#define FF_TCOPARAM_INPUT_LTC                 1
#define FF_TCOPARAM_INPUT_VIDEO               2
#define FF_TCOPARAM_INPUT_WCK                 3
#define FF_TCOPARAM_FRAMERATE_24fps           1
#define FF_TCOPARAM_FRAMERATE_25fps           2
#define FF_TCOPARAM_FRAMERATE_29_97fps        3
#define FF_TCOPARAM_FRAMERATE_29_97dfps       4
#define FF_TCOPARAM_FRAMERATE_30fps           5
#define FF_TCOPARAM_FRAMERATE_30dfps          6
#define FF_TCOPARAM_WORD_CLOCK_CONV_1_1       1     // 1:1
#define FF_TCOPARAM_WORD_CLOCK_CONV_44_48     2     // 44.1 kHz-> 48 kHz
#define FF_TCOPARAM_WORD_CLOCK_CONV_48_44     3     // 48 kHz -> 44.1 kHz
#define FF_TCOPARAM_SRATE_44_1                1     // Rate is 44.1 kHz
#define FF_TCOPARAM_SRATE_48                  2     // Rate is 48 kHz
#define FF_TCOPARAM_SRATE_FROM_APP            3
#define FF_TCPPARAM_PULL_NONE                 1
#define FF_TCOPARAM_PULL_UP_01                2     // +0.1%
#define FF_TCOPARAM_PULL_DOWN_01              3     // -0.1%
#define FF_TCOPARAM_PULL_UP_40                4     // +4.0%
#define FF_TCOPARAM_PULL_DOWN_40              5     // -4.0%
#define FF_TCOPARAM_TERMINATION_ON            1

// The state of the TCO
typedef struct {
  unsigned int locked, ltc_valid;
  unsigned int hours, minutes, seconds, frames;
  unsigned int frame_rate;
  unsigned int drop_frame;
  unsigned int video_input;
  unsigned int word_clock_state;
  float sample_rate;
} FF_TCO_state_t;

// TCO state field defines
#define FF_TCOSTATE_FRAMERATE_24fps           1
#define FF_TCOSTATE_FRAMERATE_25fps           2
#define FF_TCOSTATE_FRAMERATE_29_97fps        3
#define FF_TCOSTATE_FRAMERATE_30fps           4
#define FF_TCOSTATE_VIDEO_NONE                0
#define FF_TCOSTATE_VIDEO_PAL                 1
#define FF_TCOSTATE_VIDEO_NTSC                2
#define FF_TCOSTATE_WORDCLOCK_NONE            0
#define FF_TCOSTATE_WORDCLOCK_1x              1
#define FF_TCOSTATE_WORDCLOCK_2x              2
#define FF_TCOSTATE_WORDCLOCK_4x              3

#endif
