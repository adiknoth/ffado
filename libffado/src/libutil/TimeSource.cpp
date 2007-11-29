/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include "TimeSource.h"

#include <time.h>

#include <assert.h>

#include "libutil/Time.h"

namespace Util {

IMPL_DEBUG_MODULE( TimeSource, TimeSource, DEBUG_LEVEL_NORMAL );

TimeSource::TimeSource() :
    m_Master(NULL), m_last_master_time(0), m_last_time(0),
    m_slave_rate(0.0), m_slave_offset(0), m_last_err(0.0)
{

}

TimeSource::~TimeSource() {

}
/**
 * (re)Initializes the TimeSource
 *
 * @return true if successful
 */
void TimeSource::initSlaveTimeSource() {
    ffado_microsecs_t my_time;
    ffado_microsecs_t master_time;
    ffado_microsecs_t my_time2;
    ffado_microsecs_t master_time2;

    if (m_Master) {
        my_time=getCurrentTime();
        master_time=m_Master->getCurrentTime();

        struct timespec ts;

        // sleep for ten milliseconds
        ts.tv_sec=0;
        ts.tv_nsec=10000000L;

        nanosleep(&ts,NULL);

        my_time2=getCurrentTime();
        master_time2=m_Master->getCurrentTime();

        float diff_slave=my_time2-my_time;
        float diff_master=master_time2-master_time;

        m_slave_rate=diff_slave/diff_master;

        // average of the two offset estimates
        m_slave_offset  = my_time-wrapTime((ffado_microsecs_t)(master_time*m_slave_rate));
        m_slave_offset += my_time2-wrapTime((ffado_microsecs_t)(master_time2*m_slave_rate));
        m_slave_offset /= 2;

        m_last_master_time=master_time2;
        m_last_time=my_time2;

        debugOutput(DEBUG_LEVEL_NORMAL,"init slave: master=%lu, master2=%lu, diff=%f\n",
            master_time, master_time2, diff_master);
        debugOutput(DEBUG_LEVEL_NORMAL,"init slave: slave =%lu, slave2 =%lu, diff=%f\n",
            my_time, my_time2, diff_slave);
        debugOutput(DEBUG_LEVEL_NORMAL,"init slave: slave rate=%f, slave_offset=%lu\n",
            m_slave_rate, m_slave_offset
            );
    }


}

/**
 * Maps a time point of the master to a time point
 * on it's own timeline
 *
 * @return the mapped time point
 */
ffado_microsecs_t TimeSource::mapMasterTime(ffado_microsecs_t master_time) {
    if(m_Master) {
        // calculate the slave based upon the master
        // and the estimated rate

        // linear interpolation
        int delta_master=master_time-m_last_master_time;

        float offset=m_slave_rate * ((float)delta_master);

        ffado_microsecs_t mapped=m_last_time+(int)offset;

        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"map time: master=%d, offset=%f, slave_base=%lu, pred_ticks=%lu\n",
            master_time, offset, m_last_time,mapped
            );

        return wrapTime(mapped);

    } else {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Requested map for non-slave TimeSource\n");

        return master_time;
    }
}

/**
 * Update the internal state of the TimeSource
 *
 */
bool TimeSource::updateTimeSource() {
    // update all slaves
    for ( TimeSourceVectorIterator it = m_Slaves.begin();
          it != m_Slaves.end(); ++it ) {

        // update the slave with the current
        // master time
        if (!(*it)->updateTimeSource()) return false;
    }

    // this TimeSource has a master
    if(m_Master) {
        ffado_microsecs_t my_time=getCurrentTime();
        ffado_microsecs_t master_time=m_Master->getCurrentTime();

        // we assume that the master and slave time are
        // measured at the same time, but that of course is
        // not really true. The DLL will have to filter this
        // out.

        // the difference in master time
        int64_t delta_master;
        if (master_time > m_last_master_time) {
            delta_master=master_time-m_last_master_time;
        } else { // wraparound
            delta_master=m_Master->unWrapTime(master_time)-m_last_master_time;
        }

        // the difference in slave time
        int64_t delta_slave;
        if (my_time > m_last_time) {
            delta_slave=my_time-m_last_time;
        } else { // wraparound
            delta_slave=unWrapTime(my_time)-m_last_time;
        }

        // the estimated slave difference
        int delta_slave_est=(int)(m_slave_rate * ((double)delta_master));

        // the measured & estimated rate
        double rate_meas=((double)delta_slave/(double)delta_master);
        double rate_est=((double)m_slave_rate);

        m_last_err=(rate_meas-rate_est);

        m_slave_rate += 0.01*m_last_err;

        debugOutput(DEBUG_LEVEL_VERBOSE,"update slave: master=%llu, master2=%llu, diff=%lld\n",
            master_time, m_last_master_time, delta_master);
        debugOutput(DEBUG_LEVEL_VERBOSE,"update slave: slave =%llu, slave2 =%llu, diff=%lld, diff_est=%d\n",
            my_time, m_last_time, delta_slave, delta_slave_est);
        debugOutput(DEBUG_LEVEL_VERBOSE,"update slave: slave rate meas=%f, slave rate est=%f, err=%f, slave rate=%f\n",
            rate_meas, rate_est, m_last_err, m_slave_rate
            );


        m_last_master_time=master_time;

        int64_t tmp = delta_slave_est;
        tmp += m_last_time;

        m_last_time = tmp;



    }

    return true;
}

/**
 * Sets the master TimeSource for this timesource.
 * This TimeSource will sync to the master TimeSource,
 * making that it will be able to map a time point of
 * the master to a time point on it's own timeline
 *
 * @param ts master TimeSource
 * @return true if successful
 */
bool TimeSource::setMaster(TimeSource *ts) {
    if (m_Master==NULL) {
        m_Master=ts;

        // initialize ourselves.
        initSlaveTimeSource();

        return true;
    } else return false;
}

/**
 * Clears the master TimeSource for this timesource.
 *
 * @return true if successful
 */
void TimeSource::clearMaster() {
    m_Master=NULL;
}

/**
 * Registers a slave timesource to this master.
 * A slave TimeSource will sync to this TimeSource,
 * making that it will be able to map a time point of
 * the master (this) TimeSource to a time point on
 * it's own timeline
 *
 * @param ts slave TimeSource to register
 * @return true if successful
 */
bool TimeSource::registerSlave(TimeSource *ts) {
    // TODO: we should check for circular master-slave relationships.

    debugOutput( DEBUG_LEVEL_VERBOSE, "Registering slave (%p)\n", ts);
    assert(ts);

    // inherit debug level
    ts->setVerboseLevel(getDebugLevel());

    if(ts->setMaster(this)) {
        m_Slaves.push_back(ts);
        return true;
    } else {
        return false;
    }
}

/**
 * Unregisters a slave TimeSource
 *
 * @param ts slave TimeSource to unregister
 * @return true if successful
 */
bool TimeSource::unregisterSlave(TimeSource *ts) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Unregistering TimeSource (%p)\n", ts);
    assert(ts);

    for ( TimeSourceVectorIterator it = m_Slaves.begin();
          it != m_Slaves.end(); ++it ) {

        if ( *it == ts ) {
            m_Slaves.erase(it);
            ts->clearMaster();
            return true;
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, " TimeSource (%p) not found\n", ts);

    return false;
}

/**
 * Set verbosity level.
 * All slave timesources get the same verbosity level
 *
 * @param l verbosity level
 */
void TimeSource::setVerboseLevel(int l) {
    setDebugLevel(l);

    for ( TimeSourceVectorIterator it = m_Slaves.begin();
          it != m_Slaves.end(); ++it ) {

        (*it)->setVerboseLevel(l);
    }

}

void TimeSource::printTimeSourceInfo() {
    debugOutputShort( DEBUG_LEVEL_NORMAL, "TimeSource (%p) info\n", this);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Master           : %p\n", m_Master);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Slave rate       : %f\n", m_slave_rate);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Slave offset     : %lld\n", m_slave_offset);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Last error       : %f\n", m_last_err);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Last master time : %llu\n",m_last_master_time );
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Last slave time  : %llu\n",m_last_time );


    for ( TimeSourceVectorIterator it = m_Slaves.begin();
          it != m_Slaves.end(); ++it ) {

        (*it)->printTimeSourceInfo();
    }
}

} // end of namespace Util
