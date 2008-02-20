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

#include "config.h"

#include "CycleTimerHelper.h"
#include "ieee1394service.h"
#include "libutil/PosixThread.h"

#define DLL_PI        (3.141592653589793238)
#define DLL_SQRT2     (1.414213562373095049)

// the high-bandwidth coefficients are used
// to speed up inital tracking
#define DLL_BANDWIDTH_HIGH (0.1)
#define DLL_OMEGA_HIGH     (2.0*DLL_PI*DLL_BANDWIDTH_HIGH)
#define DLL_COEFF_B_HIGH   (DLL_SQRT2 * DLL_OMEGA_HIGH)
#define DLL_COEFF_C_HIGH   (DLL_OMEGA_HIGH * DLL_OMEGA_HIGH)

// the low-bandwidth coefficients are used once we have a
// good estimate of the internal parameters
#define DLL_BANDWIDTH (0.01)
#define DLL_OMEGA     (2.0*DLL_PI*DLL_BANDWIDTH)
#define DLL_COEFF_B   (DLL_SQRT2 * DLL_OMEGA)
#define DLL_COEFF_C   (DLL_OMEGA * DLL_OMEGA)

#define UPDATES_WITH_HIGH_BANDWIDTH 200

/*
#define ENTER_CRITICAL_SECTION { \
    if (pthread_mutex_trylock(&m_compute_vars_lock) == EBUSY) { \
        debugWarning(" (%p) lock clash\n", this); \
        ENTER_CRITICAL_SECTION; \
    } \
    }
*/
#define ENTER_CRITICAL_SECTION { \
    pthread_mutex_lock(&m_compute_vars_lock); \
    }
#define EXIT_CRITICAL_SECTION { \
    pthread_mutex_unlock(&m_compute_vars_lock); \
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
    , m_sleep_until ( 0 )
    , m_cycle_timer_prev ( 0 )
    , m_cycle_timer_ticks_prev ( 0 )
    , m_high_bw_updates ( UPDATES_WITH_HIGH_BANDWIDTH )
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
    , m_sleep_until ( 0 )
    , m_cycle_timer_prev ( 0 )
    , m_cycle_timer_ticks_prev ( 0 )
    , m_high_bw_updates ( UPDATES_WITH_HIGH_BANDWIDTH )
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
#if IEEE1394SERVICE_USE_CYCLETIMER_DLL
    m_high_bw_updates = UPDATES_WITH_HIGH_BANDWIDTH;
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
#endif
    return true;
}

bool
CycleTimerHelper::Init()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initialize %p...\n", this);
    pthread_mutex_init(&m_compute_vars_lock, NULL);
    
    // initialize the 'prev ctr' values
    uint64_t local_time;
    int maxtries2 = 10;
    do {
        if(!m_Parent.readCycleTimerReg(&m_cycle_timer_prev, &local_time)) {
            debugError("Could not read cycle timer register\n");
            return false;
        }
        if (m_cycle_timer_prev == 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "Bogus CTR: %08X on try %02d\n",
                        m_cycle_timer_prev, maxtries2);
        }
    } while (m_cycle_timer_prev == 0 && maxtries2--);
    m_cycle_timer_ticks_prev = CYCLE_TIMER_TO_TICKS(m_cycle_timer_prev);
    return true;
}

bool
CycleTimerHelper::setThreadParameters(bool rt, int priority) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) switch to: (rt=%d, prio=%d)...\n", this, rt, priority);
    if (priority > THREAD_MAX_RTPRIO) priority = THREAD_MAX_RTPRIO; // cap the priority
    m_realtime = rt;
    m_priority = priority;

#if IEEE1394SERVICE_USE_CYCLETIMER_DLL
    if (m_Thread) {
        if (m_realtime) {
            m_Thread->AcquireRealTime(m_priority);
        } else {
            m_Thread->DropRealTime();
        }
    }
#endif

    return true;
}

#if IEEE1394SERVICE_USE_CYCLETIMER_DLL
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

bool
CycleTimerHelper::Execute()
{
    debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, "Execute %p...\n", this);
    if (!m_first_run) {
        // wait for the next update period
        ffado_microsecs_t now = m_TimeSource.getCurrentTimeAsUsecs();
        int sleep_time = m_sleep_until - now;
        debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, "(%p) Sleep until %lld/%f (now: %lld, diff=%d) ...\n",
                    this, m_sleep_until, m_next_time_usecs, now, sleep_time);
        m_TimeSource.SleepUsecAbsolute(m_sleep_until);
        debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, " (%p) back...\n", this);
    }

    uint32_t cycle_timer;
    uint64_t local_time;
    int64_t usecs_late;
    int ntries=2;
    uint64_t cycle_timer_ticks;
    double diff_ticks;

    // if the difference between the predicted value and the
    // actual value seems to be too large, retry reading the cycle timer
    // some host controllers return bogus values on some reads
    // (looks like a non-atomic update of the register)
    do {
        if(!readCycleTimerWithRetry(&cycle_timer, &local_time, 10)) {
            debugError("Could not read cycle timer register\n");
            return false;
        }
        usecs_late = local_time - m_sleep_until;
        cycle_timer_ticks = CYCLE_TIMER_TO_TICKS(cycle_timer);
        diff_ticks = diffTicks(cycle_timer_ticks, (int64_t)m_next_time_ticks);
        if(diff_ticks < -((double)TICKS_PER_HALFCYCLE)) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "have to retry, diff = %f\n",diff_ticks);
        }
        
    } while(diff_ticks < -((double)TICKS_PER_HALFCYCLE) && --ntries && !m_first_run);

    debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, " read : CTR: %11lu, local: %17llu\n",
                        cycle_timer, local_time);

    if (m_first_run) {
        m_sleep_until = local_time + m_usecs_per_update;
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
        m_sleep_until += m_usecs_per_update;

        double diff_ticks_corr;
        // correct for late wakeup
        int64_t ticks_late = (usecs_late * TICKS_PER_SECOND) / 1000000LL;
        if (ticks_late > 0) {
            // if we are usecs_late usecs late 
            // the cycle timer has ticked approx ticks_late ticks too much
            cycle_timer_ticks = substractTicks(cycle_timer_ticks, ticks_late);
            diff_ticks_corr = diff_ticks - ticks_late;
        } else {
            debugError("Early wakeup, should not happen!\n");
            // recover
            cycle_timer_ticks = addTicks(cycle_timer_ticks, -ticks_late);
            diff_ticks_corr = diff_ticks + ticks_late;
        }

        #ifdef DEBUG
        if(usecs_late > 20) {
            debugWarning("Rather late wakeup: %lld usecs\n", usecs_late);
        }
        #endif

        // update the x-axis values
        m_current_time_ticks = m_next_time_ticks;

        // decide what coefficients to use
        double coeff_b, coeff_c;
        if (m_high_bw_updates > 0) {
            coeff_b = DLL_COEFF_B_HIGH;
            coeff_c = DLL_COEFF_C_HIGH;
            m_high_bw_updates--;
            if (m_high_bw_updates == 0) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Switching to low-bandwidth coefficients\n");
            }
        } else {
            coeff_b = DLL_COEFF_B;
            coeff_c = DLL_COEFF_C;
        }

        // do the calculation in 'tick space'
        int64_t tmp = (uint64_t)(coeff_b * diff_ticks_corr);
        if(m_dll_e2 > 0) {
            tmp = addTicks(tmp, (uint64_t)m_dll_e2);
        } else {
            tmp = substractTicks(tmp, (uint64_t)(-m_dll_e2));
        }
        if(tmp < 0) {
            debugWarning("negative slope: %lld!\n", tmp);
        }
        m_next_time_ticks = addTicks((uint64_t)m_current_time_ticks, tmp);

        // it should be ok to not do this in tick space since it's value
        // is approx equal to the rate, being 24.576 ticks/usec
        m_dll_e2 += coeff_c * diff_ticks_corr;

        // For jitter graphs
        debugOutputShort(DEBUG_LEVEL_NORMAL, "0123456789 %f %f %f %lld %lld %lld\n", 
                         diff_ticks, diff_ticks_corr, m_dll_e2, cycle_timer_ticks, (int64_t)m_next_time_ticks, usecs_late);

        // update the y-axis values
        m_current_time_usecs = m_next_time_usecs;
        m_next_time_usecs += m_usecs_per_update;

        debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, " usecs: current: %f next: %f usecs_late=%lld ticks_late=%lld\n",
                            m_current_time_usecs, m_next_time_usecs, usecs_late, ticks_late);
        debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, " ticks: current: %f next: %f diff=%f\n",
                            m_current_time_ticks, m_next_time_ticks, diff_ticks);
        debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, " state: local: %11llu, dll_e2: %f, rate: %f\n",
                            local_time, m_dll_e2, getRate());
    }

    // FIXME: priority inversion possible, run this at higher prio than client threads
    ENTER_CRITICAL_SECTION;
    m_current_vars.ticks = (uint64_t)(m_current_time_ticks);
    m_current_vars.usecs = (uint64_t)m_current_time_usecs;
    m_current_vars.rate = getRate();
    EXIT_CRITICAL_SECTION;

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

    int64_t time_diff = now - my_vars.usecs;
    double y_step_in_ticks = ((double)time_diff) * my_vars.rate;
    int64_t y_step_in_ticks_int = (int64_t)y_step_in_ticks;
    uint64_t offset_in_ticks_int = my_vars.ticks;

    if (y_step_in_ticks_int > 0) {
        retval = addTicks(offset_in_ticks_int, y_step_in_ticks_int);
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "y_step_in_ticks_int > 0: %lld, time_diff: %f, rate: %f, retval: %lu\n", 
                    y_step_in_ticks_int, time_diff, my_vars.rate, retval);
    } else {
        retval = substractTicks(offset_in_ticks_int, -y_step_in_ticks_int);

        // this can happen if the update thread was woken up earlier than it should have been
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "y_step_in_ticks_int <= 0: %lld, time_diff: %f, rate: %f, retval: %lu\n", 
                    y_step_in_ticks_int, time_diff, my_vars.rate, retval);
    }

    return retval;
}

uint32_t
CycleTimerHelper::getCycleTimer()
{
    uint64_t now = m_Parent.getCurrentTimeAsUsecs();
    return getCycleTimer(now);
}

uint32_t
CycleTimerHelper::getCycleTimer(uint64_t now)
{
    uint32_t ticks = getCycleTimerTicks(now);
    uint32_t result = TICKS_TO_CYCLE_TIMER(ticks);
#ifdef DEBUG
    if(CYCLE_TIMER_TO_TICKS(result) != ticks) {
        debugWarning("Bad ctr conversion");
    }
#endif
    return result;
}

#else

float
CycleTimerHelper::getRate()
{
    return getNominalRate();
}

float
CycleTimerHelper::getNominalRate()
{
    float rate = ((double)TICKS_PER_SECOND) / 1000000.0;
    return rate;
}

bool
CycleTimerHelper::Execute()
{
    usleep(1000*1000);
    return true;
}

uint32_t
CycleTimerHelper::getCycleTimerTicks()
{
    return CYCLE_TIMER_TO_TICKS(getCycleTimer());
}

uint32_t
CycleTimerHelper::getCycleTimerTicks(uint64_t now)
{
    debugWarning("not implemented!\n");
    return getCycleTimerTicks();
}

uint32_t
CycleTimerHelper::getCycleTimer()
{
    uint32_t cycle_timer;
    uint64_t local_time;
    readCycleTimerWithRetry(&cycle_timer, &local_time, 10);
    return cycle_timer;
}

uint32_t
CycleTimerHelper::getCycleTimer(uint64_t now)
{
    debugWarning("not implemented!\n");
    return getCycleTimer();
}

#endif

bool
CycleTimerHelper::readCycleTimerWithRetry(uint32_t *cycle_timer, uint64_t *local_time, int ntries)
{
    bool good=false;
    int maxtries = ntries;

    do {
        // the ctr read can return 0 sometimes. if that happens, reread the ctr.
        int maxtries2=ntries;
        do {
            if(!m_Parent.readCycleTimerReg(cycle_timer, local_time)) {
                debugError("Could not read cycle timer register\n");
                return false;
            }
            if (*cycle_timer == 0) {
                debugOutput(DEBUG_LEVEL_VERBOSE,
                           "Bogus CTR: %08X on try %02d\n",
                           *cycle_timer, maxtries2);
            }
        } while (*cycle_timer == 0 && maxtries2--);
        
        // catch bogus ctr reads (can happen)
        uint64_t cycle_timer_ticks = CYCLE_TIMER_TO_TICKS(*cycle_timer);
    
        if (diffTicks(cycle_timer_ticks, m_cycle_timer_ticks_prev) < 0) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                        "non-monotonic CTR (try %02d): %llu -> %llu\n",
                        maxtries, m_cycle_timer_ticks_prev, cycle_timer_ticks);
            debugOutput( DEBUG_LEVEL_VERBOSE,
                        "                            : %08X -> %08X\n",
                        m_cycle_timer_prev, *cycle_timer);
            debugOutput( DEBUG_LEVEL_VERBOSE,
                        " current: %011llu (%03us %04ucy %04uticks)\n",
                        cycle_timer_ticks,
                        (unsigned int)TICKS_TO_SECS( cycle_timer_ticks ),
                        (unsigned int)TICKS_TO_CYCLES( cycle_timer_ticks ),
                        (unsigned int)TICKS_TO_OFFSET( cycle_timer_ticks ) );
            debugOutput( DEBUG_LEVEL_VERBOSE,
                        " prev   : %011llu (%03us %04ucy %04uticks)\n",
                        m_cycle_timer_ticks_prev,
                        (unsigned int)TICKS_TO_SECS( m_cycle_timer_ticks_prev ),
                        (unsigned int)TICKS_TO_CYCLES( m_cycle_timer_ticks_prev ),
                        (unsigned int)TICKS_TO_OFFSET( m_cycle_timer_ticks_prev ) );
        } else {
            good = true;
            m_cycle_timer_prev = *cycle_timer;
            m_cycle_timer_ticks_prev = cycle_timer_ticks;
        }
    } while (!good && maxtries--);
    return true;
}

void
CycleTimerHelper::setVerboseLevel(int l)
{
    setDebugLevel(l);
}
