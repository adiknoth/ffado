/* functionblock.h
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

#ifndef FUNCTION_BLOCK_H
#define FUNCTION_BLOCK_H

#include "debugmodule/debugmodule.h"

class AvDeviceSubunit;

class FunctionBlock {
public:
    FunctionBlock( AvDeviceSubunit& subunit,
                   bool verbose );
    FunctionBlock( const FunctionBlock& rhs );
    virtual ~FunctionBlock();

    virtual bool discover();
    virtual bool discoverConnections();

    virtual const char* getName() = 0;

protected:
    AvDeviceSubunit* m_subunit;
    bool m_verbose;

    DECLARE_DEBUG_MODULE;
};

typedef std::vector<FunctionBlock*> FunctionBlockVector;

class FunctionBlockMixer: public FunctionBlock
{
public:
    FunctionBlockMixer( AvDeviceSubunit& subunit,
                        bool verbose );
    FunctionBlockMixer( const FunctionBlockMixer& rhs );
    virtual ~FunctionBlockMixer();

    virtual const char* getName();
};

#endif
