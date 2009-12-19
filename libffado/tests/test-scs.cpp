/*
 * Copyright (C) 2005-2009 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#include "debugmodule/debugmodule.h"

#include "devicemanager.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libutil/Configuration.h"
#include "libutil/SystemTimeSource.h"

#include "genericavc/stanton/scs.h"
using namespace GenericAVC;
using namespace GenericAVC::Stanton;

#include <argp.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>

#include <signal.h>

#include <alsa/asoundlib.h>
#define ALSA_SEQ_BUFF_SIZE 32
#define MIDI_TRANSMIT_BUFFER_SIZE 512

using namespace std;
using namespace Util;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_ARGS 1000

int run;
snd_seq_t *m_seq_handle = NULL;

static void sighandler(int sig)
{
    run = 0;

    // send an event to wake the iterator loop
    if(m_seq_handle) {
        snd_seq_nonblock(m_seq_handle, 1);
    }
}

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-scs 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-avccmd -- test program to test the Stanton SCS code.";
static char args_doc[] = "NODE_ID";
static struct argp_option options[] = {
    {"verbose",   'v', "LEVEL",     0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
    {"node",      'n', "NODE",      0,  "Set node" },
   { 0 }
};

struct arguments
{
    arguments()
        : nargs ( 0 )
        , verbose( DEBUG_LEVEL_NORMAL )
        , test( false )
        , port( -1 )
        , node( -1 )
        {
            args[0] = 0;
        }

    char* args[MAX_ARGS];
    int   nargs;
    int  verbose;
    bool  test;
    int   port;
    int   node;
} arguments;

// Parse a single option.
static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;

    char* tail;
    errno = 0;
    switch (key) {
    case 'v':
        arguments->verbose = strtol(arg, &tail, 0);
        break;
    case 't':
        arguments->test = true;
        break;
    case 'p':
        arguments->port = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case 'n':
        arguments->node = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            // Too many arguments.
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        arguments->nargs++;
        break;
    case ARGP_KEY_END:
        if(arguments->nargs<0) {
            printf("not enough arguments\n");
            return -1;
        }
        
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

class HSS1394AlsaSeqMidiBridge {
    class HSS1394UserDataHandler;

public:
    HSS1394AlsaSeqMidiBridge(snd_seq_t *seq_handle, GenericAVC::Stanton::ScsDevice &device)
    : m_seq_handle (seq_handle)
    , m_device( device )
    , m_name( "UNSPECIFIED" )
    , m_out_port_nr( -1 )
    , m_out_parser( NULL )
    , m_input_handler( NULL )
    {};

    virtual ~HSS1394AlsaSeqMidiBridge() {
        if(m_input_handler) {
            // remove the handler
            if(!m_device.m_hss1394handler->removeMessageHandler(GenericAVC::Stanton::ScsDevice::eMT_UserData, m_input_handler)) {
                debugError("Could not register input message handler\n");
            }
            delete m_input_handler;
            m_input_handler = NULL;
        }
        if(m_out_port_nr >= 0) {
            snd_seq_delete_simple_port(m_seq_handle, m_out_port_nr);
            m_out_port_nr = -1;
        }
        if(m_out_parser) {
            snd_midi_event_free(m_out_parser);
            m_out_parser = NULL;
        }
    };

    bool init() {
        // need local copy as the nickname can change
        m_name = m_device.getNickname();

        // create the output port
        m_out_port_nr = snd_seq_create_simple_port(m_seq_handle, m_name.c_str(),
                            SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                            SND_SEQ_PORT_TYPE_MIDI_GENERIC);
        if(m_out_port_nr < 0) {
            debugError("Could not create ALSA Sequencer port\n");
            return false;
        }

        // create the output message encoder
        if(snd_midi_event_new( ALSA_SEQ_BUFF_SIZE, &m_out_parser) < 0) {
            debugError("could not init output event encoder");
            return false;
        }

        // disable running-status
       snd_midi_event_no_status(m_out_parser, 1);

        // create a handler
        HSS1394UserDataHandler *m_input_handler = new HSS1394UserDataHandler(*this);
        if(m_input_handler == NULL) {
            debugError("Error creating handler.");
            return false;
        }
        if(!m_input_handler->init(m_name)) {
            debugError("Could not init input message handler\n");
            return false;
        }
        if(!m_device.m_hss1394handler->addMessageHandler(GenericAVC::Stanton::ScsDevice::eMT_UserData, m_input_handler)) {
            debugError("Could not register input message handler\n");
            return false;
        }
        return true;
    };

    bool sendAlsaSeqEvent(snd_seq_event_t *ev) {
        int bytes_to_send = 0;

        // decode it to the work buffer
        if((bytes_to_send = snd_midi_event_decode ( m_out_parser,
                m_work_buffer,
                MIDI_TRANSMIT_BUFFER_SIZE,
                ev)) < 0) { // failed to decode
                debugError(" Error decoding event for port %d (errcode=%d)", ev->dest.port, bytes_to_send);
                return false;
        } else {
            if(!m_device.writeHSS1394Message(GenericAVC::Stanton::ScsDevice::eMT_UserData,
                                                    m_work_buffer, bytes_to_send)) {
                debugError("Failed to send message\n");
                return false;
            }
        }
        return true;
    };

    int getAlsaSeqOutputPortNumber() {
        return m_out_port_nr;
    }
    int getAlsaSeqInputPortNumber() {
        if (m_input_handler) {
            return m_input_handler->getAlsaSeqPortNumber();
        } else {
            debugError("input handler not initialized yet");
            return -1;
        }
    }

    void setVerboseLevel(int i) {
        setDebugLevel(i);
    };

private:
    snd_seq_t *m_seq_handle;
    GenericAVC::Stanton::ScsDevice &m_device;
    std::string m_name;
    int m_out_port_nr;
    snd_midi_event_t *m_out_parser;

    HSS1394UserDataHandler *m_input_handler;

    unsigned char m_work_buffer[MIDI_TRANSMIT_BUFFER_SIZE];

    DECLARE_DEBUG_MODULE;

private: // the class that handles the async messages from the HSS1394 node
    class HSS1394UserDataHandler : public GenericAVC::Stanton::ScsDevice::HSS1394Handler::MessageFunctor {
    public:
        HSS1394UserDataHandler(HSS1394AlsaSeqMidiBridge &parent)
        : m_parent( parent )
        , m_ready(false)
        , m_seq_port_nr( -1 )
        , m_parser( NULL )
        , m_debugModule(parent.m_debugModule)
        {};
    
        virtual ~HSS1394UserDataHandler() {
            if(m_seq_port_nr >= 0) {
                snd_seq_delete_simple_port(m_parent.m_seq_handle, m_seq_port_nr);
                m_seq_port_nr = -1;
            }
            if(m_parser) {
                snd_midi_event_free(m_parser);
                m_parser = NULL;
            }
            m_ready = false;
        }
    
        bool init(std::string name) {
            m_seq_port_nr = snd_seq_create_simple_port(m_parent.m_seq_handle, name.c_str(),
                                SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
                                SND_SEQ_PORT_TYPE_MIDI_GENERIC);
            if(m_seq_port_nr < 0) {
                debugError("Could not create ALSA Sequencer port\n");
                return false;
            }
    
            if(snd_midi_event_new( ALSA_SEQ_BUFF_SIZE, &m_parser) < 0) {
                debugError("could not init parser");
                return false;
            }
            m_ready = true;
            return m_ready;
        }

        int getAlsaSeqPortNumber() {
            return m_seq_port_nr;
        }

        virtual void operator() (byte_t *buff, size_t len) {
            if (m_ready) {
                debugOutput(DEBUG_LEVEL_NORMAL, "got message len %zd\n", len);
    
                for (size_t s=0; s < len; s++) {
                    byte_t *byte = (buff+s);
                    snd_seq_event_t ev;
                    if ((snd_midi_event_encode_byte(m_parser, (*byte) & 0xFF, &ev)) > 0) {
                            // a midi message is complete, send it out to ALSA
                            snd_seq_ev_set_subs(&ev);
                            snd_seq_ev_set_direct(&ev);
                            snd_seq_ev_set_source(&ev, m_seq_port_nr);
                            snd_seq_event_output_direct(m_parent.m_seq_handle, &ev);
                    }
                }
            } else {
                debugError("Not ready\n");
            }
        };
    private:
        HSS1394AlsaSeqMidiBridge &m_parent;
        bool m_ready;
        int m_seq_port_nr;
        snd_midi_event_t *m_parser;
        DECLARE_DEBUG_MODULE_REFERENCE;
    };
};
IMPL_DEBUG_MODULE( HSS1394AlsaSeqMidiBridge, HSS1394AlsaSeqMidiBridge, DEBUG_LEVEL_NORMAL );

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{
    // arg parsing
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        printMessage("Could not parse command line\n" );
        exit(-1);
    }
    errno = 0;

    run=1;

    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    DeviceManager *m_deviceManager = new DeviceManager();
    if ( !m_deviceManager ) {
        printMessage("Could not allocate device manager\n" );
        return -1;
    }

    if ( arguments.verbose ) {
        setDebugLevel(arguments.verbose);
        m_deviceManager->setVerboseLevel(arguments.verbose);
    }

    if ( !m_deviceManager->initialize() ) {
        printMessage("Could not initialize device manager\n" );
        delete m_deviceManager;
        return -1;
    }

    char s[1024];
    if(arguments.port > -1 && arguments.node > -1) {
        snprintf(s, 1024, "hw:%d,%d", arguments.port, arguments.node);
        if ( !m_deviceManager->addSpecString(s) ) { 
            printMessage("Could not add spec string %s to device manager\n", s );
            delete m_deviceManager;
            return -1;
        }
    } else if (arguments.port > -1) {
        snprintf(s, 1024, "hw:%d", arguments.port);
        if ( !m_deviceManager->addSpecString(s) ) { 
            printMessage("Could not add spec string %s to device manager\n", s );
            delete m_deviceManager;
            return -1;
        }
    }

    if ( !m_deviceManager->discover(false) ) {
        printMessage("Could not discover devices\n" );
        delete m_deviceManager;
        return -1;
    }

    // loop over all discovered devices and extract the SCS devices
    int nb_devices = m_deviceManager->getAvDeviceCount();
    if(nb_devices == 0) {
        printMessage("No devices found\n");
        delete m_deviceManager;
        return -1;
    }

    typedef std::vector<ScsDevice*> ScsDeviceVector;
    typedef std::vector<ScsDevice*>::iterator ScsDeviceVectorIterator;

    ScsDeviceVector scsDevices;
    for(int i=0; i<nb_devices; i++) {
        FFADODevice *device = m_deviceManager->getAvDeviceByIndex(i);
        GenericAVC::Stanton::ScsDevice* scsDevice = dynamic_cast<GenericAVC::Stanton::ScsDevice*>(device);
        if(scsDevice == NULL) {
            printMessage("Device %d (GUID: %s) is not a Stanton SCS device\n", i, device->getConfigRom().getGuidString().c_str() );
        } else {
            printMessage("Device %d (GUID: %s) is a Stanton SCS device\n", i, device->getConfigRom().getGuidString().c_str() );
            scsDevices.push_back(scsDevice);
        }
    }

    if(scsDevices.size() == 0) {
        printMessage("No SCS devices found\n");
        delete m_deviceManager;
        return -1;
    }

    // open the alsa sequencer
    if (snd_seq_open(&m_seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        debugError("Error opening ALSA sequencer.");
        delete m_deviceManager;
        return -1;
    }

    snd_seq_set_client_name(m_seq_handle, "SCS MIDI");

    // this maps the alsa sequencer ports to the corresponding bridges
    typedef std::map<int, HSS1394AlsaSeqMidiBridge*> BridgeMap;
    typedef std::map<int, HSS1394AlsaSeqMidiBridge*>::iterator BridgeMapIterator;
    BridgeMap seqport2bridgemap;

    for ( ScsDeviceVectorIterator it = scsDevices.begin();
          it != scsDevices.end();
          ++it )
    {
        HSS1394AlsaSeqMidiBridge *bridge = new HSS1394AlsaSeqMidiBridge(m_seq_handle, **it);
        if(bridge == NULL) {
            debugError("Could not allocate HSS1394 <=> ALSA bridge\n");
            delete m_deviceManager;
            return -1;
        }
        bridge->setVerboseLevel(arguments.verbose);
        if(!bridge->init()) {
            debugError("Could not init HSS1394 <=> ALSA bridge\n");
            delete m_deviceManager;
            return -1;
        }
        int portNumber = bridge->getAlsaSeqOutputPortNumber();
        #ifdef DEBUG
        if(portNumber < -1) {
            debugError("BUG: port should be >= 0 after init\n");
        }
        #endif

        BridgeMapIterator it = seqport2bridgemap.find(portNumber);
        if(it == seqport2bridgemap.end()) {
            seqport2bridgemap[portNumber] = bridge;
        } else {
            debugError("BUG: port already present in bridge map, duplicate port.\n");
            delete bridge;
        }
    }

    // enter a wait loop
    printMessage(" >>> Entering wait loop, use CTRL-C to exit... \n" );
    while(run) {
        snd_seq_event_t *ev;
        int err = 0;

        // get next event, if one is present, blocks until an event is received
        err = snd_seq_event_input(m_seq_handle, &ev);
        if(err > 0 && ev) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Got event...\n");
            if (ev->source.client == SND_SEQ_CLIENT_SYSTEM)
                    continue;

            // figure out what bridge this is intended for
            BridgeMapIterator it = seqport2bridgemap.find(ev->dest.port);
            if(it != seqport2bridgemap.end()) {
                HSS1394AlsaSeqMidiBridge *bridge = (*it).second;
                if(bridge) {
                    if(!bridge->sendAlsaSeqEvent(ev)) {
                        debugError("Failed to send event to HSS1394 node\n");
                    }
                } else {
                    debugError("Bogus bridge in seqport2bridgemap\n");
                }
            } else {
                debugWarning("Received message for unknown port\n");
            }

        } else {
            switch(err) {
                case -EAGAIN:
                    debugOutput(DEBUG_LEVEL_VERBOSE, "no events in ALSA-SEQ FIFO\n");
                    break;
                case -ENOSPC:
                    debugWarning("ALSA-SEQ FIFO overrun, events dropped!\n");
                    break;
                default:
                    debugError("Failed to receive ALSA-SEQ event (%d)\n", err);
            }
        }
    }
    printMessage(" <<< Exit wait loop... \n" );

    // cleanup
    for ( BridgeMapIterator it = seqport2bridgemap.begin();
          it != seqport2bridgemap.end();
          ++it )
    {
        HSS1394AlsaSeqMidiBridge *bridge = (*it).second;
        delete bridge;
    }
    seqport2bridgemap.clear();

    snd_seq_close(m_seq_handle);
    delete m_deviceManager;

    printMessage("Bye... \n" );
    return 0;
}

