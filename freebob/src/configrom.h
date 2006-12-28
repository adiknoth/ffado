/* configrom.h
 * Copyright (C) 2006 by Daniel Wagner
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

#include <string>

#include <libraw1394/raw1394.h>
#include "csr1212.h"


class ConfigRom {
public:
    ConfigRom( raw1394handle_t raw1394Handle, int iNodeId );
    virtual ~ConfigRom();

    bool initialize();

    const bool isAvcDevice() const;
    const octlet_t getGuid() const;
    const std::string getModelName() const;
    const std::string getVendorName() const;

protected:

    void processUnitDirectory(struct csr1212_csr* csr,
                              struct csr1212_keyval* ud_kv,
                              unsigned int *id);

    void processRootDirectory(struct csr1212_csr* csr);

    raw1394handle_t m_raw1394Handle;
    int m_iNodeId;
    bool m_bAvcDevice;
    octlet_t m_guid;
    std::string m_vendorName;
    std::string m_modelName;

    /* only used during parsing */
    struct csr1212_keyval* m_vendorNameKv;
    struct csr1212_keyval* m_modelNameKv;
    struct csr1212_csr* m_csr;
};



#endif /* CONFIGROM_H */
