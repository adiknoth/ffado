/*
 * Copyright (C) 2012 by Peter Hinkel
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
 * Implementation of the DICE firmware loader specification based upon
 * TCAT public available document v3.2.0.0 (2008-06-20)
 *
 */

#ifndef __DICE_FIRMWARE_LOADER_H__
#define __DICE_FIRMWARE_LOADER_H__

#include <stdint.h>

/* private memory space offset for command interface */
#define DICE_FL_INTERFACE_SPACE	0x0000FFFFE0100000ULL

/* memory space offsets relative to DICE_FL_INTERFACE_SPACE */
#define DICE_FL_VERSION			0x0 //unused
#define DICE_FL_OPCODE			0x4
#define DICE_FL_RETURN_STATUS	0x8
#define DICE_FL_PROGRESS		0xC
#define DICE_FL_CAPABILITIES	0x10
#define DICE_FL_RESERVED		0x14 //unused
#define DICE_FL_PARAMETER		0x2C
#define DICE_FL_TESTDELAY		0xFD8
#define DICE_FL_TESTBUF			0xFDC

#define DICE_FL_BUFFER			0x34 //offset for Upload buffer

/* Opcode IDs for implemented functions (firmware dependent) */
#define	DICE_FL_OP_GET_IMAGE_DESC			0x0		// parameters: imageId  return: imageDesc
#define	DICE_FL_OP_DELETE_IMAGE				0x1		// parameters: name  return: none
#define	DICE_FL_OP_CREATE_IMAGE				0x2		// parameters: execAddr, entryAddr, name  return: none
#define	DICE_FL_OP_UPLOAD					0x3		// parameters: index, length, data, return none
#define	DICE_FL_OP_UPLOAD_STAT				0x4		// parameters: length return checksum
#define	DICE_FL_OP_RESET_IMAGE				0x5		// parameters: none
#define	DICE_FL_OP_TEST_ACTION				0x6		// parameters: none
#define	DICE_FL_OP_GET_FLASH_INFO			0x7		// parameters: none
#define	DICE_FL_OP_READ_MEMORY				0x8		// parameters: none
#define	DICE_FL_OP_NOOP						0x9		// parameters: none
#define	DICE_FL_OP_GET_RUNNING_IMAGE_VINFO	0xA		// parameters: none  return: vendorInfo
#define	DICE_FL_OP_CREATE_IMAGE2			0xB		// parameters: none  return: vendorInfo
#define	DICE_FL_OP_GET_APP_INFO				0xC		// parameters: none  return: new info structure

/* return status #defines */
#define DICE_FL_RETURN_NO_ERROR			0x00000000	//Operation successful
#define DICE_FL_E_GEN_NOMATCH			0xFF000000	//for Opcode ID: 0x0
#define DICE_FL_E_FIS_ILLEGAL_IMAGE		0xC5000001	//for Opcode ID: 0x1, 0x2
//#define DICE_FL_E_FIS_FLASH_OP_FAILED	0x00000000	//for Opcode ID: 0x1, 0x2
//#define DICE_FL_E_FIS_NO_SPACE			0x00000000	//for Opcode ID: 0x2
#define DICE_FL_E_BAD_INPUT_PARAM		0xC3000003	//for Opcode ID: 0x3
//#define DICE_FL_E_FAIL					0x00000000	//for Opcode ID: 0x6

/* data structure #defines */
#define MAX_IMAGE_NAME	16

/* data structures input parameters */
typedef struct {
	uint32_t	length;
	uint32_t	execAddr;
	uint32_t	entryAddr;
	char		name[MAX_IMAGE_NAME];
} DICE_FL_CREATE_IMAGE_PARAM;

typedef struct {
	char		name[MAX_IMAGE_NAME];
} DICE_FL_DELETE_IMAGE_PARAM;

//decomposition of upload parameter due to limited transfer-bandwidth (max 128 quadlets = 512 bytes)
//header part of upload parameter
typedef struct {
	uint32_t	index;
	uint32_t	length;
} DICE_FL_UPLOAD_HEADER_PARAM;

//data part of upload parameter
typedef struct {
	uint32_t	buffer[128];
} DICE_FL_UPLOAD_DATA_PARAM;

typedef struct {
	uint32_t	cmdID;
	uint32_t	lvalue0;
	uint32_t	lvalue1;
} DICE_FL_TEST_ACTION_PARAM;

typedef struct {
	uint32_t uiStartAddress;
	uint32_t uiEndAddress;
	uint32_t uiNumBlocks;
	uint32_t uiBlockSize;
} DICE_FL_INFO_PARAM;

typedef struct {
	uint32_t uiStartAddress;
	uint32_t uiLen;
	char ReadBuffer[500]/*[4000]*/;
} DICE_FL_READ_MEMORY;

/* data structures output parameters */
typedef struct {
	char		name[MAX_IMAGE_NAME];
	uint32_t	flashBase;
	uint32_t	memBase;
	uint32_t	size;
	uint32_t	entryPoint;
	uint32_t	length;
	uint32_t	chkSum;
	uint32_t	uiBoardSerialNumber;
	uint32_t 	uiVersionHigh;
	uint32_t	uiVersionLow;
	uint32_t	uiConfigurationFlags;
	char		BuildTime[64];
	char		BuildDate[64];
} DICE_FL_GET_IMAGE_DESC_RETURN;

typedef struct {
	uint32_t	uiProductID;
	char		uiVendorID[8];
	uint32_t	uiVMajor;
	uint32_t	uiVMinor;
	uint32_t	user1;
	uint32_t	user2;
} DICE_FL_GET_VENDOR_IMAGE_DESC_RETURN;

typedef struct {
	uint32_t	data[100];
} DICE_FL_TEST_ACTION_RETURN;

typedef struct
{
	uint32_t	uiBaseSDKVersion;			//The full version/revision of the SDK this build was based on
	uint32_t	uiApplicationVersion;		//The full version/revision of the Application
	uint32_t	uiVendorID;					//The Vendor ID
	uint32_t	uiProductID;				//The product ID
	char		BuildTime[64];				//Build time
	char		BuildDate[64];				//Build date
	uint32_t	uiBoardSerialNumber;		//The serial number of the board as obtained from persist. storage
} DICE_FL_GET_APP_INFO_RETURN;

#endif
