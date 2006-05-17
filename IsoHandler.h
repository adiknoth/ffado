/* $Id$ */

/*
 *   FreeBob Streaming API
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
#ifndef __FREEBOB_ISOHANDLER__ 
#define __FREEBOB_ISOHANDLER__ 

#include <libraw1394/raw1394.h>


enum raw1394_iso_disposition ;
namespace FreebobStreaming
{

extern "C" {
		enum raw1394_iso_disposition iso_transmit_handler(raw1394handle_t handle,
				unsigned char *data, unsigned int *length,
				unsigned char *tag, unsigned char *sy,
				int cycle, unsigned int dropped);
}
/*!
\brief The Base Class for ISO Handlers

 These classes perform the actual ISO communication through libraw1394.
 They are different from IsoStreams because one handler can provide multiple
 streams with packets in case of ISO multichannel receive.

*/

class IsoHandler
{
	protected:
	
	public:
	
		enum EHandlerType {
			EHT_Receive,
			EHT_Transmit
		};
	
		IsoHandler() : m_handle(0)
		{}
		virtual ~IsoHandler()
		{}

	    bool Initialize( int port );
		

		virtual enum EHandlerType getType() = 0;

	private:
	    raw1394handle_t m_handle;
    	int             m_port;

};

/*!
\brief ISO receive handler class
*/

class IsoRecvHandler : public IsoHandler 
{

	public:
		IsoRecvHandler();
		virtual ~IsoRecvHandler();

	bool Initialize( int port );
		
		virtual enum EHandlerType getType() { return EHT_Receive;};
	
	private:
		enum raw1394_iso_disposition  
			PutPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped);

};

/*!
\brief ISO transmit handler class
*/

class IsoXmitHandler  : public IsoHandler 
{
   	public:
		friend enum raw1394_iso_disposition iso_transmit_handler(raw1394handle_t handle,
				unsigned char *data, unsigned int *length,
				unsigned char *tag, unsigned char *sy,
				int cycle, unsigned int dropped);

        IsoXmitHandler();
        virtual ~IsoXmitHandler();

	    bool Initialize( int port );
		
		virtual enum EHandlerType getType() { return EHT_Transmit;};

	private:
		enum raw1394_iso_disposition  
			GetPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped);


};

}

#endif /* __FREEBOB_ISOHANDLER__  */



