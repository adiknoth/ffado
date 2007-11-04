/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
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
