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

#include <fstream>
#include "config.h"

#include "dice/dice_avdevice.h"
#include "dice/dice_firmware_loader.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include "libutil/ByteSwap.h"
#include <libraw1394/csr.h>

#include <stdint.h>
#include <iostream>
#include <sstream>

#include <string>
#include <cstring>
#include <assert.h>

#include "libutil/Configuration.h"
#include "devicemanager.h"

#include "focusrite/saffire_pro40.h"
#include "focusrite/saffire_pro24.h"

using namespace std;

namespace Dice {

fb_quadlet_t tmp_quadlet;

bool
Device::showDiceInfoFL() {

	DICE_FL_GET_VENDOR_IMAGE_DESC_RETURN image_desc;

	writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_GET_RUNNING_IMAGE_VINFO);

	do {
		usleep(10000);
		readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
	} while (tmp_quadlet & (1UL<<31));

	readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

	if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
		readRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &image_desc, sizeof(image_desc));
		printMessage("Dice image vendor and product information:\n");
		printMessage("  uiVProductID: %i\n", image_desc.uiProductID);
		printMessage("  uiVendorID: %s\n", image_desc.uiVendorID);
		printMessage("  uiVMajor: %i\n", image_desc.uiVMajor);
		printMessage("  uiVMajor: %i\n", image_desc.uiVMinor);
		printMessage("  user1: %i\n", image_desc.user1);
		printMessage("  user2: %i\n", image_desc.user2);
	} else {
		printMessage("Cannot read firmware info\n");
	}
	return true;
}

bool
Device::dumpFirmwareFL(const char* filename) {

	DICE_FL_INFO_PARAM* pflash_info = showFlashInfoFL(false);
	DICE_FL_READ_MEMORY memory;

	if (pflash_info) {
		ofstream file(filename, ios::out|ios::binary);

		if (file.is_open()) {

			uint32_t size = pflash_info->uiNumBlocks * pflash_info->uiBlockSize;
			uint32_t start = pflash_info->uiStartAddress;
			uint32_t end = pflash_info->uiEndAddress;

			printMessage("Downloading complete DICE flash into file (flash size = %i KBytes)\n", size/1024);
			printMessage("Please wait, dumping will take about a minute\n");
			printMessage("Dump in progress ...\n");
			while (start<end) {
				memory.uiLen = min<uint32_t>(end-start, sizeof(memory.ReadBuffer));
				memory.uiStartAddress = start;

				writeRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &memory, sizeof(memory));

				writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_READ_MEMORY);

				do {
					usleep(4000);
					readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
				} while (tmp_quadlet & (1UL<<31));

				readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

				if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
					readRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &memory, sizeof(memory));
					file.write(memory.ReadBuffer, memory.uiLen);
				} else {
					printMessage("in dumpFirmwareFL, unknown error =  0x%X \nSTOP.\n", tmp_quadlet);
					return false;
				}
				start += memory.uiLen;
			}
		}
		file.close();
		printMessage("Dumping successfully finished to file %s\n", filename);
		return true;
	} else {
		printMessage("Downloading not supported for this device\n");
		return false;
	}
}

DICE_FL_INFO_PARAM*
Device::showFlashInfoFL(bool v) {

	DICE_FL_INFO_PARAM flash_info;
	DICE_FL_INFO_PARAM* pflash_info = new DICE_FL_INFO_PARAM;

	writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_GET_FLASH_INFO);

	do {
		usleep(10000);
		readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
	} while (tmp_quadlet & (1UL<<31));

	readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

	if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
		readRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &flash_info, sizeof(flash_info));
		if (v) {
			printMessage("Flash Information:\n");
			printMessage("  uiStartAddress: 0x%X\n", flash_info.uiStartAddress);
			printMessage("  uiEndAddress: 0x%X\n", flash_info.uiEndAddress);
			printMessage("  uiNumBlocks: %i\n", flash_info.uiNumBlocks);
			printMessage("  uiBlockSize: %i\n", flash_info.uiBlockSize);
		}
		memcpy(pflash_info, &flash_info, sizeof(flash_info));
		return pflash_info;
	} else {
		printMessage("Cannot read flash information\n");
		return pflash_info = NULL;
	}
}

bool
Device::showImgInfoFL() {

	DICE_FL_GET_IMAGE_DESC_RETURN img_desc;
	uint32_t imageID=0;

	do {
		writeReg(DICE_FL_OFFSET + DICE_FL_PARAMETER, imageID);

		writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_GET_IMAGE_DESC);
		do {
			usleep(100);
			readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
		} while (tmp_quadlet & (1UL<<31));

		readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

		if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
			readRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &img_desc, sizeof(img_desc));
			printMessage("Detailed information of:\n");
			printMessage("  image: %s\n", img_desc.name);
			printMessage("  flashBase @addr: 0x%X\n", img_desc.flashBase);
			printMessage("  memBase @addr:0x%X\n", img_desc.memBase);
			printMessage("  size: %i Bytes (0x%X)\n", img_desc.size, img_desc.size); //size in flash (blocks)
			printMessage("  entryPoint: 0x%X\n", img_desc.entryPoint);
			printMessage("  length: %i Bytes\n", img_desc.length); //real number of bytes of image
			printMessage("  checksum: %i\n", img_desc.chkSum);
			printMessage("  uiBoardSerialNumber: %i\n", img_desc.uiBoardSerialNumber);
			printMessage("  uiVersionHigh: %i\n", img_desc.uiVersionHigh);
			printMessage("  uiVersionLow: %i\n", img_desc.uiVersionLow);
			printMessage("  uiConfigurationFlags: %i\n", img_desc.uiConfigurationFlags);
			printMessage("  Build Time: %s\n", img_desc.BuildTime);
			printMessage("  Build Date: %s\n", img_desc.BuildDate);
		} else if (tmp_quadlet == DICE_FL_E_GEN_NOMATCH) {
			//printMessage("in showImgInfoFL(): Image not exists.\nSTOP.\n");
			return false;
		} else {
			//printMessage("in showImgInfoFL(): unknown error =  0x%X\nSTOP.\n", tmp_quadlet);
			return false;
		}
		imageID++;
	} while (tmp_quadlet == DICE_FL_RETURN_NO_ERROR);
	return true;
}

bool
Device::showAppInfoFL() {

	DICE_FL_GET_APP_INFO_RETURN app_info;

	writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_GET_APP_INFO);

	do {
		usleep(10000);
		readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
	} while (tmp_quadlet & (1UL<<31));

	readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

	if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
		readRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &app_info, sizeof(app_info));
		printMessage("Application information of 'dice' image:\n");
		printMessage("  uiBaseSDKVersion: %X\n", app_info.uiBaseSDKVersion); //value needs to be parsed, but how?
		printMessage("  uiApplicationVersion: %X\n", app_info.uiApplicationVersion); //value needs to be parsed, but how?
		printMessage("  uiVendorID: %X\n", app_info.uiVendorID);
		printMessage("  uiProductID: %X\n", app_info.uiProductID);
		printMessage("  BuildTime: %s\n", app_info.BuildTime);
		printMessage("  BuildDate: %s\n", app_info.BuildDate);
		printMessage("  uiBoardSerialNumber: %d\n", app_info.uiBoardSerialNumber);
		return true;
	} else {
		printMessage("in showAppInfoFL(): unknown error =  0x%X\nSTOP.\n", tmp_quadlet);
		printMessage("Cannot read application information\n");
		return false;
	}
}

bool
Device::testDiceFL(int action) {

	DICE_FL_TEST_ACTION_PARAM testParam;
	DICE_FL_TEST_ACTION_RETURN testReturn;
	char pvalue0[11];
	char pvalue1[11];
	char* pEnd;

	switch (action) {
		case 1 :
			testParam.cmdID = action;

			printMessage("Use for input (quadlet = 32 bit) hex values only, i.e. '0x8080'\n");
			printMessage("Writeable address range in RAM: 0x000000 - 0x7FFFFF\n");
			printMessage("The address must be 32 bit aligned\n");

			printMessage("Enter the @addr to write: ");
			cin >> pvalue0;
			testParam.lvalue0 = strtoul(pvalue0, &pEnd, 16);
			if (testParam.lvalue0 > 0x7FFFFF) {
				printMessage("@addr out of range. Aborting.\nSTOP.\n");
				return false;
			}

			printMessage("Enter the value to write: ");
			cin >> pvalue1;
			testParam.lvalue1 = strtoul(pvalue1, &pEnd, 16);

			writeRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &testParam, sizeof(testParam));

			writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_TEST_ACTION);

			do {
				usleep(10000);
				readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
			} while (tmp_quadlet & (1UL<<31));

			readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

			if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
				printMessage("Quadlet written successfully\n");
				return true;
			} else {
				printMessage("in testDiceFL(): unknown error =  0x%X\nSTOP.\n", tmp_quadlet);
				return false;
			}
			break;
		case 2 :
			testParam.cmdID = action;

			printMessage("Use for input hex values only, i.e. '0x8080'\n");
			printMessage("The address must be 32 bit aligned\n");

			printMessage("Enter the @addr to read: ");
			cin >> pvalue0;
			testParam.lvalue0 = strtoul(pvalue0, &pEnd, 16);

			writeRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &testParam, sizeof(testParam));

			writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_TEST_ACTION);

			do {
				usleep(10000);
				readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
			} while (tmp_quadlet & (1UL<<31));

			readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

			if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
				readRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &testReturn, sizeof(testReturn));
				printMessage("Value @addr 0x%X = 0x%X\n", testParam.lvalue0, testReturn.data[0]);
				printMessage("Quadlet read successfully\n");
				return true;
			} else {
				printMessage("in testDiceFL(): unknown error =  0x%X\nSTOP.\n", tmp_quadlet);
				return false;
			}
			break;
		default :
			printMessage("Test&Debug command not found.\n");
			return false;
			break;
	}
}

bool
Device::deleteImgFL(const char* image, bool v) {

	DICE_FL_DELETE_IMAGE_PARAM imageDelete;

	memcpy(imageDelete.name, image, strlen(image)+1);

	printMessage("Deleting image '%s'\n", image);
	printMessage("Please wait, this will take some time\n");
	printMessage("Deletion in progress ...\n");

	writeRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &imageDelete, sizeof(imageDelete));

	writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_DELETE_IMAGE);

	do {
		usleep(300000);
		readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
	} while (tmp_quadlet & (1UL<<31));

	readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

	if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
		printMessage("Deletion successfully finished\n");
		return true;
	} else if (tmp_quadlet == DICE_FL_E_FIS_ILLEGAL_IMAGE) {
		if (v) {
			printMessage("in deleteImgFL(): FIS illegal image\nSTOP.\n");
			return false;
		} else {
			printMessage("No image with name '%s' in firmware. Nothing to delete.\n", image);
			return true;
		}
	} else {
		printMessage("in deleteImgFL(): unknown error =  0x%X\nSTOP.\n", tmp_quadlet);
		return false;
	}
}

bool
Device::flashDiceFL(const char* filename, const char* image) {

	/***************************
	 * CHECKING CAPABILITY BITS
	 ***************************/

	readReg(DICE_FL_OFFSET + DICE_FL_CAPABILITIES, &tmp_quadlet);

	printMessage("CAPABILITIES = 0x%X\n", tmp_quadlet);
	/*
	 * this operation is necessary to determine if IMAGE-AUTOERASE and PROGRESS-INFORMATION are supported
	 * to be implemented in a future release
	 */

	/***************
	 * UPLOAD IMAGE
	 ***************/

	DICE_FL_UPLOAD_HEADER_PARAM upload_header;
	DICE_FL_UPLOAD_DATA_PARAM upload_data;
	uint32_t imageSize = 0;
	uint32_t index = 0;
	uint32_t chksum = 0;
	unsigned char* byteValue;

	ifstream file(filename, ios::in|ios::binary|ios::ate);

	if (file.is_open()) {
		imageSize = file.tellg();
		file.seekg(0, ios::beg);

		printMessage("Uploading DICE image (image size = %i Bytes)\n", imageSize);
		printMessage("Please wait, this will take some time\n");
		printMessage("Upload in progress ...\n");
		while (file.good()) {
			file.read((char*) upload_data.buffer, sizeof(upload_data));

			if ((upload_header.length = (uint32_t) file.gcount()) > 0) {

				upload_header.index = index;

				byteValue = (unsigned char*) upload_data.buffer;
				for (uint32_t i=0; i<upload_header.length; i++) {
					chksum += *byteValue++;
				}

				writeRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &upload_header, sizeof(upload_header));
				writeRegBlock(DICE_FL_OFFSET + DICE_FL_BUFFER, (fb_quadlet_t*) &upload_data, sizeof(upload_data));

				writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_UPLOAD);

				do {
					usleep(100);
					readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
				} while (tmp_quadlet & (1UL<<31));

				readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

				if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
					//printMessage("Upload operation successful");
				} else if (tmp_quadlet == DICE_FL_E_BAD_INPUT_PARAM) {
					printMessage("in flashDiceFL(): bad input parameter\nSTOP.\n");
					return false;
				} else {
					printMessage("in flashDiceFL(): unknown error =  0x%X\nSTOP.\n", tmp_quadlet);
					return false;
				}
				index += upload_header.length;
			}
		}

		file.close();
	} else {
		printMessage("Cannot open file %s\nSTOP.\n", filename);
		return false;
	}

	/**************
	 * UPLOAD_STAT
	 **************/

	writeReg(DICE_FL_OFFSET + DICE_FL_PARAMETER, imageSize);

	writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_UPLOAD_STAT);

	do {
		usleep(1000000); //take care on polling, depends on dice-image size (to low values lead to a read error)
		readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
	} while (tmp_quadlet & (1UL<<31));

	readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

	if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
		readReg(DICE_FL_OFFSET + DICE_FL_PARAMETER, &tmp_quadlet);
		//printMessage("Offline (DICE) checksum calculation = %i\n", tmp_quadlet);
		//printMessage("Offline (local) checksum calculation = %i\n", chksum);
	} else {
		printMessage("in flashDiceFL(): unknown error =  0x%X\nSTOP.\n", tmp_quadlet);
		return false;
	}

	/***********
	 * FLASHING
	 ***********/

	if (tmp_quadlet == chksum) {

		printMessage(
				"\n***********************************************************************\n\n"
				"Flashing process was successfully tested on a TCAT DICE EVM002 board.\n"
				"It may work with other DICE based boards, but it can cause your device\n"
				"to magically stop working (a.k.a. bricking), too.\n\n"
				"If you are on a BIG ENDIAN machine (i.e. Apple Mac) this process will\n"
				"definitely brick your device. You have been warned.\n\n"
				"By pressing 'y' you accept the risk, otherwise process will be aborted.\n\n"
				"        *****  DON'T TURN OFF POWER DURING FLASH PROCESS *****\n\n"
				"***********************************************************************\n\n"
				"Continue anyway? ");
		char query;
		cin >> query;
		if (query == 'y') {

			DICE_FL_CREATE_IMAGE_PARAM imageCreate;

			imageCreate.length = imageSize;
			imageCreate.execAddr = 0x30000;
			imageCreate.entryAddr = 0x30040;
			memcpy(imageCreate.name, image, strlen(image)+1);

			deleteImgFL(image);

			printMessage("Writing image '%s' to device\n", image);
			printMessage("Please wait, this will take some time\n");
			printMessage("Flash in progress ...\n");

			writeRegBlock(DICE_FL_OFFSET + DICE_FL_PARAMETER, (fb_quadlet_t*) &imageCreate, sizeof(imageCreate));

			writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_CREATE_IMAGE);

			do {
				usleep(300000);
				readReg(DICE_FL_OFFSET + DICE_FL_OPCODE, &tmp_quadlet);
			} while (tmp_quadlet & (1UL<<31));

			readReg(DICE_FL_OFFSET + DICE_FL_RETURN_STATUS, &tmp_quadlet);

			if (tmp_quadlet == DICE_FL_RETURN_NO_ERROR) {
				printMessage("Flashing successfully finished\n");
				printMessage("You have to restart the device manually to load the new image\n");
				return true;
			} else if (tmp_quadlet == DICE_FL_E_FIS_ILLEGAL_IMAGE) {
				printMessage("in flashDiceFL(): FIS illegal image\nSTOP.\n");
				return false;
			} else {
				printMessage("in flashDiceFL(): unknown error =  0x%X\nSTOP.\n", tmp_quadlet);
				return false;
			}

		/**************
		 * RESET_IMAGE
		 **************/

		//writeReg(DICE_FL_OFFSET + DICE_FL_OPCODE, (1UL<<31) | DICE_FL_OP_RESET_IMAGE);
		/*
		 * this would lead to a segmentation fault due to a BUS reset by DICE device
		 */

		} else {
			return false;
		}
	} else {
		printMessage("Checksum mismatch. Flash operation aborted.\n");
		return false;
	}
}

} //namespace Dice
