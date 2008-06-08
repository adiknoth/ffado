/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2005-2008 by Pieter Palmers
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
 // FOR CACHE_VERSION

#include "serialize.h"

using namespace std;


IMPL_DEBUG_MODULE( Util::XMLSerialize,   XMLSerialize,   DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( Util::XMLDeserialize, XMLDeserialize, DEBUG_LEVEL_NORMAL );

Util::XMLSerialize::XMLSerialize( std::string fileName )
    : IOSerialize()
    , m_filepath( fileName )
    , m_verboseLevel( DEBUG_LEVEL_NORMAL )
{
    setDebugLevel( DEBUG_LEVEL_NORMAL );
    try {
        m_doc.create_root_node( "ffado_cache" );
        writeVersion();
    } catch ( const exception& ex ) {
        cout << "Exception caught: " << ex.what();
    }
}

Util::XMLSerialize::XMLSerialize( std::string fileName, int verboseLevel )
    : IOSerialize()
    , m_filepath( fileName )
    , m_verboseLevel( verboseLevel )
{
    setDebugLevel(verboseLevel);
    try {
        m_doc.create_root_node( "ffado_cache" );
        writeVersion();
    } catch ( const exception& ex ) {
        cout << "Exception caught: " << ex.what();
    }
}

Util::XMLSerialize::~XMLSerialize()
{
    try {
        m_doc.write_to_file_formatted( m_filepath );
    } catch ( const exception& ex ) {
        cout << "Exception caugth: " << ex.what();
    }

}

void
Util::XMLSerialize::writeVersion()
{
    Xml::Node* pNode = m_doc.get_root_node();
    if(pNode) {
        char* valstr;
        asprintf( &valstr, "%s", CACHE_VERSION );

        // check whether we already have a version node
        Xml::Nodes versionfields = (*pNode)["CacheVersion"];

        if(versionfields.size()) { // set the value
            versionfields.at(0)->set_child_text( valstr );
        } else {
            // make a new field
            Xml::Node &n = pNode->add( Xml::Node("CacheVersion", NULL) );
            n.set_child_text( valstr );
        }
        free( valstr );
    } else {
        debugError("No root node\n");
    }
}

bool
Util::XMLSerialize::write( std::string strMemberName,
                           long long value )

{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "write %s = %d\n",
                 strMemberName.c_str(), value );

    std::vector<std::string> tokens;
    tokenize( strMemberName, tokens, "/" );

    if ( tokens.size() == 0 ) {
        debugWarning( "token size is 0\n" );
        return false;
    }

    Xml::Node* pNode = m_doc.get_root_node();
    if(pNode) {
        pNode = getNodePath( pNode, tokens );
        if(pNode) {
            // element to be added
            Xml::Node& n = pNode->add( Xml::Node(tokens[tokens.size() - 1].c_str(), NULL) );
            char* valstr;
            asprintf( &valstr, "%lld", value );
            n.set_child_text( valstr );
            free( valstr );
        } else {
            return false;
        }
    } else {
        return false;
    }
    return true;
}

bool
Util::XMLSerialize::write( std::string strMemberName,
                           std::string str)
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "write %s = %s\n",
                 strMemberName.c_str(), str.c_str() );

    std::vector<std::string> tokens;
    tokenize( strMemberName, tokens, "/" );

    if ( tokens.size() == 0 ) {
        debugWarning( "token size is 0\n" );
        return false;
    }

    Xml::Node* pNode = m_doc.get_root_node();
    if(pNode) {
        pNode = getNodePath( pNode, tokens );
        if(pNode) {
            // element to be added
            Xml::Node& n = pNode->add( Xml::Node(tokens[tokens.size() - 1].c_str(), NULL) );
            n.set_child_text( str );
        } else {
            return false;
        }
    } else {
        return false;
    }
    return true;

}

Util::Xml::Node*
Util::XMLSerialize::getNodePath( Util::Xml::Node* pRootNode,
                                 std::vector<string>& tokens )
{
    // returns the correct node on which the new element has to be added.
    // if the path does not exist, it will be created.

    if ( tokens.size() == 1 ) {
        return pRootNode;
    }

    unsigned int iTokenIdx = 0;
    Xml::Node* pCurNode = pRootNode;
    for (bool bFound = false;
         ( iTokenIdx < tokens.size() - 1 );
         bFound = false, iTokenIdx++ )
    {
        Xml::Node::Children& nodeList = pCurNode->children;
        for ( Xml::Node::Children::iterator it = nodeList.begin();
              it != nodeList.end();
              ++it )
        {
            Xml::Node* thisNode = &( *it );
            if ( thisNode->name.compare(tokens[iTokenIdx]) == 0 ) {
                pCurNode = thisNode;
                bFound = true;
                break;
            }
        }
        if ( !bFound ) {
            break;
        }
    }

    for ( unsigned int i = iTokenIdx; i < tokens.size() - 1; i++, iTokenIdx++ ) {
        Xml::Node& n = pCurNode->add( Xml::Node(tokens[iTokenIdx].c_str(), NULL) );
        pCurNode = &n;
    }
    return pCurNode;
}

/***********************************/

Util::XMLDeserialize::XMLDeserialize( std::string fileName )
    : IODeserialize()
    , m_filepath( fileName )
    , m_verboseLevel( DEBUG_LEVEL_NORMAL )
{
    setDebugLevel(DEBUG_LEVEL_NORMAL);
    try {
//         m_parser.set_substitute_entities(); //We just want the text to
                                            //be resolved/unescaped
                                            //automatically.
        m_doc.load_from_file(m_filepath);
    } catch ( const exception& ex ) {
        cout << "Exception caught: " << ex.what();
    }
}

Util::XMLDeserialize::XMLDeserialize( std::string fileName, int verboseLevel )
    : IODeserialize()
    , m_filepath( fileName )
    , m_verboseLevel( verboseLevel )
{
    setDebugLevel(verboseLevel);
    try {
//         m_parser.set_substitute_entities(); //We just want the text to
                                            //be resolved/unescaped
                                            //automatically.
        m_doc.load_from_file(m_filepath);
    } catch ( const exception& ex ) {
        cout << "Exception caught: " << ex.what();
    }
}

Util::XMLDeserialize::~XMLDeserialize()
{
}

bool
Util::XMLDeserialize::isValid()
{
    return checkVersion();
}

bool
Util::XMLDeserialize::checkVersion()
{
    std::string savedVersion;
    if (read( "CacheVersion", savedVersion )) {
        std::string expectedVersion = CACHE_VERSION;
        debugOutput( DEBUG_LEVEL_NORMAL, "Cache version: %s, expected: %s.\n", savedVersion.c_str(), expectedVersion.c_str() );
        if (expectedVersion == savedVersion) {
            debugOutput( DEBUG_LEVEL_VERBOSE, "Cache version OK.\n" );
            return true;
        } else {
            debugOutput( DEBUG_LEVEL_VERBOSE, "Cache version not OK.\n" );
            return false;
        }
    } else return false;
}

bool
Util::XMLDeserialize::read( std::string strMemberName,
                            long long& value )

{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "lookup %s\n", strMemberName.c_str() );

    Xml::Node* pNode = m_doc.get_root_node();

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "pNode = %s\n", pNode->name.c_str() );
    if(pNode) {
        Xml::Nodes nodeSet = pNode->find( strMemberName );
        for ( Xml::Nodes::iterator it = nodeSet.begin();
            it != nodeSet.end();
            ++it )
        {
            Xml::Node *n = *it;
            if(n && n->has_child_text() ) {
                char* tail;
                value = strtoll( n->get_child_text().c_str(),
                                &tail, 0 );
                debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "found %s = %d\n",
                            strMemberName.c_str(), value );
                return true;
            }
            debugWarning( "no such a node %s\n", strMemberName.c_str() );
            return false;
        }
    
        debugWarning( "no such a node %s\n", strMemberName.c_str() );
        return false;
    } else {
        debugError("No root node\n");
        return false;
    }
}

bool
Util::XMLDeserialize::read( std::string strMemberName,
                            std::string& str )
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "lookup %s\n", strMemberName.c_str() );

    Xml::Node* pNode = m_doc.get_root_node();

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "pNode = %s\n", pNode->name.c_str() );
    if(pNode) {
        Xml::Nodes nodeSet = pNode->find( strMemberName );
        for ( Xml::Nodes::iterator it = nodeSet.begin();
            it != nodeSet.end();
            ++it )
        {
            Xml::Node *n = *it;
            if(n) {
                if(n->has_child_text() ) {
                    str = n->get_child_text();
                } else {
                    str = "";
                }
                debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "found %s = %s\n",
                            strMemberName.c_str(), str.c_str() );
                return true;
            }
            debugWarning( "no such a node %s\n", strMemberName.c_str() );
            return false;
        }
    
        debugWarning( "no such a node %s\n", strMemberName.c_str() );
        return false;
    } else {
        debugError("No root node\n");
        return false;
    }
}

bool
Util::XMLDeserialize::isExisting( std::string strMemberName )
{
    Xml::Node* pNode = m_doc.get_root_node();
    if(pNode) {
        Xml::Nodes nodeSet = pNode->find( strMemberName );
        return nodeSet.size() > 0;
    } else return false;
}

void
tokenize(const string& str,
         vector<string>& tokens,
         const string& delimiters)
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
