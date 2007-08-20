#include "bebob_configparser.h"

#include <fstream>
#include <istream>
#include <iostream>

BeBoB::ConfigParser::ConfigParser( const char* filename )
{
    using namespace std;

    cout << "XXX BeBoB::ConfigParser::ConfigParser" << endl;

    ifstream in ( filename );

    if ( !in ) {
        perror( filename );
        return;
    }

    string line;
    while ( !getline( in,  line ).eof() ) {


    }
}

BeBoB::ConfigParser::~ConfigParser()
{
    for ( VendorModelEntryVector::iterator it = m_vendorModelEntries.begin();
          it != m_vendorModelEntries.end();
          ++it )
    {
        delete *it;
    }
}

const BeBoB::VendorModelEntryVector&
BeBoB::ConfigParser::getVendorModelEntries() const
{
    return m_vendorModelEntries;
}

