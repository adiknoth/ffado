/* motu_avdevice.h
 * Copyright (C) 2006 by Pieter Palmers
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef MOTUDEVICE_H
#define MOTUDEVICE_H

#include "iavdevice.h"

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebob/xmlparser.h"

#define MOTU_BASE_ADDR     0xfffff0000000ULL
#define MOTU_BASE_RATE_44100 		(0<<4)
#define MOTU_BASE_RATE_48000 		(1<<4)
#define MOTU_RATE_MULTIPLIER_1X 	(0<<5)
#define MOTU_RATE_MULTIPLIER_2X 	(1<<5)
#define MOTU_RATE_MULTIPLIER_4X 	(2<<5)
#define MOTU_RATE_MASK				(0x00000007)


class ConfigRom;
class Ieee1394Service;

namespace Motu {

class MotuDevice : public IAvDevice {
public:
    MotuDevice( Ieee1394Service& ieee1394Service,
		  int nodeId,
		  int verboseLevel );
    virtual ~MotuDevice();

    virtual bool discover();
    virtual ConfigRom& getConfigRom() const;
    virtual bool addXmlDescription( xmlNodePtr deviceNode );
    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual void showDevice() const;

protected:
    Ieee1394Service* m_1394Service;
    ConfigRom*       m_configRom;
    int              m_nodeId;
    int              m_verboseLevel;

private:
	unsigned int ReadRegister(unsigned int reg);
	signed int WriteRegister(unsigned int reg, quadlet_t data);

	/* ======================================================================= */
	/* MOTU-related defines, definitions, enumerations etc */

	/* IEEE1394 Vendor IDs.  One would expect only MOTU, but you never know if a
	* clone might appear some day.
	*/
	enum EVendorId {
		VENDOR_MOTU = 0x000001f2,
		VENDOR_MOTU_TEST     = 0x0000030D,
	};
	
	/* IEEE1394 Model IDs for different MOTU hardware */
	enum EModelId {
		MOTU_828mkII  = 0x00101800,
		MOTU_TRAVELER = 0x00104800,
		MOTU_TEST     = 0x00000000,
	};

	/* Input/Output channels.  Channels are included here even if they are
	* uni-directional (eg: phones - which is output only).  It is up to
	* individual functions taking a motufw_io_channel_t argument to ensure
	* that operations are appropriate for the channel supplied.
	*/
	enum EIoChannel {
		MOTUFW_IO_ANALOG1, MOTUFW_IO_ANALOG2, MOTUFW_IO_ANALOG3,
		MOTUFW_IO_ANALOG4, MOTUFW_IO_ANALOG5, MOTUFW_IO_ANALOG6,
		MOTUFW_IO_ANALOG7, MOTUFW_IO_ANALOG8,
		MOTUFW_IO_AESEBU_L, MOTUFW_IO_AESEBU_R, /* Traveler only */
		MOTUFW_IO_MAIN_L,   MOTUFW_IO_MAIN_R,   /* 828MkII only */
		MOTUFW_IO_SPDIF_L,  MOTUFW_IO_SPDIF_R,
		MOTUFW_IO_ADAT1, MOTUFW_IO_ADAT2, MOTUFW_IO_ADAT3,
		MOTUFW_IO_ADAT4, MOTUFW_IO_ADAT5, MOTUFW_IO_ADAT6,
		MOTUFW_IO_ADAT7, MOTUFW_IO_ADAT8,
	};
	
	/* Stereo output buses */
	enum EOutputBus {
		MOTUFW_OUTPUT_DISABLED ,
		MOTUFW_OUTPUT_PHONES,
		MOTUFW_OUTPUT_ANALOG1_2, MOTUFW_OUTPUT_ANALOG3_4,
		MOTUFW_OUTPUT_ANALOG5_6, MOTUFW_OUTPUT_ANALOG7_8,
		MOTUFW_OUTPUT_AESEBU, MOTUFW_OUTPUT_MAINPOUT,
		MOTUFW_OUTPUT_SPDIF,
		MOTUFW_OUTPUT_ADAT1_2, MOTUFW_OUTPUT_ADAT3_4,
		MOTUFW_OUTPUT_ADAT5_6, MOTUFW_OUTPUT_ADAT7_8,
	};
	
	/* Cuemix mixers available */
	enum ECuemixMixer {
		MOTUFW_CUEMIX_MIX1,
		MOTUFW_CUEMIX_MIX2,
		MOTUFW_CUEMIX_MIX3,
		MOTUFW_CUEMIX_MIX4,
	};
	
	/* MOTU configuration details */
	enum ESampleRate {
		MOTUFW_SAMPLERATE_44k1,  MOTUFW_SAMPLERATE_48k,
		MOTUFW_SAMPLERATE_88k2,  MOTUFW_SAMPLERATE_96k,
		MOTUFW_SAMPLERATE_176k4, MOTUFW_SAMPLERATE_192k,
	};
	
	enum EClockSource {
		MOTUFW_CLOCKSOURCE_INT,
		MOTUFW_CLOCKSOURCE_SMPTE,
		MOTUFW_CLOCKSOURCE_AESEBU,
		MOTUFW_CLOCKSOURCE_SPDIF,
		MOTUFW_CLOCKSOURCE_TOSLINK,
		MOTUFW_CLOCKSOURCE_WORDCLOCK,
		MOTUFW_CLOCKSOURCE_ADAT_9PIN,
		MOTUFW_CLOCKSOURCE_ADAT_OPTICAL,
	};
	
	enum ERefLevel{
		MOTUFW_REFLEVEL_PLUS4_DBU, MOTUFW_REFLEVEL_MINUS10_DBU,
	};
	
	enum EOpticalMode {
		MOTUFW_OPTICAL_MODE_OFF,
		MOTUFW_OPTICAL_MODE_ADAT,
		MOTUFW_OPTICAL_MODE_TOSLINK,
	};


	// debug support
    DECLARE_DEBUG_MODULE;
};

}

#endif
