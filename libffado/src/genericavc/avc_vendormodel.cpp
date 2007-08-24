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
#include <iterator>

using namespace std;

static void
tokenize(const string& str,
         vector<string>& tokens,
         const string& delimiters = " ")
{
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}

GenericAVC::VendorModel::VendorModel( const char* filename )
{
    ifstream in ( filename );

    if ( !in ) {
        perror( filename );
        return;
    }

    cout << "vendorId\t\tmodelId\t\tvendorName\t\tmodelName" << endl;
    string line;
    while ( !getline( in,  line ).eof() ) {
        string::size_type i = line.find_first_not_of( " \t\n\v" );
        if ( i != string::npos && line[i] == '#' )
            continue;

        vector<string> tokens;
        tokenize( line, tokens, "," );

        for ( vector<string>::iterator it = tokens.begin();
              it != tokens.end();
              ++it )
        {
            string vendorId = *it++;
            string modelId = *it++;
            string vendorName = *it++;
            string modelName= *it;
            cout << vendorId << "\t" << modelId << "\t" <<vendorName << "\t" << modelName << endl;
        }
    }

    if ( !in.eof() ) {
        cout << "GenericAVC::VendorModel::VendorModel: error in parsing" << endl;
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
