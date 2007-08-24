/*
 * Copyright (C) 2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef GENERICAVC_VENDORMODEL_H
#define GENERICAVC_VENDORMODEL_H

#include <vector>

namespace GenericAVC {

// struct to define the supported devices
struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    char *vendor_name;
    char *model_name;
};

typedef std::vector<VendorModelEntry*> VendorModelEntryVector;

class VendorModel {
public:
    VendorModel( const char* filename );
    ~VendorModel();

    const VendorModelEntryVector& getVendorModelEntries() const;
private:
    VendorModelEntryVector m_vendorModelEntries;
};

}

#endif
