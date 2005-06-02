/* sync_mode.cpp
 * Copyright (C) 2005 by Daniel Wagner
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

#include "avc_extended_stream_format.h"
#include "avc_plug_info.h"
#include "avc_signal_source.h"
#include "serialize.h"
#include "convert.h"

#include <argp.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "sync_mode 0.1";
const char *argp_program_bug_address = "<freebob-devel@lists.sf.net>";
static char doc[] = "sync_mode -- get/set sync mode for FreeBob";
static char args_doc[] = "NODE_ID";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,     "Produce verbose output" },
    {"test",      't', 0,           0,     "Do tests instead get/set action" },
    {"sync",      's', "SYNC_MODE", 0,     "Set sync mode" },
    {"port",      'p', "PORT",      0,     "Set port" },
   { 0 }
};

struct arguments
{
    arguments()
        : verbose( false )
        , test( false )
        , sync_mode_id( -1 )
        , port( 0 )
        {
            args[0] = 0;
        }

    char* args[1];
    bool  verbose;
    bool  test;
    int   sync_mode_id;
    int   port;
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
    case 's':
        arguments->sync_mode_id = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case 'p':
        errno = 0;
        arguments->port = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 1) {
            // Too many arguments.
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 1) {
            // Not enough arguments.
            argp_usage (state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

////////////////////////////////////////
// Test application
////////////////////////////////////////
bool
doTest( raw1394handle_t handle, int node_id )
{
    /*
    ExtendedStreamFormatCmd extendedStreamFormatCmd;
    UnitPlugAddress unitPlugAddress( 0x00,  0x00 );
    extendedStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                                         PlugAddress::ePD_Input,
                                                         unitPlugAddress ) );
    extendedStreamFormatCmd.setVerbose( arguments.verbose );
    extendedStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
    extendedStreamFormatCmd.fire( handle, node_id );

    PlugInfoCmd plugInfoCmd;
    plugInfoCmd.setVerbose( arguments.verbose );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    if ( plugInfoCmd.fire( handle,  node_id ) ) {
        CoutSerializer se;
        plugInfoCmd.serialize( se );
    }

    plugInfoCmd.setSubFunction( PlugInfoCmd::eSF_SerialBusAsynchonousPlug );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    if ( plugInfoCmd.fire( handle,  node_id ) ) {
        CoutSerializer se;
        plugInfoCmd.serialize( se );
    }
    */

    SignalSourceCmd signalSourceCmd;
    SignalUnitAddress dest;
    dest.m_plugId = 0x00;
    signalSourceCmd.setSignalDestination( dest );
    signalSourceCmd.setCommandType( AVCCommand::eCT_Status );
    signalSourceCmd.setVerbose( arguments.verbose );

    CoutSerializer se;
    signalSourceCmd.serialize( se );

    if ( signalSourceCmd.fire( handle, node_id ) ) {
        signalSourceCmd.serialize( se );
    }

    return true;
}

////////////////////////////////////////
// Main application
////////////////////////////////////////

struct PlugInfo {
    PlugInfo()
        : m_name( "" )
        , m_subunitType( AVCCommand::eST_Reserved )
        , m_subunitId( 0xff )
        , m_plugId( 0xff )
        {}
    PlugInfo( string name, AVCCommand::ESubunitType subunitType, byte_t subunitId, byte_t plugId )
        : m_name( name )
        , m_subunitType( subunitType )
        , m_subunitId( subunitId )
        , m_plugId( plugId )
        {}

    bool operator == ( const PlugInfo& rhs ) const
        {
            return ( ( m_subunitType == rhs.m_subunitType )
                     && ( m_subunitId == rhs.m_subunitId )
                     && ( m_plugId == rhs.m_plugId ) );
        }

    string m_name;
    AVCCommand::ESubunitType m_subunitType;
    byte_t       m_subunitId;
    byte_t       m_plugId;
};

ostream& operator << ( ostream& stream, PlugInfo& info )
{
    return stream << info.m_name
                  << ", subunitType " << info.m_subunitType
                  << ", subunitId " << ( int )info.m_subunitId
                  << ", plugId " << ( int )info.m_plugId;
}

struct SyncConnectionInfo {
    SyncConnectionInfo( string name, int id, PlugInfo sourcePlug, PlugInfo destinationPlug )
        : m_name( name )
        , m_id( id )
        , m_sourcePlug( sourcePlug )
        , m_destinationPlug( destinationPlug )
        {}

    string m_name;
    int    m_id;
    PlugInfo m_sourcePlug;
    PlugInfo m_destinationPlug;
};
typedef vector<SyncConnectionInfo> SyncConnectionInfos;



static
bool inquireConnection( raw1394handle_t handle,
                        int node_id,
                        SyncConnectionInfos& syncConnectionInfos,
                        const PlugInfo& sourcePlug,
                        const PlugInfo& destPlug,
                        const string connectionName )
{
    SignalSourceCmd signalSourceCmd;
    signalSourceCmd.setSubunitType( destPlug.m_subunitType );
    signalSourceCmd.setSubunitId( destPlug.m_subunitId );
    signalSourceCmd.setCommandType( AVCCommand::eCT_SpecificInquiry );
    signalSourceCmd.setVerbose( arguments.verbose );

    SignalSubunitAddress signalSubunitAddressSource;
    signalSubunitAddressSource.m_subunitType = sourcePlug.m_subunitType;
    signalSubunitAddressSource.m_subunitId = sourcePlug.m_subunitId;
    signalSubunitAddressSource.m_plugId = sourcePlug.m_plugId;
    signalSourceCmd.setSignalSource( signalSubunitAddressSource );

    SignalSubunitAddress signalSubunitAddressDestination;
    signalSubunitAddressDestination.m_subunitType = destPlug.m_subunitType;
    signalSubunitAddressDestination.m_subunitId = destPlug.m_subunitId;
    signalSubunitAddressDestination.m_plugId = destPlug.m_plugId;
    signalSourceCmd.setSignalDestination( signalSubunitAddressDestination );

    if ( signalSourceCmd.fire( handle,  node_id ) ) {
        usleep( 10000 );
        if ( signalSourceCmd.getResponse() == AVCCommand::eR_Implemented ) {
            syncConnectionInfos.push_back( SyncConnectionInfo( connectionName, syncConnectionInfos.size(), sourcePlug, destPlug ) );
        }
    } else {
        return false;
    }

    return true;
}


static SyncConnectionInfo*
getSyncConnectionInfo( SyncConnectionInfos& syncConnectionInfos,
                       PlugInfo& sourcePlug )
{
    SyncConnectionInfo* info = 0;
    for ( SyncConnectionInfos::iterator it = syncConnectionInfos.begin();
          it != syncConnectionInfos.end();
          ++it )
    {
        if ( it->m_sourcePlug == sourcePlug ) {
            info = &( *it );
        }

    }
    return info;
}

static bool
findSyncPlugs( raw1394handle_t handle, int node_id, PlugInfo& syncInputPlug, PlugInfo& syncOutputPlug )
{
    // First we have to find the music subunit sync input plug (address)
    PlugInfoCmd plugInfoCmd;
    plugInfoCmd.setVerbose( arguments.verbose );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    plugInfoCmd.setSubunitType( AVCCommand::eST_Music );
    plugInfoCmd.setSubunitId( 0x00 );

    // find input sync plug
    if ( plugInfoCmd.fire( handle,  node_id ) ) {
        usleep( 10000 );
        for ( int plugIdx = 0;
              plugIdx < plugInfoCmd.m_destinationPlugs;
              ++plugIdx )
        {
            ExtendedStreamFormatCmd extendedStreamFormatCmd;

            SubunitPlugAddress subunitPlugAddress( plugIdx );
            extendedStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
                                                                 PlugAddress::ePAM_Subunit,
                                                                 subunitPlugAddress ) );
            extendedStreamFormatCmd.setSubunitType( AVCCommand::eST_Music );
            extendedStreamFormatCmd.setSubunitId( 0x00 );
            extendedStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
            extendedStreamFormatCmd.setVerbose( arguments.verbose );

            if ( extendedStreamFormatCmd.fire( handle, node_id ) ) {
                usleep( 10000 );
                FormatInformation* formatInformation = extendedStreamFormatCmd.getFormatInformation();
                if ( formatInformation
                     && ( formatInformation->m_root   == FormatInformation::eFHR_AudioMusic )
                     && ( formatInformation->m_level1 == FormatInformation::eFHL1_AUDIOMUSIC_AM824 )
                     && ( formatInformation->m_level2 == FormatInformation::eFHL2_AM824_SYNC_STREAM ) )
                {
                    syncInputPlug.m_name = "sync input plug";
                    syncInputPlug.m_subunitType = AVCCommand::eST_Music;
                    syncInputPlug.m_subunitId = 0x00;
                    syncInputPlug.m_plugId = plugIdx;
                }
            }
        }

        // find output sync plug
        for ( int plugIdx = 0;
              plugIdx < plugInfoCmd.m_sourcePlugs;
              ++plugIdx )
        {
            ExtendedStreamFormatCmd extendedStreamFormatCmd;

            SubunitPlugAddress subunitPlugAddress( plugIdx );
            extendedStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Output,
                                                                 PlugAddress::ePAM_Subunit,
                                                                 subunitPlugAddress ) );
            extendedStreamFormatCmd.setSubunitType( AVCCommand::eST_Music );
            extendedStreamFormatCmd.setSubunitId( 0x00 );
            extendedStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
            extendedStreamFormatCmd.setVerbose( arguments.verbose );

            if ( extendedStreamFormatCmd.fire( handle, node_id ) ) {
                usleep( 10000 );
                FormatInformation* formatInformation = extendedStreamFormatCmd.getFormatInformation();
                if ( formatInformation
                     && ( formatInformation->m_root   == FormatInformation::eFHR_AudioMusic )
                     && ( formatInformation->m_level1 == FormatInformation::eFHL1_AUDIOMUSIC_AM824 )
                     && ( formatInformation->m_level2 == FormatInformation::eFHL2_AM824_SYNC_STREAM ) )
                {
                    syncOutputPlug.m_name = "sync output plug";
                    syncOutputPlug.m_subunitType = AVCCommand::eST_Music;
                    syncOutputPlug.m_subunitId = 0x00;
                    syncOutputPlug.m_plugId = plugIdx;
                }
            }
        }
    }
    if ( arguments.verbose ) {
        cout << syncInputPlug << endl;
        cout << syncOutputPlug << endl;
    }

    return true;
}

static
bool
findSupportedConnections( raw1394handle_t handle,
                          int node_id,
                          PlugInfo& syncInputPlug,
                          PlugInfo& syncOutputPlug,
                          SyncConnectionInfos& syncConnectionInfos )
{
    // - Music subunit sync output plug = internal sync (CSP)
    inquireConnection( handle,
                       node_id,
                       syncConnectionInfos,
                       syncOutputPlug,
                       syncInputPlug,
                       "internal (CSP)" );

    // - Unit input plug 0 = SYT match
    PlugInfo iPCR0( "iPCR[0]", AVCCommand::eST_Unit, 0xff, 0x00 );
    inquireConnection( handle,
                       node_id,
                       syncConnectionInfos,
                       iPCR0,
                       syncInputPlug,
                       "SYT match" );

    // - Unit input plut 1 = sync stream
    PlugInfo iPCR1( "iPCR[1]", AVCCommand::eST_Unit, 0xff, 0x01 );
    inquireConnection( handle,
                       node_id,
                       syncConnectionInfos,
                       iPCR1,
                       syncInputPlug,
                       "sync stream" );

    // Find out how many external input plugs exits.
    // Every one of them is possible a sync source
    {
        PlugInfoCmd plugInfoCmd;
        plugInfoCmd.setVerbose( arguments.verbose );
        plugInfoCmd.setCommandType( AVCCommand::eCT_Status );

        if ( plugInfoCmd.fire( handle, node_id ) ) {
            usleep( 10000 ); // Don't overload device with requests
            for ( int plugIdx = 0;
                  plugIdx < plugInfoCmd.m_externalInputPlugs;
                  ++plugIdx )
            {
                // If we want to know what plug it is we have to
                // parse the descriptor which is a overkill ATM.
                // Let's keep it simple.
                string plugName = "external plug " + stringify( plugIdx );
                PlugInfo externalPlug( plugName,
                                       AVCCommand::eST_Unit,
                                       0xff,
                                       0x80 + plugIdx );
                inquireConnection( handle,
                                   node_id,
                                   syncConnectionInfos,
                                   externalPlug,
                                   syncInputPlug,
                                   plugName);
            }
        }
    }
    return true;
}

static
bool
setCurrentActive( raw1394handle_t handle,
                  int node_id,
                  PlugInfo& syncInputPlug,
                  SyncConnectionInfos& syncConnectionInfos,
                  int id )
{
    SyncConnectionInfo* info = 0;

    for ( SyncConnectionInfos::iterator it = syncConnectionInfos.begin();
          it != syncConnectionInfos.end();
          ++it )
    {
        if ( it->m_id == id ) {
            info = &( *it );
            break;
        }
    }

    bool result = false;
    if ( info ) {
        SignalSourceCmd signalSourceCmd;
        signalSourceCmd.setSubunitType( syncInputPlug.m_subunitType );
        signalSourceCmd.setSubunitId( syncInputPlug.m_subunitId );
        signalSourceCmd.setCommandType( AVCCommand::eCT_Control );
        signalSourceCmd.setVerbose( arguments.verbose );

        if ( info->m_sourcePlug.m_subunitType == AVCCommand::eST_Unit ) {
            SignalUnitAddress signalUnitAddress;
            signalUnitAddress.m_plugId = info->m_sourcePlug.m_plugId;
            signalSourceCmd.setSignalSource( signalUnitAddress );
        } else {
            SignalSubunitAddress signalSubunitAddress;
            signalSubunitAddress.m_subunitType = info->m_sourcePlug.m_subunitType;
            signalSubunitAddress.m_subunitId = info->m_sourcePlug.m_subunitId;
            signalSubunitAddress.m_plugId = info->m_sourcePlug.m_plugId;
            signalSourceCmd.setSignalSource( signalSubunitAddress );
        }

        SignalSubunitAddress signalSubunitAddressDestination;
        signalSubunitAddressDestination.m_subunitType = syncInputPlug.m_subunitType;
        signalSubunitAddressDestination.m_subunitId = syncInputPlug.m_subunitId;
        signalSubunitAddressDestination.m_plugId = syncInputPlug.m_plugId;
        signalSourceCmd.setSignalDestination( signalSubunitAddressDestination );

        if ( signalSourceCmd.fire( handle, node_id ) ) {
            usleep( 10000 );
            switch ( signalSourceCmd.getResponse() )
            {
            case AVCCommand::eR_Accepted:
                cout << endl << " -> new sync mode accepted" << endl;
                result = true;
                break;
            case AVCCommand::eR_Rejected:
                cout << endl << " -> new sync mode rejected" << endl;
                result = true;
                break;
            default:
                cerr << "unexpected response" << endl;
                result = false;
            }
        } else {
            cerr << "no response" << endl;
            result = false;
        }
    } else {
        cerr << "no sync method found for " << id << endl;
        result = false;
    }

    return result;
}

static
bool printSupportedModes( raw1394handle_t handle,
                          int node_id,
                          SyncConnectionInfos& syncConnectionInfos )
{
    cout << "Supported sync modes:" << endl;
    for ( SyncConnectionInfos::iterator it = syncConnectionInfos.begin();
          it != syncConnectionInfos.end();
          ++it )
    {
        cout << " (" << it->m_id << ") " << it->m_name << endl;
    }
    return true;
}

static bool
printCurrentActive( raw1394handle_t handle,
                    int node_id,
                    SyncConnectionInfos& syncConnectionInfos,
                    PlugInfo& syncInputPlug )
{
    SignalSourceCmd signalSourceCmd;
    signalSourceCmd.setSubunitType( syncInputPlug.m_subunitType );
    signalSourceCmd.setSubunitId( syncInputPlug.m_subunitId );
    signalSourceCmd.setCommandType( AVCCommand::eCT_Status );
    signalSourceCmd.setVerbose( arguments.verbose );

    SignalSubunitAddress signalSubunitAddressSource;
    signalSubunitAddressSource.m_subunitType = 0xff;
    signalSubunitAddressSource.m_subunitId = 0xff;
    signalSubunitAddressSource.m_plugId = 0xff;
    signalSourceCmd.setSignalSource( signalSubunitAddressSource );

    SignalSubunitAddress signalSubunitAddressDestination;
    signalSubunitAddressDestination.m_subunitType = syncInputPlug.m_subunitType;
    signalSubunitAddressDestination.m_subunitId = syncInputPlug.m_subunitId;
    signalSubunitAddressDestination.m_plugId = syncInputPlug.m_plugId;
    signalSourceCmd.setSignalDestination( signalSubunitAddressDestination );

    if ( signalSourceCmd.fire( handle, node_id ) ) {
        usleep( 10000 );
        if ( signalSourceCmd.getResponse() == AVCCommand::eR_Implemented ) {
            cout << endl << "Currently activated mode:" << endl;

            SignalUnitAddress* signalUnitAddress = 0;
            try {
                signalUnitAddress = dynamic_cast<SignalUnitAddress*>( signalSourceCmd.getSignalSource() );
            } catch ( bad_cast ) { }

            SignalSubunitAddress* signalSubnitAddress = 0;
            try {
                signalSubnitAddress = dynamic_cast<SignalSubunitAddress*>( signalSourceCmd.getSignalSource() );
            } catch ( bad_cast ) { }

            PlugInfo sourcePlug;
            if ( signalUnitAddress ) {
                sourcePlug.m_subunitType = AVCCommand::eST_Unit;
                sourcePlug.m_subunitId = 0x00;
                sourcePlug.m_plugId = signalUnitAddress->m_plugId;
            } else if ( signalSubnitAddress ) {
                sourcePlug.m_subunitType = static_cast<AVCCommand::ESubunitType>( signalSubnitAddress->m_subunitType );
                sourcePlug.m_subunitId = signalSubnitAddress->m_subunitId;
                sourcePlug.m_plugId = signalSubnitAddress->m_plugId;
            }

            SyncConnectionInfo* info = getSyncConnectionInfo( syncConnectionInfos, sourcePlug );
            if ( info ) {
                cout << " (" << info->m_id << ") " << info->m_name << endl;
            }
        }
    }

    return true;
}

bool
doApp( raw1394handle_t handle, int node_id, int sync_mode_id )
{
    // Following sync sources always exists:
    // - Music subunit sync output plug = internal sync (CSP)
    // - Unit input plug 0 = SYT match
    // - Unit input plut 1 = sync stream

    //
    // Following sync sources are device specific:
    // - All unit external input plugs which have a sync information (WS, SPDIF, ...)

    PlugInfo syncInputPlug;
    PlugInfo syncOutputPlug;
    SyncConnectionInfos syncConnectionInfos;

    findSyncPlugs( handle, node_id, syncInputPlug, syncOutputPlug );
    findSupportedConnections( handle, node_id, syncInputPlug, syncOutputPlug, syncConnectionInfos );

    printSupportedModes( handle, node_id, syncConnectionInfos );
    if ( sync_mode_id != -1 ) {
        setCurrentActive( handle, node_id, syncInputPlug, syncConnectionInfos, sync_mode_id );
    }
    printCurrentActive( handle, node_id, syncConnectionInfos, syncInputPlug );

    return true;
}

int
main(int argc, char **argv)
{
    // arg parsing
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    errno = 0;
    char* tail;
    int node_id = strtol(arguments.args[0], &tail, 0);
    if (errno) {
	perror("argument parsing failed:");
	return -1;
    }

    // create raw1394handle
    raw1394handle_t handle;
    handle = raw1394_new_handle ();
    if (!handle) {
	if (!errno) {
	    perror("lib1394raw not compatable\n");
	} else {
	    fprintf(stderr, "Could not get 1394 handle");
	    fprintf(stderr, "Is ieee1394, driver, and raw1394 loaded?\n");
	}
	return -1;
    }

    if (raw1394_set_port(handle, arguments.port) < 0) {
	fprintf(stderr, "Could not set port");
	raw1394_destroy_handle (handle);
	return -1;
    }

    if ( arguments.test ) {
        doTest( handle, node_id );
    } else {
        doApp( handle, node_id, arguments.sync_mode_id );
    }

    raw1394_destroy_handle( handle );

    return 0;
}
