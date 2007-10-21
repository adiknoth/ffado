/*
 * Copyright (C) 2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * FFADO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FFADO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFADO; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 *
 */

#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#include "debugmodule/debugmodule.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libutil/cmd_serialize.h"
#include "libavc/general/avc_generic.h"

#include "fireworks/efc/efc_avc_cmd.h"
#include "fireworks/efc/efc_cmds_mixer.h"
#include "fireworks/efc/efc_cmds_monitor.h"
#include "fireworks/efc/efc_cmds_hardware.h"
using namespace FireWorks;

#include <argp.h>
#include <stdlib.h>
#include <iostream>

using namespace std;
using namespace AVC;
using namespace Util;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_ARGS 1000

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-echomixer 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-avccmd -- test program to examine the echo audio mixer.";
static char args_doc[] = "NODE_ID";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
    {"node",      'n', "NODE",      0,  "Set node" },
   { 0 }
};

struct arguments
{
    arguments()
        : nargs ( 0 )
        , verbose( false )
        , test( false )
        , port( 0 )
        {
            args[0] = 0;
        }

    char* args[MAX_ARGS];
    int   nargs;
    bool  verbose;
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
    switch (key) {
    case 'v':
        arguments->verbose = true;
        break;
    case 't':
        arguments->test = true;
        break;
    case 'p':
        errno = 0;
        arguments->port = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case 'n':
        errno = 0;
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

bool doEfcOverAVC(Ieee1394Service& m_1394Service, int node, EfcCmd &c) 
{
    EfcOverAVCCmd cmd( m_1394Service );
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    cmd.setNodeId( node );
    cmd.setSubunitType( AVC::eST_Unit  );
    cmd.setSubunitId( 0xff );

    cmd.setVerbose( DEBUG_LEVEL_NORMAL );

    cmd.m_cmd = &c;

    if ( !cmd.fire()) {
        debugError( "EfcOverAVCCmd command failed\n" );
        c.showEfcCmd();
        return false;
    }

    if (   c.m_header.retval != EfcCmd::eERV_Ok
        && c.m_header.retval != EfcCmd::eERV_FlashBusy) {
        debugError( "EFC command failed\n" );
        c.showEfcCmd();
        return false;
    }

    return true;
}

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{
    // arg parsing
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        exit(-1);
    }
    errno = 0;

    Ieee1394Service *m_1394Service = new Ieee1394Service();
    if ( !m_1394Service ) {
        debugFatal( "Could not create Ieee1349Service object\n" );
        return false;
    }

    if ( !m_1394Service->initialize( arguments.port ) ) {
        debugFatal( "Could not initialize Ieee1349Service object\n" );
        delete m_1394Service;
        m_1394Service = 0;
        return false;
    }

    EfcHardwareInfoCmd  m_HwInfo;
    
    if (!doEfcOverAVC(*m_1394Service, arguments.node, m_HwInfo)) {
        debugFatal("Could not obtain FireWorks device info\n");
        return false;
    }
    m_HwInfo.showEfcCmd();

    unsigned int ch=0;
    
    uint32_t pbk_vol[m_HwInfo.m_nb_1394_playback_channels][5];
    uint32_t rec_vol[m_HwInfo.m_nb_1394_record_channels][5];
    uint32_t out_vol[m_HwInfo.m_nb_phys_audio_out][5];
    uint32_t in_vol[m_HwInfo.m_nb_phys_audio_in][5];
    
    memset(pbk_vol, 0, sizeof(uint32_t) * 5 * m_HwInfo.m_nb_1394_playback_channels);
    memset(rec_vol, 0, sizeof(uint32_t) * 5 * m_HwInfo.m_nb_1394_record_channels);
    memset(out_vol, 0, sizeof(uint32_t) * 5 * m_HwInfo.m_nb_phys_audio_out);
    memset(in_vol, 0, sizeof(uint32_t) * 5 * m_HwInfo.m_nb_phys_audio_in);
    
    enum eMixerTarget t=eMT_PlaybackMix;
    enum eMixerCommand c = eMC_Gain;
    enum eCmdType type = eCT_Get;
    EfcGenericMixerCmd cmd(t,c);
    cmd.setType(type);

#define DO_PLAYBACK_MIX
// #define DO_RECORD_MIX
#define DO_PHYS_OUT_MIX
// #define DO_PHYS_IN_MIX

#ifdef DO_PLAYBACK_MIX
    cmd.setTarget(eMT_PlaybackMix);
    for (ch=0;ch<m_HwInfo.m_nb_1394_playback_channels;ch++) {
//     for (ch=0;ch<1;ch++) {
        {
            cmd.setCommand(eMC_Gain);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            pbk_vol[ch][0]=cmd.m_value;
        }
//         {
//             cmd.setCommand(eMC_Solo);
//             cmd.m_channel=ch;
//             if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
//                 debugFatal("Cmd failed\n");
//             }
//             pbk_vol[ch][1]=cmd.m_value;
//         }
        {
            cmd.setCommand(eMC_Mute);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            pbk_vol[ch][2]=cmd.m_value;
        }
//         {
//             cmd.setCommand(eMC_Pan);
//             cmd.m_channel=ch;
//             if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
//                 debugFatal("Cmd failed\n");
//             }
//             pbk_vol[ch][3]=cmd.m_value;
//         }
//         {
//             cmd.setCommand(eMC_Nominal);
//             cmd.m_channel=ch;
//             if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
//                 debugFatal("Cmd failed\n");
//             }
//             pbk_vol[ch][4]=cmd.m_value;
//         }
    }
#endif

#ifdef DO_RECORD_MIX
    cmd.setTarget(eMT_RecordMix);
    for (ch=0;ch<m_HwInfo.m_nb_1394_record_channels;ch++) {
//     for (ch=0;ch<1;ch++) {
        {
            cmd.setCommand(eMC_Gain);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            rec_vol[ch][0]=cmd.m_value;
        }
        {
            cmd.setCommand(eMC_Solo);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            rec_vol[ch][1]=cmd.m_value;
        }
        {
            cmd.setCommand(eMC_Mute);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            rec_vol[ch][2]=cmd.m_value;
        }
        {
            cmd.setCommand(eMC_Pan);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            rec_vol[ch][3]=cmd.m_value;
        }
        {
            cmd.setCommand(eMC_Nominal);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            rec_vol[ch][4]=cmd.m_value;
        }
    }
#endif

#ifdef DO_PHYS_OUT_MIX
    cmd.setTarget(eMT_PhysicalOutputMix);
    for (ch=0;ch<m_HwInfo.m_nb_phys_audio_out;ch++) {
//     for (ch=0;ch<1;ch++) {
        {
            cmd.setCommand(eMC_Gain);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            out_vol[ch][0]=cmd.m_value;
        }
//         {
//             cmd.setCommand(eMC_Solo);
//             cmd.m_channel=ch;
//             if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
//                 debugFatal("Cmd failed\n");
//             }
//             out_vol[ch][1]=cmd.m_value;
//         }
        {
            cmd.setCommand(eMC_Mute);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            out_vol[ch][2]=cmd.m_value;
        }
//         {
//             cmd.setCommand(eMC_Pan);
//             cmd.m_channel=ch;
//             if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
//                 debugFatal("Cmd failed\n");
//             }
//             out_vol[ch][3]=cmd.m_value;
//         }
        {
            cmd.setCommand(eMC_Nominal);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            out_vol[ch][4]=cmd.m_value;
        }
    }
#endif

#ifdef DO_PHYS_IN_MIX
    cmd.setTarget(eMT_PhysicalInputMix);
    for (ch=0;ch<m_HwInfo.m_nb_phys_audio_in;ch++) {
//     for (ch=0;ch<1;ch++) {
        {
            cmd.setCommand(eMC_Gain);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            in_vol[ch][0]=cmd.m_value;
        }
        {
            cmd.setCommand(eMC_Solo);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            in_vol[ch][1]=cmd.m_value;
        }
        {
            cmd.setCommand(eMC_Mute);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            in_vol[ch][2]=cmd.m_value;
        }
        {
            cmd.setCommand(eMC_Pan);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            in_vol[ch][3]=cmd.m_value;
        }
        {
            cmd.setCommand(eMC_Nominal);
            cmd.m_channel=ch;
            if (!doEfcOverAVC(*m_1394Service, arguments.node, cmd)) {
                debugFatal("Cmd failed\n");
            }
            in_vol[ch][4]=cmd.m_value;
        }
    }
#endif

    uint32_t monitor_gain[m_HwInfo.m_nb_phys_audio_in][m_HwInfo.m_nb_phys_audio_out];
    for (unsigned int ch_in=0;ch_in<m_HwInfo.m_nb_phys_audio_in;ch_in++) {
        for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
            {
                EfcGetMonitorGainCmd getCmd;
                getCmd.m_input=ch_in;
                getCmd.m_output=ch_out;
                if (!doEfcOverAVC(*m_1394Service, arguments.node, getCmd)) {
                    debugFatal("Cmd failed\n");
                }
                monitor_gain[ch_in][ch_out]=getCmd.m_value;
            }
        }
    }
    uint32_t monitor_pan[m_HwInfo.m_nb_phys_audio_in][m_HwInfo.m_nb_phys_audio_out];
    for (unsigned int ch_in=0;ch_in<m_HwInfo.m_nb_phys_audio_in;ch_in++) {
        for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
            {
                EfcGetMonitorPanCmd getCmd;
                getCmd.m_input=ch_in;
                getCmd.m_output=ch_out;
                if (!doEfcOverAVC(*m_1394Service, arguments.node, getCmd)) {
                    debugFatal("Cmd failed\n");
                }
                monitor_pan[ch_in][ch_out]=getCmd.m_value;
            }
        }
    }
    uint32_t monitor_mute[m_HwInfo.m_nb_phys_audio_in][m_HwInfo.m_nb_phys_audio_out];
    for (unsigned int ch_in=0;ch_in<m_HwInfo.m_nb_phys_audio_in;ch_in++) {
        for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
            {
                EfcGetMonitorMuteCmd getCmd;
                getCmd.m_input=ch_in;
                getCmd.m_output=ch_out;
                if (!doEfcOverAVC(*m_1394Service, arguments.node, getCmd)) {
                    debugFatal("Cmd failed\n");
                }
                monitor_mute[ch_in][ch_out]=getCmd.m_value;
            }
        }
    }

    uint32_t monitor_solo[m_HwInfo.m_nb_phys_audio_in][m_HwInfo.m_nb_phys_audio_out];
    for (unsigned int ch_in=0;ch_in<m_HwInfo.m_nb_phys_audio_in;ch_in++) {
        for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
            {
                EfcGetMonitorSoloCmd getCmd;
                getCmd.m_input=ch_in;
                getCmd.m_output=ch_out;
                if (!doEfcOverAVC(*m_1394Service, arguments.node, getCmd)) {
                    debugFatal("Cmd failed\n");
                }
                monitor_solo[ch_in][ch_out]=getCmd.m_value;
            }
        }
    }

    printf("Mixer state info\n");
    printf("================\n");
    printf("        %10s %10s %10s %10s %10s\n","GAIN","PAN","SOLO","MUTE","NOMINAL");
#ifdef DO_PLAYBACK_MIX
    printf("Playback mixer state:\n");
    for (ch=0;ch<m_HwInfo.m_nb_1394_playback_channels;ch++) {
        printf(" ch %2d: ", ch);
        int j;
        for (j=0;j<5;j++) {
            if (j==0 || j==2)
                printf("%10u ", pbk_vol[ch][j]);
            else
                printf("%10s ", "*");
        }
        printf("\n");
    }
#endif

#ifdef DO_RECORD_MIX
    printf("Record mixer state:\n");
    for (ch=0;ch<m_HwInfo.m_nb_1394_record_channels;ch++) {
        printf(" ch %2d: ", ch);
        int j;
        for (j=0;j<5;j++) {
            printf("%10u ", rec_vol[ch][j]);
        }
        printf("\n");
    }    
#endif

#ifdef DO_PHYS_OUT_MIX
    printf("Output mixer state:\n");
    for (ch=0;ch<m_HwInfo.m_nb_phys_audio_out;ch++) {
        printf(" ch %2d: ", ch);
        int j;
        for (j=0;j<5;j++) {
           if (j==0 || j==2 || j==4)
                printf("%10u ", out_vol[ch][j]);
            else
                printf("%10s ", "*");
        }
        printf("\n");
    }
#endif
    
#ifdef DO_PHYS_IN_MIX
    printf("Input mixer state:\n");
    for (ch=0;ch<m_HwInfo.m_nb_phys_audio_in;ch++) {
        printf(" ch %2d: ", ch);
        int j;
        for (j=0;j<5;j++) {
            printf("%10u ", in_vol[ch][j]);
        }
        printf("\n");
    }
#endif
    
    printf("\nMonitor state info\n");
    printf("==================\n");
    
    printf("GAIN  ");
    for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
        printf("      OUT%02u",ch_out);
    }
    printf("\n");
    
    for (unsigned int ch_in=0;ch_in<m_HwInfo.m_nb_phys_audio_in;ch_in++) {
        printf("ch %2u:", ch_in);
        for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
            printf(" %10u", monitor_gain[ch_in][ch_out]);
        }
        printf("\n");
    }
    printf("\n");
    
    printf("PAN   ");
    for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
        printf("      OUT%02u",ch_out);
    }
    printf("\n");
    
    for (unsigned int ch_in=0;ch_in<m_HwInfo.m_nb_phys_audio_in;ch_in++) {
        printf("ch %2u:", ch_in);
        for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
            printf(" %10u", monitor_pan[ch_in][ch_out]);
        }
        printf("\n");
    }
    printf("\n");

    printf("MUTE  ");
    for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
        printf("      OUT%02u",ch_out);
    }
    printf("\n");
    
    for (unsigned int ch_in=0;ch_in<m_HwInfo.m_nb_phys_audio_in;ch_in++) {
        printf("ch %2u:", ch_in);
        for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
            printf(" %10u", monitor_mute[ch_in][ch_out]);
        }
        printf("\n");
    }
    printf("\n");

    printf("SOLO  ");
    for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
        printf("      OUT%02u",ch_out);
    }
    printf("\n");
    
    for (unsigned int ch_in=0;ch_in<m_HwInfo.m_nb_phys_audio_in;ch_in++) {
        printf("ch %2u:", ch_in);
        for (unsigned int ch_out=0;ch_out<m_HwInfo.m_nb_phys_audio_out;ch_out++) {
            printf(" %10u", monitor_solo[ch_in][ch_out]);
        }
        printf("\n");
    }
    printf("\n");

    delete m_1394Service;

    return 0;
}

