/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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
#ifndef __FREEBOB_TIMESOURCE__
#define __FREEBOB_TIMESOURCE__

#include "../debugmodule/debugmodule.h"

#include <vector>

typedef uint64_t freebob_microsecs_t;

namespace FreebobUtil {

class TimeSource;
typedef std::vector<TimeSource *> TimeSourceVector;
typedef std::vector<TimeSource *>::iterator TimeSourceVectorIterator;

/*!
\brief The base class for all TimeSource's.

    Any object that can act as a source of timing 
    information should subclass this class and implement
    its virtual functions.
    
    A TimeSource can be slaved to another TimeSource, allowing
    the mapping of the master's time to the slave's time.
*/
class TimeSource {

public:

    TimeSource();
    virtual ~TimeSource();

    virtual freebob_microsecs_t getCurrentTime()=0;
    virtual freebob_microsecs_t getCurrentTimeAsUsecs()=0;
    virtual freebob_microsecs_t unWrapTime(freebob_microsecs_t t)=0;
    virtual freebob_microsecs_t wrapTime(freebob_microsecs_t t)=0;
    
    freebob_microsecs_t mapMasterTime(freebob_microsecs_t t);
    
    bool updateTimeSource();
    
    bool registerSlave(TimeSource *ts);
    bool unregisterSlave(TimeSource *ts);

    virtual void setVerboseLevel(int l);
    
    virtual void printTimeSourceInfo();
    
protected:

private:
    bool setMaster(TimeSource *ts);
    void clearMaster();
    
    void initSlaveTimeSource();
    
    TimeSource * m_Master;
    TimeSourceVector m_Slaves;
    
    freebob_microsecs_t m_last_master_time;
    freebob_microsecs_t m_last_time;
    
    double m_slave_rate;
    int64_t m_slave_offset;
    double m_last_err;
    
    DECLARE_DEBUG_MODULE;

};

} // end of namespace FreebobUtil

#endif /* __FREEBOB_TIMESOURCE__ */


