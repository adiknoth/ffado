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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>

#include <getopt.h>

#include <signal.h>
#include "src/debugmodule/debugmodule.h"

#include "libutil/ByteSwap.h"

#include "src/libieee1394/cycletimer.h"
#include "src/libieee1394/configrom.h"
#include "src/libieee1394/ieee1394service.h"

#include "src/libutil/Thread.h"
#include "src/libutil/Functors.h"
#include "src/libutil/PosixThread.h"
#include <libraw1394/raw1394.h>
#include "libutil/Time.h"


#define NB_THREADS 1
#define THREAD_RT  true
#define THREAD_PRIO 51
#define THREAD_SLEEP_US 50000

#define DISP_CYCLE_SLEEP_SECS 2

using namespace Util;

DECLARE_GLOBAL_DEBUG_MODULE;

#define DIFF_CONSIDERED_LARGE (TICKS_PER_CYCLE/2)
int PORT_TO_USE = 0;
int VERBOSE_LEVEL = 4;

int max_diff=-99999;
int min_diff= 99999;

int run=1;
static void sighandler (int sig)
{
    run = 0;
}

class MyFunctor : public Functor
{
public:
    MyFunctor() {}
    virtual ~MyFunctor() {}

    void operator() () {
        printf("hello from the functor (%p)\n", this);
    };
    virtual bool matchCallee(void *) {return false;};
};

class CtrThread : public Util::RunnableInterface
{
    public:
        CtrThread(Ieee1394Service *s)
        : m_service(s)
        {};
        virtual ~CtrThread() {};
        virtual bool Init()
        {
            debugOutput(DEBUG_LEVEL_NORMAL, "(%p) Init\n", this);
            ctr = 0;
            ctr_dll = 0;
        
            ctr_prev = 0;
            ctr_dll_prev = 0;
            nb_checks = 0;
            summed_diff = 0;
            avg_diff = 0;
            m_reset_avg = 1;
            m_handle = raw1394_new_handle_on_port( PORT_TO_USE );
            if ( !m_handle ) {
                if ( !errno ) {
                    debugFatal("libraw1394 not compatible\n");
                } else {
                    debugFatal("Ieee1394Service::initialize: Could not get 1394 handle: %s\n",
                        strerror(errno) );
                    debugFatal("Is ieee1394 and raw1394 driver loaded?\n");
                }
                return false;
            }
            return true;
        }
        virtual bool Execute();

        Ieee1394Service *m_service;
        raw1394handle_t m_handle;
        uint64_t ctr;
        uint64_t ctr_dll;

        uint64_t ctr_prev;
        uint64_t ctr_dll_prev;
        
        uint64_t nb_checks;
        int64_t summed_diff;
        double avg_diff;
        int m_reset_avg;
};

bool CtrThread::Execute() {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "(%p) Execute\n", this);
    
    SleepRelativeUsec(THREAD_SLEEP_US);

    uint32_t cycle_timer;
    uint64_t local_time;
    uint32_t cycle_timer2;
    uint64_t local_time2;
    uint64_t ticks1, ticks2;
    int err;

    do {
        // read the CTR 'raw' from a handle
        // and read it from the 1394 service, which uses a DLL
        err = raw1394_read_cycle_timer(m_handle, &cycle_timer2, &local_time2);
        err = raw1394_read_cycle_timer(m_handle, &cycle_timer, &local_time);
        
        ticks1 = CYCLE_TIMER_TO_TICKS(cycle_timer);
        ticks2 = CYCLE_TIMER_TO_TICKS(cycle_timer2);
    } while (diffTicks(ticks1, ticks2) < 0);
    
    ctr_prev = ctr;
    ctr_dll_prev = ctr_dll;
    
    ctr = CYCLE_TIMER_TO_TICKS( cycle_timer );
    ctr_dll = m_service->getCycleTimerTicks(local_time);

    if(err) {
        debugError("(%p) CTR read error\n", this);
    }
    debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                "(%p) Cycle timer: %011llu (%03us %04ucy %04uticks)\n",
                this, ctr,
                (unsigned int)TICKS_TO_SECS( ctr ),
                (unsigned int)TICKS_TO_CYCLES( ctr ),
                (unsigned int)TICKS_TO_OFFSET( ctr ) );
    debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                "(%p)    from DLL: %011llu (%03us %04ucy %04uticks)\n",
                this, ctr_dll,
                (unsigned int)TICKS_TO_SECS( ctr_dll ),
                (unsigned int)TICKS_TO_CYCLES( ctr_dll ),
                (unsigned int)TICKS_TO_OFFSET( ctr_dll ) );
    int64_t diff = diffTicks(ctr, ctr_dll);
    uint64_t abs_diff;
    // for jitter plots
    //     debugOutput(DEBUG_LEVEL_NORMAL, "9876543210: %lld\n", diff);

    if(m_reset_avg) {
        m_reset_avg = 0;
        summed_diff = 0;
        nb_checks = 0;
    }

    // not 100% thread safe, but will do
    if (diff > max_diff) max_diff = diff;
    if (diff < min_diff) min_diff = diff;
    summed_diff += diff;
    nb_checks++;
    avg_diff = ((double)summed_diff)/((double)nb_checks);
    
    if (diff < 0) {
        abs_diff = -diff;
    } else {
        abs_diff = diff;
    }
    debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                "(%p)       diff: %s%011llu (%03us %04ucy %04uticks)\n", this,
                ((int64_t)abs_diff==diff?" ":"-"), abs_diff, (unsigned int)TICKS_TO_SECS( abs_diff ),
                (unsigned int)TICKS_TO_CYCLES( abs_diff ), (unsigned int)TICKS_TO_OFFSET( abs_diff ) );
    if (abs_diff > DIFF_CONSIDERED_LARGE) {
        debugWarning("(%p) Alert, large diff: %lld\n", this, diff);
        debugOutput ( DEBUG_LEVEL_NORMAL,
                    "(%p)  Cycle timer: %011llu (%03us %04ucy %04uticks)\n",
                    this, ctr,
                    (unsigned int)TICKS_TO_SECS( ctr ),
                    (unsigned int)TICKS_TO_CYCLES( ctr ),
                    (unsigned int)TICKS_TO_OFFSET( ctr ) );
        debugOutput ( DEBUG_LEVEL_NORMAL,
                    "(%p)   from DLL: %011llu (%03us %04ucy %04uticks)\n",
                    this, ctr_dll,
                    (unsigned int)TICKS_TO_SECS( ctr_dll ),
                    (unsigned int)TICKS_TO_CYCLES( ctr_dll ),
                    (unsigned int)TICKS_TO_OFFSET( ctr_dll ) );
    }
    
    diff = diffTicks(ctr, ctr_prev);
    if (diff < 0) {
        debugWarning("(%p) Alert, non-monotonic ctr (direct): %llu - %llu = %lld\n",
                     this, ctr, ctr_prev, diff);
        debugOutput ( DEBUG_LEVEL_NORMAL,
                    "(%p)  Cycle timer now : %011llu (%03us %04ucy %04uticks)\n",
                    this, ctr,
                    (unsigned int)TICKS_TO_SECS( ctr ),
                    (unsigned int)TICKS_TO_CYCLES( ctr ),
                    (unsigned int)TICKS_TO_OFFSET( ctr ) );
        debugOutput ( DEBUG_LEVEL_NORMAL,
                    "(%p)  Cycle timer prev: %011llu (%03us %04ucy %04uticks)\n",
                    this, ctr_prev,
                    (unsigned int)TICKS_TO_SECS( ctr_prev ),
                    (unsigned int)TICKS_TO_CYCLES( ctr_prev ),
                    (unsigned int)TICKS_TO_OFFSET( ctr_prev ) );
    }
    diff = diffTicks(ctr_dll, ctr_dll_prev);
    if (diff < 0) {
        debugWarning("(%p) Alert, non-monotonic ctr (dll): %llu - %llu = %lld\n",
                     this, ctr_dll, ctr_dll_prev, diff);
        debugOutput ( DEBUG_LEVEL_NORMAL,
                    "(%p)  Cycle timer now : %011llu (%03us %04ucy %04uticks)\n",
                    this, ctr_dll,
                    (unsigned int)TICKS_TO_SECS( ctr_dll ),
                    (unsigned int)TICKS_TO_CYCLES( ctr_dll ),
                    (unsigned int)TICKS_TO_OFFSET( ctr_dll ) );
        debugOutput ( DEBUG_LEVEL_NORMAL,
                    "(%p)  Cycle timer prev: %011llu (%03us %04ucy %04uticks)\n",
                    this, ctr_dll_prev,
                    (unsigned int)TICKS_TO_SECS( ctr_dll_prev ),
                    (unsigned int)TICKS_TO_CYCLES( ctr_dll_prev ),
                    (unsigned int)TICKS_TO_OFFSET( ctr_dll_prev ) );
    }
    
    // check some calculations
    uint32_t tmp_orig = m_service->getCycleTimer();
    uint32_t tmp_ticks = CYCLE_TIMER_TO_TICKS(tmp_orig);
    uint32_t tmp_ctr = TICKS_TO_CYCLE_TIMER(tmp_ticks);
    
    if (tmp_orig != tmp_ctr) {
        debugError("CTR => TICKS => CTR failed\n");
        debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                    "(%p) orig CTR : %08X (%03us %04ucy %04uticks)\n",
                    this, (uint32_t)tmp_orig,
                    (unsigned int)CYCLE_TIMER_GET_SECS( tmp_orig ),
                    (unsigned int)CYCLE_TIMER_GET_CYCLES( tmp_orig ),
                    (unsigned int)CYCLE_TIMER_GET_OFFSET( tmp_orig ) );
        debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                    "(%p) TICKS: %011llu (%03us %04ucy %04uticks)\n",
                    this, tmp_ticks,
                    (unsigned int)TICKS_TO_SECS( tmp_ticks ),
                    (unsigned int)TICKS_TO_CYCLES( tmp_ticks ),
                    (unsigned int)TICKS_TO_OFFSET( tmp_ticks ) );
        debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                    "(%p) new CTR : %08X (%03us %04ucy %04uticks)\n",
                    this, (uint32_t)tmp_ctr,
                    (unsigned int)CYCLE_TIMER_GET_SECS( tmp_ctr ),
                    (unsigned int)CYCLE_TIMER_GET_CYCLES( tmp_ctr ),
                    (unsigned int)CYCLE_TIMER_GET_OFFSET( tmp_ctr ) );
    }
    
    debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                "(%p)  wait...\n", this);
    return true;
}

int main(int argc, char *argv[])
{
    int i=0;
    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    static struct option long_opts[] = { { "port", 1, 0, 'p' },  { "verbose", 1, 0, 'v' }, { "help", 0, 0, 'h' }, { 0, 0, 0, 0 } };
    int optindex = 0;
    while(1) {
        int c = getopt_long( argc, argv, "p:h", long_opts, &optindex );
        if(c==-1)
            break;
        switch(c) {
            case 'p':
                PORT_TO_USE = atoi( optarg );
                break;
            case 'v':
                VERBOSE_LEVEL = atoi( optarg );
                break;
            case 'h':
                printf( "USAGE:\n\
 Currently two options are understood:\n\
  --port <number> or -p <number> selects the firewire-port to use, default is 0\n\
  --verbose <number> or -v <number> selects the verbose level, default is 4\n\
  --help or -h shows this help and exits.\n\
" );
                return 0;
                break;
        }
    }

    printf("FFADO Ieee1394Service test application\n");
    printf(" Using port %d\n", PORT_TO_USE);
    printf(" Verbose level %d\n", VERBOSE_LEVEL);

    setDebugLevel(VERBOSE_LEVEL);

    Ieee1394Service *m_service=NULL;

    m_service = new Ieee1394Service();
    m_service->setVerboseLevel(VERBOSE_LEVEL);
    if(!m_service->initialize(PORT_TO_USE)) {
        printf("Could not initialize 1394 service\n");
        delete m_service;
        exit(-1);
    }
    m_service->setThreadParameters(true, 1);

    MyFunctor *test_busreset=new MyFunctor();

    printf(" adding (%p) as busreset handler\n", test_busreset);

    m_service->addBusResetHandler(test_busreset);

    nodeaddr_t addr =  m_service->findFreeARMBlock(0x0000FFFFE0000000ULL, 4, 4 );

    Ieee1394Service::ARMHandler *test_arm = new Ieee1394Service::ARMHandler(*m_service, addr,
                           4,
                           RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
                           RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
                           0);

    printf(" adding (%p) as arm handler\n", test_arm);

    if (!m_service->registerARMHandler(test_arm)) {
        printf("  failed\n");
    }

    addr =  m_service->findFreeARMBlock(0x0000FFFFE0000000ULL, 4, 4 );

    Ieee1394Service::ARMHandler *test_arm2 = new Ieee1394Service::ARMHandler(*m_service, addr,
                            4,
                            RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
                            RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
                            0);

    printf(" adding (%p) as arm handler\n", test_arm2);

    if (!m_service->registerARMHandler(test_arm2)) {
        printf("  failed\n");
    }

    CtrThread *thread_runners[NB_THREADS];
    Thread* threads[NB_THREADS];
    for (i=0; i < NB_THREADS; i++) {
        thread_runners[i] = new CtrThread(m_service);
        if (thread_runners[i] == NULL) {
            debugError("could not create thread runner %d\n", i);
            exit(-1);
        }
        threads[i] = new PosixThread(thread_runners[i], THREAD_RT, THREAD_PRIO, PTHREAD_CANCEL_DEFERRED);
        if (threads[i] == NULL) {
            debugError("could not create thread %d\n", i);
            exit(-1);
        }
    }
    
    for (i=0; i < NB_THREADS; i++) {
        threads[i]->Start();
    }

    int cnt=0;
    while(run) {
        cnt++;
        debugOutput(DEBUG_LEVEL_NORMAL, "%08d: (max: %6d, min: %6d)\n", cnt, max_diff, min_diff);
        m_service->show();
        max_diff = -999999;
        min_diff = 999999;
        
        for (i=0; i < NB_THREADS; i++) {
            debugOutput(DEBUG_LEVEL_NORMAL, "%2d: avg: %6f\n", i,  thread_runners[i]->avg_diff);
            thread_runners[i]->m_reset_avg = 1;
        }
        
        
        sleep(DISP_CYCLE_SLEEP_SECS);
    }

    for (i=0; i < NB_THREADS; i++) {
        threads[i]->Stop();
    }

    for (i=0; i < NB_THREADS; i++) {
        delete threads[i];
        delete thread_runners[i];
    }

    delete m_service;
    delete test_busreset;
    delete test_arm;
    delete test_arm2;

    printf("Bye...\n");

    return EXIT_SUCCESS;
}
