/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#include "CycleTimerHelper.h"
#include "ieee1394service.h"
#include "libutil/PosixThread.h"

#define DLL_BANDWIDTH (0.01)
#define DLL_PI        (3.141592653589793238)
#define DLL_SQRT2     (1.414213562373095049)
#define DLL_OMEGA     (2.0*DLL_PI*DLL_BANDWIDTH)
#define DLL_COEFF_B   (DLL_SQRT2 * DLL_OMEGA)
#define DLL_COEFF_C   (DLL_OMEGA * DLL_OMEGA)

/*
#define ENTER_CRITICAL_SECTION { \
    if (pthread_mutex_trylock(&m_compute_vars_lock) == EBUSY) { \
        debugWarning(" (%p) lock clash\n", this); \
        ENTER_CRITICAL_SECTION; \
    } \
    }
*/
#define ENTER_CRITICAL_SECTION { \
    ENTER_CRITICAL_SECTION; \
    }
#define EXIT_CRITICAL_SECTION { \
    EXIT_CRITICAL_SECTION; \
    }


IMPL_DEBUG_MODULE( CycleTimerHelper, CycleTimerHelper, DEBUG_LEVEL_NORMAL );

CycleTimerHelper::CycleTimerHelper(Ieee1394Service &parent, unsigned int update_period_us)
    : m_Parent ( parent )
    , m_ticks_per_update ( ((uint64_t)TICKS_PER_SECOND) * ((uint64_t)update_period_us) / 1000000ULL )
    , m_usecs_per_update ( update_period_us )
    , m_avg_wakeup_delay ( 0.0 )
    , m_dll_e2 ( 0.0 )
    , m_current_time_usecs ( 0 )
    , m_next_time_usecs ( 0 )
    , m_current_time_ticks ( 0 )
    , m_next_time_ticks ( 0 )
    , m_first_run ( true )
    , m_Thread ( NULL )
    , m_realtime ( false )
    , m_priority ( 0 )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create %p...\n", this);
}

CycleTimerHelper::CycleTimerHelper(Ieee1394Service &parent, unsigned int update_period_us, bool rt, int prio)
    : m_Parent ( parent )
    , m_ticks_per_update ( ((uint64_t)TICKS_PER_SECOND) * ((uint64_t)update_period_us) / 1000000ULL )
    , m_usecs_per_update ( update_period_us )
    , m_avg_wakeup_delay ( 0.0 )
    , m_dll_e2 ( 0.0 )
    , m_current_time_usecs ( 0 )
    , m_next_time_usecs ( 0 )
    , m_current_time_ticks ( 0 )
    , m_next_time_ticks ( 0 )
    , m_first_run ( true )
    , m_Thread ( NULL )
    , m_realtime ( rt )
    , m_priority ( prio )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create %p...\n", this);
}

CycleTimerHelper::~CycleTimerHelper()
{
    if (m_Thread) {
        m_Thread->Stop();
        delete m_Thread;
    }
}

bool
CycleTimerHelper::Start()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Start %p...\n", this);
    m_Thread = new Util::PosixThread(this, m_realtime, m_priority, 
                                     PTHREAD_CANCEL_DEFERRED);
    if(!m_Thread) {
        debugFatal("No thread\n");
        return false;
    }
    if (m_Thread->Start() != 0) {
        debugFatal("Could not start update thread\n");
        return false;
    }
    return true;
}

bool
CycleTimerHelper::Init()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initialize %p...\n", this);
    pthread_mutex_init(&m_compute_vars_lock, NULL);
    return true;
}

bool
CycleTimerHelper::setThreadParameters(bool rt, int priority) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) switch to: (rt=%d, prio=%d)...\n", this, rt, priority);
    if (priority > 98) priority = 98; // cap the priority
    m_realtime = rt;
    m_priority = priority;

    if (m_Thread) {
        if (m_realtime) {
            m_Thread->AcquireRealTime(m_priority);
        } else {
            m_Thread->DropRealTime();
        }
    }
    return true;
}

float
CycleTimerHelper::getRate()
{
    float rate = (float)(diffTicks((uint64_t)m_next_time_ticks, (uint64_t)m_current_time_ticks));
    rate /= (float)(m_next_time_usecs - m_current_time_usecs);
    return rate;
}

float
CycleTimerHelper::getNominalRate()
{
    float rate = ((double)TICKS_PER_SECOND) / 1000000.0;
    return rate;
}

#define OLD_STYLE
#ifdef OLD_STYLE

bool
CycleTimerHelper::Execute()
{
    usleep(m_usecs_per_update);
    return true;
}
uint32_t
CycleTimerHelper::getCycleTimerTicks()
{
    uint32_t cycle_timer;
    uint64_t local_time;
    if(!m_Parent.readCycleTimerReg(&cycle_timer, &local_time)) {
        debugError("Could not read cycle timer register\n");
        return false;
    }
    return CYCLE_TIMER_TO_TICKS(cycle_timer);
}

uint32_t
CycleTimerHelper::getCycleTimerTicks(uint64_t now)
{
    return getCycleTimerTicks();
}

#else

bool
CycleTimerHelper::Execute()
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Execute %p...\n", this);
    uint32_t cycle_timer;
    uint64_t local_time;
    if(!m_Parent.readCycleTimerReg(&cycle_timer, &local_time)) {
        debugError("Could not read cycle timer register\n");
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " read : CTR: %11lu, local: %17llu\n",
                    cycle_timer, local_time);

    double usecs_late;
    if (m_first_run) {
        usecs_late = 0.0;
        m_dll_e2 = m_ticks_per_update;
        m_current_time_usecs = local_time;
        m_next_time_usecs = m_current_time_usecs + m_usecs_per_update;
        m_current_time_ticks = CYCLE_TIMER_TO_TICKS( cycle_timer );
        m_next_time_ticks = addTicks( (uint64_t)m_current_time_ticks, (uint64_t)m_dll_e2);
        debugOutput( DEBUG_LEVEL_VERBOSE, " First run\n");
        debugOutput( DEBUG_LEVEL_VERBOSE, "  usecs/update: %lu, ticks/update: %lu, m_dll_e2: %f\n",
                                          m_usecs_per_update, m_ticks_per_update, m_dll_e2);
        debugOutput( DEBUG_LEVEL_VERBOSE, "  usecs current: %f, next: %f\n", m_current_time_usecs, m_next_time_usecs);
        debugOutput( DEBUG_LEVEL_VERBOSE, "  ticks current: %f, next: %f\n", m_current_time_ticks, m_next_time_ticks);
        m_first_run = false;
    } else {

        double diff = m_next_time_usecs - m_current_time_usecs;
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " usecs: local: %11llu current: %f next: %f, diff: %f\n",
                    local_time, m_current_time_usecs, m_next_time_usecs, diff);

        uint64_t cycle_timer_ticks = CYCLE_TIMER_TO_TICKS(cycle_timer);
        usecs_late = ((double)local_time) - (m_next_time_usecs);

        // we update the x-axis values
        m_current_time_usecs = m_next_time_usecs;
        m_next_time_usecs = (local_time - usecs_late) + m_usecs_per_update;
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " usecs: current: %f next: %f usecs_late=%f\n",
                    m_current_time_usecs, m_next_time_usecs, usecs_late);

        // and the y-axis values
        double diff_ticks = diffTicks(cycle_timer_ticks, (int64_t)m_next_time_ticks);
        m_current_time_ticks = m_next_time_ticks;
        m_next_time_ticks = addTicks((uint64_t)m_current_time_ticks,
                                     (uint64_t)((DLL_COEFF_B * diff_ticks) + m_dll_e2));
        m_dll_e2 += DLL_COEFF_C * diff_ticks;
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " ticks: current: %f next: %f diff=%f\n",
                    m_current_time_ticks, m_next_time_ticks, diff_ticks);

        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " state: local: %11llu, dll_e2: %f, rate: %f\n",
                    local_time, m_dll_e2, getRate());
    }

    // track the average wakeup delay
    m_avg_wakeup_delay += 0.01 * usecs_late;

    // FIXME: priority inversion!
    ENTER_CRITICAL_SECTION;
    m_current_vars.ticks = m_current_time_ticks;
    m_current_vars.usecs = m_current_time_usecs;
    m_current_vars.rate = getRate();
    EXIT_CRITICAL_SECTION;

    // wait for the next update period
    int64_t time_to_sleep = (int64_t)m_next_time_usecs - m_Parent.getCurrentTimeAsUsecs();
    time_to_sleep -= (int64_t)m_avg_wakeup_delay;
    //int64_t time_to_sleep = m_usecs_per_update;
    if (time_to_sleep > 0) {
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " sleeping %lld usecs (avg delay: %f)\n", time_to_sleep, m_avg_wakeup_delay);
        usleep(time_to_sleep);
    }
    return true;
}

uint32_t
CycleTimerHelper::getCycleTimerTicks()
{
    uint64_t now = m_Parent.getCurrentTimeAsUsecs();
    return getCycleTimerTicks(now);
}

uint32_t
CycleTimerHelper::getCycleTimerTicks(uint64_t now)
{
    uint32_t retval;
    struct compute_vars my_vars;

    // reduce lock contention
    ENTER_CRITICAL_SECTION;
    my_vars = m_current_vars;
    EXIT_CRITICAL_SECTION;

    double time_diff = now - my_vars.usecs;
    double y_step_in_ticks = time_diff * my_vars.rate;
    int64_t y_step_in_ticks_int = (int64_t)y_step_in_ticks;
    uint64_t offset_in_ticks_int = (uint64_t)my_vars.ticks;

    if (y_step_in_ticks_int > 0) {
        retval = addTicks(offset_in_ticks_int, y_step_in_ticks_int);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "y_step_in_ticks_int > 0: %lld, time_diff: %f, rate: %f, retval: %lu\n", 
                     y_step_in_ticks_int, time_diff, my_vars.rate, retval);
    } else {
        retval = substractTicks(offset_in_ticks_int, -y_step_in_ticks_int);

        // this can happen if the update thread was woken up earlier than it should have been
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "y_step_in_ticks_int <= 0: %lld, time_diff: %f, rate: %f, retval: %lu\n", 
                     y_step_in_ticks_int, time_diff, my_vars.rate, retval);
    }

    return retval;
}
#endif

uint32_t
CycleTimerHelper::getCycleTimer()
{
    return TICKS_TO_CYCLE_TIMER(getCycleTimerTicks());
}

uint32_t
CycleTimerHelper::getCycleTimer(uint64_t now)
{
    return TICKS_TO_CYCLE_TIMER(getCycleTimerTicks(now));
}

void
CycleTimerHelper::setVerboseLevel(int l)
{
    setDebugLevel(l);
}
