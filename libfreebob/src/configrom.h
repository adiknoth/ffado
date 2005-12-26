/* configrom.h
 * Copyright (C) 2005 by Daniel Wagner
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

#ifndef CONFIGROM_H
#define CONFIGROM_H

#include "fbtypes.h"
#include "csr1212.h"

#include "debugmodule/debugmodule.h"

#include <string>

class Ieee1394Service;

class ConfigRom {
 public:
    ConfigRom( Ieee1394Service* ieee1394service, fb_nodeid_t nodeId );
    virtual ~ConfigRom();

    bool initialize();

    const bool isAvcDevice() const;
    const fb_octlet_t getGuid() const;
    const std::string getModelName() const;
    const std::string getVendorName() const;

 protected:
    void processUnitDirectory( struct csr1212_csr*    csr,
                               struct csr1212_keyval* ud_kv,
                               unsigned int*          id );

    void processRootDirectory( struct csr1212_csr* csr );

    Ieee1394Service* m_1394Service;
    fb_nodeid_t      m_nodeId;
    bool             m_avcDevice;
    fb_octlet_t      m_guid;
    std::string      m_vendorName;
    std::string      m_modelName;

    /* only used during parsing */
    struct csr1212_keyval* m_vendorNameKv;
    struct csr1212_keyval* m_modelNameKv;
    struct csr1212_csr*    m_csr;

private:
    ConfigRom( const ConfigRom& );

    DECLARE_DEBUG_MODULE;
};

#endif /* CONFIGROM_H */
