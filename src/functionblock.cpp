/* functionblock.cpp
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

#include "functionblock.h"
#include "avdevicesubunit.h"

IMPL_DEBUG_MODULE( FunctionBlock, FunctionBlock, DEBUG_LEVEL_NORMAL );

FunctionBlock::FunctionBlock( AvDeviceSubunit& subunit,
                              bool verbose )
    : m_subunit( &subunit )
    , m_verbose( verbose )
{
    if ( m_verbose ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
}

FunctionBlock::FunctionBlock( const FunctionBlock& rhs )
    : m_subunit( rhs.m_subunit )
    , m_verbose( rhs.m_verbose )
{
}

FunctionBlock::~FunctionBlock()
{
}

bool
FunctionBlock::discover()
{
    //printf( "XXX function block discover\n" );
    return true;
}

bool
FunctionBlock::discoverConnections()
{
    //printf( "XXX fucntion block connection discover\n" );
    return true;
}

///////////////////////

FunctionBlockMixer::FunctionBlockMixer( AvDeviceSubunit& subunit,
                                        bool verbose )
    : FunctionBlock( subunit, verbose )
{
}

FunctionBlockMixer::FunctionBlockMixer( const FunctionBlockMixer& rhs )
    : FunctionBlock( rhs )
{
}

FunctionBlockMixer::~FunctionBlockMixer()
{
}

const char*
FunctionBlockMixer::getName()
{
    return "Mixer";
}
