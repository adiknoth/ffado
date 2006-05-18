/* $Id$ */

/*
 *   FreeBob streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */
#ifndef __FREEBOB_ISOHANDLERMANAGER__
#define __FREEBOB_ISOHANDLERMANAGER__

#include "../debugmodule/debugmodule.h"

#include <vector>

namespace FreebobStreaming
{

/*!
\brief The ISO Handler management class

 This class manages the use of ISO handlers by ISO streams

*/

class IsoHandler;
class IsoXmitHandler;
class IsoRecvHandler;

typedef std::vector<IsoXmitHandler *> IsoXmitHandlerVector;
typedef std::vector<IsoXmitHandler *>::iterator IsoXmitHandlerVectorIterator;
typedef std::vector<IsoRecvHandler *> IsoRecvHandlerVector;
typedef std::vector<IsoRecvHandler *>::iterator IsoRecvHandlerVectorIterator;


class IsoHandlerManager
{
	private:
		IsoXmitHandlerVector m_IsoXmitHandlers;
		IsoRecvHandlerVector m_IsoRecvHandlers;

    public:

        IsoHandlerManager();
        virtual ~IsoHandlerManager();

		int registerHandler(IsoHandler *);
		int unregisterHandler(IsoHandler *);


};

}

#endif /* __FREEBOB_ISOHANDLERMANAGER__  */



