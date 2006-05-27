/* iavdevice.h
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

#ifndef IAVDEVICE_H
#define IAVDEVICE_H

#include "libfreebobavc/avc_definitions.h"
#include "libfreebob/xmlparser.h"

class ConfigRom;
class Ieee1394Service;

namespace FreebobStreaming {
	class StreamProcessor;
}
/*!
\brief Interface that is to be implemented to support a device.

 This interface should be used to implement freebob support for a specific device.
 
*/
class IAvDevice {
public:
	virtual ~IAvDevice() {}
	
	/// Returns the ConfigRom of the device node.
	virtual ConfigRom& getConfigRom() const = 0;
	
	/// this is called to discover & configure the device
	virtual bool discover() = 0;
	
	virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency ) = 0;
	virtual int getSamplingFrequency( ) = 0;
	
	/// obsolete
	virtual bool addXmlDescription( xmlNodePtr deviceNode ) = 0;
	
	/// debug: outputs the device configuration to stderr/stdout
	virtual void showDevice() const = 0;
	
	/** prepare the device
	 *  \todo when exactly is this called? and what should it do?
	 */
	virtual bool prepare() = 0;
	
	/// returns the number of ISO streams implemented/used by this device
	virtual int getStreamCount() = 0;
	
	/** returns the stream processor for the stream with index \ref i
	 * \param i : Stream index
	 * \pre \ref i smaller than getStreamCount()
	 */
	virtual FreebobStreaming::StreamProcessor *getStreamProcessorByIndex(int i) = 0;
	
	/** starts the stream with index \ref i
	 * \param i : Stream index
	 * \pre \ref i smaller than getStreamCount()
	 */
	virtual int startStreamByIndex(int i) = 0;
	
	/** stops the stream with index \ref i
	 * \param i : Stream index
	 * \pre \ref i smaller than getStreamCount()
	 */
	virtual int stopStreamByIndex(int i) = 0;
	
};

#endif
