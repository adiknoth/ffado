/*
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

#ifndef __FFADO_TIMESOURCE__
#define __FFADO_TIMESOURCE__

#include "../debugmodule/debugmodule.h"

#include <vector>

typedef uint64_t ffado_microsecs_t;

namespace Util {

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

    virtual ffado_microsecs_t getCurrentTime()=0;
    virtual ffado_microsecs_t getCurrentTimeAsUsecs()=0;
    virtual ffado_microsecs_t unWrapTime(ffado_microsecs_t t)=0;
    virtual ffado_microsecs_t wrapTime(ffado_microsecs_t t)=0;

    ffado_microsecs_t mapMasterTime(ffado_microsecs_t t);

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

    ffado_microsecs_t m_last_master_time;
    ffado_microsecs_t m_last_time;

    double m_slave_rate;
    int64_t m_slave_offset;
    double m_last_err;

    DECLARE_DEBUG_MODULE;

};

} // end of namespace Util

#endif /* __FFADO_TIMESOURCE__ */


