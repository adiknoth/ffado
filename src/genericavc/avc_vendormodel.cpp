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

#include "genericavc/avc_vendormodel.h"

#include <fstream>
#include <istream>
#include <iostream>

GenericAVC::VendorModel::VendorModel( const char* filename )
{
    using namespace std;

    cout << "XXX GenericAVC::VendorModel::VendorModel" << endl;

    ifstream in ( filename );

    if ( !in ) {
        perror( filename );
        return;
    }

    string line;
    while ( !getline( in,  line ).eof() ) {


    }
}

GenericAVC::VendorModel::~VendorModel()
{
    for ( VendorModelEntryVector::iterator it = m_vendorModelEntries.begin();
          it != m_vendorModelEntries.end();
          ++it )
    {
        delete *it;
    }
}

const GenericAVC::VendorModelEntryVector&
GenericAVC::VendorModel::getVendorModelEntries() const
{
    return m_vendorModelEntries;
}
