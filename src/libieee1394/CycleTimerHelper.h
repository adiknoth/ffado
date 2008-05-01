/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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
#ifndef __CYCLETIMERHELPER_H__
#define __CYCLETIMERHELPER_H__

/**
 * Implements a DLL based mechanism to track the cycle timer
 * register of the Ieee1394Service pointed to by the 'parent'.
 *
 * A DLL mechanism is performance-wise better suited, since it
 * does not require an OS call. Hence we run a thread to update
 * the DLL at regular time intervals, and then use the DLL to
 * generate a cycle timer estimate for the parent to pass on
 * to it's clients.
 *
 * The idea is to make reading the cycle timer real-time safe,
 * which isn't (necessarily)the case for the direct raw1394 call,
 * since it's a kernel call that could block (although the current
 * implementation is RT safe).
 *
 * This also allows us to run on systems not having the
 * raw1394_read_cycle_timer kernel call. We can always do a normal
 * read of our own cycle timer register, but that's not very accurate.
 * The accuracy is improved by this DLL mechanism. Still not as good
 * as when using the raw1394_read_cycle_timer call, but anyway.
 *
 * On the long run this code will also allow us to map system time
 * on to 1394 time for the current host controller, hence enabling
 * different clock domains to operate together.
 */

#include "libutil/Thread.h"
#include "libutil/SystemTimeSource.h"
#include "cycletimer.h"

#include "libutil/Functors.h"
#include "libutil/Mutex.h"

#include "debugmodule/debugmodule.h"

class Ieee1394Service;

class CycleTimerHelper : public Util::RunnableInterface
{
public:
    CycleTimerHelper(Ieee1394Service &, unsigned int);
    CycleTimerHelper(Ieee1394Service &, unsigned int, bool rt, int prio);
    ~CycleTimerHelper();

    virtual bool Init();
    virtual bool Execute();

    bool setThreadParameters(bool rt, int priority);
    bool Start();

    /**
     * @brief get the current cycle timer value (in ticks)
     * @note thread safe
     */
    uint32_t getCycleTimerTicks();

    /**
     * @brief get the cycle timer value for a specific time instant (in ticks)
     * @note thread safe
     */
    uint32_t getCycleTimerTicks(uint64_t now);

    /**
     * @brief get the current cycle timer value (in CTR format)
     * @note thread safe
     */
    uint32_t getCycleTimer();

    /**
     * @brief get the cycle timer value for a specific time instant (in CTR format)
     * @note thread safe
     */
    uint32_t getCycleTimer(uint64_t now);

    float getRate();
    float getNominalRate();

    /**
     * @brief handle a bus reset
     */
    void busresetHandler();

    void setVerboseLevel(int l);

private:
    bool readCycleTimerWithRetry(uint32_t *cycle_timer, uint64_t *local_time, int ntries);
    bool initValues();

#if IEEE1394SERVICE_USE_CYCLETIMER_DLL
    bool initDLL();
#endif

    Ieee1394Service &m_Parent;
    // parameters
    uint32_t m_ticks_per_update;
    uint32_t m_usecs_per_update;

    float    m_avg_wakeup_delay; 

    // state variables
    double m_dll_e2;

    double m_current_time_usecs;
    double m_next_time_usecs;
    double m_current_time_ticks;
    double m_next_time_ticks;
    bool m_first_run;
    ffado_microsecs_t m_sleep_until;

    uint32_t m_cycle_timer_prev;
    uint64_t m_cycle_timer_ticks_prev;
    int m_high_bw_updates;

    // cached vars used for computation
    struct compute_vars {
        uint64_t usecs;
        uint64_t ticks;
        double rate;
    };

    #define CTRHELPER_NB_SHADOW_VARS 8
    struct compute_vars m_shadow_vars[CTRHELPER_NB_SHADOW_VARS];
    volatile unsigned int m_current_shadow_idx;

    // Threading
    Util::Thread *  m_Thread;
    bool            m_realtime;
    unsigned int    m_priority;
    Util::Mutex*    m_update_lock;

    // busreset handling
    Util::Functor* m_busreset_functor;
    bool            m_unhandled_busreset;

#ifdef DEBUG
    uint64_t m_last_loop_entry;
    int m_successive_short_loops;
#endif

    // debug stuff
    DECLARE_DEBUG_MODULE;
};
#endif
