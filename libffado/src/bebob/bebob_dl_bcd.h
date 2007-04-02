/* bebob_dl_bcd.h
 * Copyright (C) 2006 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */
#ifndef BEBOB_DL_BCD_H
#define BEBOB_DL_BCD_H

#include "fbtypes.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <cstdio>

namespace BeBoB {

    class BCD {
    public:
        BCD( std::string filename );
        ~BCD();

        bool parse();

        fb_octlet_t getSoftwareDate() const
            { return m_softwareDate; }
        fb_octlet_t getSoftwareTime() const
            { return m_softwareTime; }
        fb_quadlet_t getSoftwareId() const
            { return m_softwareId; }
        fb_quadlet_t getSoftwareVersion() const
            { return m_softwareVersion; }
	fb_quadlet_t getHardwareId() const
	    { return m_hardwareId; }
	fb_quadlet_t getVendorOUI() const
	    { return m_vendorOUI; }
	
        fb_quadlet_t getImageBaseAddress() const
            { return m_imageBaseAddress; }
        fb_quadlet_t getImageOffset() const
            { return m_imageOffset; }
        fb_quadlet_t getImageLength() const
            { return m_imageLength; }
        fb_quadlet_t getImageCRC() const
            { return m_imageCRC; }

	fb_quadlet_t getCnEOffset() const
	    { return m_cneOffset; }
        fb_quadlet_t getCnELength() const
            { return m_cneLength; }
	fb_quadlet_t getCnECRC() const
	    { return m_cneCRC; }

        bool read( int addr, fb_quadlet_t* q );
        bool read( int addr, fb_octlet_t* o );
	bool read( int addr, unsigned char* b, size_t len );

	void displayInfo();

    protected:
        unsigned long crc32_table[256];
	void initCRC32Table();
	unsigned long reflect(unsigned long ref, char ch);
	unsigned int getCRC(unsigned char* text, size_t len);
        bool checkHeaderCRC( unsigned int crcOffset,
                             unsigned int headerSize );
        bool readHeaderInfo();
    protected:
        std::FILE* m_file;
        std::string m_filename;
        fb_quadlet_t m_bcd_version;

        fb_octlet_t  m_softwareDate;
        fb_octlet_t  m_softwareTime;
        fb_quadlet_t m_softwareId;
        fb_quadlet_t m_softwareVersion;
	fb_quadlet_t m_hardwareId;
	fb_quadlet_t m_vendorOUI;
	    

        fb_quadlet_t m_imageBaseAddress;
        fb_quadlet_t m_imageLength;
        fb_quadlet_t m_imageOffset;
        fb_quadlet_t m_imageCRC;

        fb_quadlet_t m_cneLength;
	fb_quadlet_t m_cneOffset;
	fb_quadlet_t m_cneCRC;



	DECLARE_DEBUG_MODULE;
    };

    std::string makeString( fb_octlet_t v );
    std::string makeDate( fb_octlet_t v );
    std::string makeTime( fb_octlet_t v );
};

#endif
