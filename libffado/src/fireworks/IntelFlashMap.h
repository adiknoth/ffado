// Copyright ECHO AUDIO
//
// defines for Fireworks flash
// Using bottom boot Intel TE28F160C3 flash part
//
#ifndef _INTEL_BOTTOM_BOOT_MAP_H_
#define _INTEL_BOTTOM_BOOT_MAP_H_

//-----------------------------------------------------------------------------
//
// Defines for Intel flash part
//
//-----------------------------------------------------------------------------

#define PROGRAMBLOCK_SIZE_WORD16			0x1000 // 16-bit words
#define MAINBLOCK_SIZE_WORD16				0x8000 // 16-bit words

#define PROGRAMBLOCK_SIZE_BYTES				(PROGRAMBLOCK_SIZE_WORD16 * 2)
#define MAINBLOCK_SIZE_BYTES				(MAINBLOCK_SIZE_WORD16 * 2)

#define PROGRAMBLOCK_SIZE_QUADS				(PROGRAMBLOCK_SIZE_WORD16 / 2)
#define MAINBLOCK_SIZE_QUADS				(MAINBLOCK_SIZE_WORD16 / 2)

#define MAINBLOCKS_BASE_OFFSET_BYTES		0x10000
		
#define FLASH_SIZE_BYTES					0x200000	// 2 MB
#define FLASH_SIZE_QUADS					(FLASH_SIZE_BYTES / 4)


//-----------------------------------------------------------------------------
//
// memory map
//
// Small blocks (8 kbytes each) from 0 - 0xffff
// Large blocks (32 kbytes each) from 0x010000 - 0x1fffff
//
//-----------------------------------------------------------------------------

//
// Fireworks 2.1
//
#define DSP_EMERGENCY_IMAGE_OFFSET_BYTES_FW21	0x00140000
#define ARM_IMAGE_OFFSET_BYTES_FW21				0x00100000
#define DSP_IMAGE_OFFSET_BYTES_FW21				0x000C0000
#define FPGA_IMAGE_OFFSET_BYTES_FW21			0x00080000
#define BOOT_IMAGE_OFFSET_BYTES_FW21			0x00000000

#define SESSION_OFFSET_BYTES_FW21				0x00008000

#define ARM_IMAGE_OFFSET_QUADS_FW21				(ARM_IMAGE_OFFSET_BYTES_FW21/4)
#define DSP_IMAGE_OFFSET_QUADS_FW21				(DSP_IMAGE_OFFSET_BYTES_FW21/4)
#define BOOT_IMAGE_OFFSET_QUADS_FW21			(BOOT_IMAGE_OFFSET_BYTES_FW21/4)


//
// Fireworks 3
//
#define FPGA_IMAGE_OFFSET_BYTES_FW3			0x00000000
#define ARM_IMAGE_OFFSET_BYTES_FW3			0x00100000
#define NAME_BLOCK_OFFSET_BYTES_FW3			0x001E0000
#define SESSION_OFFSET_BYTES_FW3			0x001F0000

//
// Fireworks HDMI
//
#define FWHDMI_CLOCK_RATIOS_OFFSET_BYTES	0x00006000


#endif // _INTEL_BOTTOM_BOOT_MAP_H_

