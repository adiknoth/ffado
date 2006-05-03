/* bebob_avdevice_xml.cpp
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

// AvDevice XML stuff

#include "bebob/bebob_avdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"

namespace BeBoB {

bool
AvDevice::addXmlDescription( xmlNodePtr deviceNode )
{
    // connection set
    //  direction
    //  connection
    //    id
    //    port
    //    node
    //    plug
    //    dimension
    //    samplerate
    //    streams
    //      stream


    ///////////
    // get plugs

    AvPlug* inputPlug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Input, 0 );
    if ( !inputPlug ) {
        debugError( "addXmlDescription: No iso input plug found with id 0\n" );
        return false;
    }
    AvPlug* outputPlug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Output, 0 );
    if ( !outputPlug ) {
        debugError( "addXmlDescription: No iso output plug found with id 0\n" );
        return false;
    }

    ///////////
    // add connection set output

    xmlNodePtr connectionSet = xmlNewChild( deviceNode, 0,
                                            BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "addXmlDescription:: Could not create 'ConnnectionSet' node for "
                    "direction 1 (playback)\n" );
        return false;
    }

    if ( !inputPlug->addXmlDescription( connectionSet ) ) {
        debugError( "addXmlDescription: Could not add iso input plug 0 to XML description\n" );
        return false;
    }

    // add connection set input

    connectionSet = xmlNewChild( deviceNode, 0,
                                 BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "addXmlDescription: Couldn't create 'ConnectionSet' node for "
                    "direction 0 (recorder)\n" );
        return false;
    }

    if ( !outputPlug->addXmlDescription( connectionSet ) ) {
        debugError( "addXmlDescription: Could not add iso output plug 0 to XML description\n" );
        return false;
    }

    ////////////
    // add stream format

    xmlNodePtr streamFormatNode = xmlNewChild( deviceNode, 0,
                                               BAD_CAST "StreamFormats", 0 );
    if ( !streamFormatNode ) {
        debugError( "addXmlDescription: Could not create 'StreamFormats' node\n" );
        return false;
    }

    if ( !inputPlug->addXmlDescriptionStreamFormats( streamFormatNode ) ) {
        debugError( "addXmlDescription:: Could not add stream format info\n" );
        return false;
    }

    streamFormatNode= xmlNewChild( deviceNode, 0,
                                 BAD_CAST "StreamFormats", 0 );
    if ( !streamFormatNode ) {
        debugError( "addXmlDescription: Could not create 'StreamFormat' node\n" );
        return false;
    }

    if ( !outputPlug->addXmlDescriptionStreamFormats( streamFormatNode ) ) {
        debugError( "addXmlDescription:: Could not add stream format info\n" );
        return false;
    }

    return true;
}

}
