/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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
 */

#include "genericavc/avc_vendormodel.h"
#include "libutil/serialize.h"

#include <fstream>
#include <istream>
#include <iostream>
#include <iterator>
#include <cerrno>
#include <functional>
#include <algorithm>

using namespace std;

GenericAVC::VendorModelEntry::VendorModelEntry()
    : vendor_id( 0 )
    , model_id( 0 )
{
}

GenericAVC::VendorModelEntry::VendorModelEntry( const VendorModelEntry& rhs )
    : vendor_id( rhs.vendor_id )
    , model_id( rhs.model_id )
    , vendor_name( rhs.vendor_name )
    , model_name( rhs.model_name )
{
}

GenericAVC::VendorModelEntry&
GenericAVC::VendorModelEntry::operator = ( const VendorModelEntry& rhs )
{
    // check for assignment to self
    if ( this == &rhs ) return *this;

    vendor_id   = rhs.vendor_id;
    model_id    = rhs.model_id;
    vendor_name = rhs.vendor_name;
    model_name  = rhs.model_name;

    return *this;
}

bool
GenericAVC::VendorModelEntry::operator == ( const VendorModelEntry& rhs ) const
{
    bool equal=true;

    equal &= (vendor_id   == rhs.vendor_id);
    equal &= (model_id    == rhs.model_id);
    equal &= (vendor_name == rhs.vendor_name);
    equal &= (model_name  == rhs.model_name);

    return equal;
}

GenericAVC::VendorModelEntry::~VendorModelEntry()
{
}

GenericAVC::VendorModel::VendorModel( const char* filename )
    : m_filename( filename )
{
}

GenericAVC::VendorModel::~VendorModel()
{
}


bool
GenericAVC::VendorModel::parse()
{
    ifstream in ( m_filename.c_str() );

    if ( !in ) {
        perror( m_filename.c_str() );
        return false;
    }

    string line;
    while ( !getline( in,  line ).eof() ) {
        string::size_type i = line.find_first_not_of( " \t\n\v" );
        if ( i != string::npos && line[i] == '#' )
            // this line starts with a '#' -> comment
            continue;

        vector<string> tokens;
        tokenize( line, tokens, "," );

        if ( tokens.size() < 4 )
            // ignore this line, it has not all needed mandatory entries
            continue;

        VendorModelEntry vme;
        vector<string>::const_iterator it = tokens.begin();
        char* tail;

        errno = 0;
        vme.vendor_id   = strtol( it++->c_str(), &tail, 0 );
        vme.model_id    = strtol( it++->c_str(), &tail, 0 );
        vme.vendor_name = *it++;
        vme.model_name  = *it++;

        if ( errno )
            // string -> int conversion failed
            continue;

        vector<string>::const_iterator end = tokens.end();
        if ( it != end )
            handleAdditionalEntries( vme, tokens, it, end );

        m_vendorModelEntries.push_back( vme );
    }

    if ( !in.eof() ) {
        cerr << "GenericAVC::VendorModel::VendorModel: error in parsing" << endl;
        return false;
    }

    return true;
}

bool
GenericAVC::VendorModel::printTable() const
{
    // Some debug output
    cout << "vendorId\t\tmodelId\t\tvendorName\t\t\t\tmodelName" << endl;
    for ( VendorModelEntryVector::const_iterator it = m_vendorModelEntries.begin();
          it != m_vendorModelEntries.end();
          ++it )
    {
        cout << it->vendor_id << "\t\t\t"
             << it->model_id << "\t"
             << it->vendor_name << "\t"
             << it->model_name << endl;
    }
    return true;
}

bool
GenericAVC::VendorModel::handleAdditionalEntries(VendorModelEntry& vme,
                                                 vector<string>& v,
                                                 vector<string>::const_iterator& b,
                                                 vector<string>::const_iterator& e )
{
    return true;
}

class is_same : public unary_function<GenericAVC::VendorModelEntry, bool> {
public:
    is_same( unsigned int vendor_id, unsigned model_id )
        : m_vendor_id( vendor_id )
        , m_model_id( model_id )
    {}

    bool operator () ( const GenericAVC::VendorModelEntry& vme ) const {
        return vme.vendor_id == m_vendor_id && vme.model_id == m_model_id;
    }

private:
    unsigned int m_vendor_id;
    unsigned int m_model_id;
};

GenericAVC::VendorModelEntry
GenericAVC::VendorModel::find( unsigned int vendor_id, unsigned model_id )
{
    VendorModelEntryVector::iterator it =
        find_if ( m_vendorModelEntries.begin(),
                  m_vendorModelEntries.end(),
                  is_same( vendor_id, model_id ) );
    if ( it != m_vendorModelEntries.end() )
        return *it;

    struct VendorModelEntry invalid;
    return invalid;
}

bool
GenericAVC::VendorModel::isPresent( unsigned int vendor_id, unsigned model_id )
{
    VendorModelEntryVector::iterator it =
        find_if ( m_vendorModelEntries.begin(),
                  m_vendorModelEntries.end(),
                  is_same( vendor_id, model_id ) );
    if ( it != m_vendorModelEntries.end() )
        return true;

    return false;
}

bool
GenericAVC::VendorModel::isValid( const GenericAVC::VendorModelEntry& vme )
{
    struct VendorModelEntry invalid;
    return !(vme==invalid);
}

const GenericAVC::VendorModelEntryVector&
GenericAVC::VendorModel::getVendorModelEntries() const
{
    return m_vendorModelEntries;
}
