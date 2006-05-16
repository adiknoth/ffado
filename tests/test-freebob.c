/* test-freebob.c
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

#include <config.h>

#include "libfreebob/freebob.h"
#include "libfreebob/freebob_bounce.h"

#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

// Program documentation.
static char doc[] = "FreeBob -- a driver for BeBob devices (test application)\n\n"
                    "OPERATION: discover\n"
                    "           odisocver\n"
                    "           setsamplerate\n"
                    "           xmldump\n"
                    "           testmultidevicediscovery\n"
                    "           streamformats\n";

// A description of the arguments we accept.
static char args_doc[] = "OPERATION";

struct arguments
{
    short silent;
    short verbose;
    int   port;
    int   node_id;
    int   node_id_set;
    int   time;
    char* args[2];  
};

// The options we understand.
static struct argp_option options[] = {
    {"quiet",    'q',       0,    0,  "Don't produce any output" },
    {"silent",   's',       0,    OPTION_ALIAS },

    {"verbose",  'v', "level",    0,  "Produce verbose output" },


    {"node",     'n',    "id",    0,  "Node to use" },
    {"port",     'p',    "nr",    0,  "IEEE1394 Port to use" },
    {"time",     't',    "time",  0,  "Workaround: sleep <time> usec after AVC command\n" },
    { 0 }
};

//-------------------------------------------------------------

// Parse a single option.
static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;
    char* tail;

    switch (key) {
	case 'q': case 's':
	    arguments->silent = 1;
	    break;
	case 'v':
	    if (arg) {
		arguments->verbose = strtol( arg, &tail, 0 );
		if ( errno ) {
		    fprintf( stderr,  "Could not parse 'verbose' argument\n" );
		    return ARGP_ERR_UNKNOWN;
		}
	    }
	    break;
	case 't':
	    if (arg) {
		arguments->time = strtol( arg, &tail, 0 );
		if ( errno ) {
		    fprintf( stderr,  "Could not parse 'time' argument\n" );
		    return ARGP_ERR_UNKNOWN;
		}
	    }
	    break;
	case 'p':
	    if (arg) {
		arguments->port = strtol( arg, &tail, 0 );
		if ( errno ) {
		    fprintf( stderr,  "Could not parse 'port' argument\n" );
		    return ARGP_ERR_UNKNOWN;
		}
	    } else {
		if ( errno ) {
		    fprintf( stderr, "Could not parse 'port' argumen\n" );
		    return ARGP_ERR_UNKNOWN;
		}
	    }
	    break;
	case 'n':
	    if (arg) {
		arguments->node_id = strtol( arg, &tail, 0 );
		if ( errno ) {
		    fprintf( stderr,  "Could not parse 'node' argument\n" );
		    return ARGP_ERR_UNKNOWN;
		}
		arguments->node_id_set=1;
	    } else {
		if ( errno ) {
		    fprintf( stderr, "Could not parse 'node' argumen\n" );
		    return ARGP_ERR_UNKNOWN;
		}
	    }
	    break;
	case ARGP_KEY_ARG:
	    if (state->arg_num >= 2) {
		// Too many arguments.
		argp_usage( state );
	    }
	    arguments->args[state->arg_num] = arg;
	    break;
	case ARGP_KEY_END:
	    if (state->arg_num < 1) {
		// Not enough arguments.
		argp_usage( state );
	    }
	    break;
	default:
	    return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// Our argp parser.
static struct argp argp = { options, parse_opt, args_doc, doc };

int
main( int argc, char **argv )
{
    struct arguments arguments;

    // Default values.
    arguments.silent      = 0;
    arguments.verbose     = 0;
    arguments.port        = 0;
    arguments.node_id     = 0;
    arguments.node_id_set = 0; // if we don't specify a node, discover all
    arguments.time        = 0;
    arguments.args[0]     = "";
    arguments.args[1]     = "";

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        return -1;
    }

    printf("verbose level = %d\n", arguments.verbose);

    printf( "Using freebob library version: %s\n\n", freebob_get_version() );

    freebob_sleep_after_avc_command( arguments.time );

    if ( strcmp( arguments.args[0], "discover" ) == 0 ) {
	freebob_handle_t fb_handle = freebob_new_handle( arguments.port );
	if ( !fb_handle ) {
	    fprintf( stderr, "Could not create freebob handle\n" );
	    return -1;
	}
		
	if ( freebob_discover_devices( fb_handle, arguments.verbose ) != 0 ) {
	    fprintf( stderr, "Could not discover devices\n" );
	    freebob_destroy_handle( fb_handle );
	    return -1;
	}
	
	freebob_connection_info_t* test_info;
	
	if(arguments.node_id_set) {
	    printf("  port = %d, node_id = %d\n", arguments.port, arguments.node_id);
	    test_info = freebob_get_connection_info( fb_handle, 
						     arguments.node_id, 
						     0 );
	    freebob_print_connection_info( test_info );
	    freebob_free_connection_info( test_info );
		
	    printf("\n");
		
	    test_info = freebob_get_connection_info( fb_handle, 
						     arguments.node_id,
						     1 );
	    freebob_print_connection_info( test_info );
	    freebob_free_connection_info( test_info );
	} else {
	    int i=0;
			
	    int devices_on_bus = freebob_get_nb_devices_on_bus(fb_handle);
	    printf("  port = %d, devices_on_bus = %d\n", arguments.port, devices_on_bus);
			
	    for(i=0;i<devices_on_bus;i++) {
		int node_id=freebob_get_device_node_id(fb_handle, i);
		printf("  get info for device = %d, node = %d\n", i, node_id);
				
		test_info = freebob_get_connection_info( fb_handle, 
							 node_id, 
							 0 );
		freebob_print_connection_info( test_info );
		freebob_free_connection_info( test_info );
			
		printf("\n");
			
		test_info = freebob_get_connection_info( fb_handle, 
							 node_id,
							 1 );
		freebob_print_connection_info( test_info );
		freebob_free_connection_info( test_info );
	    }
	}
		
	freebob_destroy_handle( fb_handle );
		
    } else if ( strcmp( arguments.args[0], "setsamplerate" ) == 0 ) {
	char* tail;
	int samplerate = strtol( arguments.args[1], &tail, 0 );
	if ( errno ) {
	    fprintf( stderr,  "Could not parse samplerate argument\n" );
	    return -1;
	}

	freebob_handle_t fb_handle = freebob_new_handle( arguments.port );
	if ( !fb_handle ) {
	    fprintf( stderr, "Could not create freebob handle\n" );
	    return -1;
	}
		
	if ( freebob_discover_devices( fb_handle, arguments.verbose ) != 0 ) {
	    fprintf( stderr, "Could not discover devices\n" );
	    freebob_destroy_handle( fb_handle );
	    return -1;
	}
	
	if(arguments.node_id_set) {
	    if (! freebob_set_samplerate(fb_handle, arguments.node_id, samplerate)) {
		fprintf( stderr, "Could not set samplerate\n" );
		freebob_destroy_handle( fb_handle );
		return -1;
	    }

	} else {
	    int i=0;
			
	    int devices_on_bus = freebob_get_nb_devices_on_bus(fb_handle);
	    printf("  port = %d, devices_on_bus = %d\n", arguments.port, devices_on_bus);
			
	    for(i=0;i<devices_on_bus;i++) {
		int node_id=freebob_get_device_node_id(fb_handle, i);
		printf("  set samplerate for device = %d, node = %d\n", i, node_id);
				
		if (! freebob_set_samplerate(fb_handle, node_id, samplerate)) {
		    fprintf( stderr, "Could not set samplerate\n" );
		    freebob_destroy_handle( fb_handle );
		    return -1;
		}
	    }

	}
		
	freebob_destroy_handle( fb_handle );
		
    } else if ( strcmp( arguments.args[0], "odiscover" ) == 0 ) {
	freebob_handle_t fb_handle = freebob_new_handle( arguments.port );
	if ( !fb_handle ) {
	    fprintf( stderr, "Could not create freebob handle\n" );
	    return -1;
	}
		
	if ( freebob_discover_devices( fb_handle, arguments.verbose ) != 0 ) {
	    fprintf( stderr, "Could not discover devices\n" );
	    freebob_destroy_handle( fb_handle );
	    return -1;
	}
    } else if ( strcmp( arguments.args[0], "xmldump" ) == 0 ) {
	freebob_handle_t fb_handle = freebob_new_handle( arguments.port );
	if ( !fb_handle ) {
	    fprintf( stderr, "Could not create freebob handle\n" );
	    return -1;
	}
		
	if ( freebob_discover_devices( fb_handle, 0 ) != 0 ) {
	    fprintf( stderr, "Could not discover devices\n" );
	    freebob_destroy_handle( fb_handle );
	    return -1;
	}
	
	if(arguments.node_id_set) {
	    freebob_print_xml_description( fb_handle, 
					   arguments.node_id, 
					   0 );
		
	    printf("\n");
		
	    freebob_print_xml_description( fb_handle, 
					   arguments.node_id,
					   1 );
	} else {
	    int i=0;
			
	    int devices_on_bus = freebob_get_nb_devices_on_bus(fb_handle);
			
	    for(i=0;i<devices_on_bus;i++) {
		int node_id=freebob_get_device_node_id(fb_handle, i);
 				
		freebob_print_xml_description( fb_handle, 
					       node_id, 
					       0 );
		printf("\n");
			
		freebob_print_xml_description( fb_handle, 
					       node_id,
					       1 );
	    }
	}
		
	freebob_destroy_handle( fb_handle );
		    
    } else if ( strcmp( arguments.args[0], "testmultidevicediscovery" ) == 0 ) {
	freebob_connection_info_t* test_info;
	freebob_handle_t fb_handle = freebob_new_handle( arguments.port );
	if ( !fb_handle ) {
	    fprintf( stderr, "Could not create freebob handle\n" );
	    return -1;
	}
		
	if ( freebob_discover_devices( fb_handle, arguments.verbose ) != 0 ) {
	    fprintf( stderr, "Could not discover devices\n" );
	    freebob_destroy_handle( fb_handle );
	    return -1;
	}
	test_info = freebob_get_connection_info( fb_handle, 
						 -1, 
						 0 );
	freebob_print_connection_info( test_info );
	freebob_free_connection_info( test_info );
	
	printf("\n");
	
	test_info = freebob_get_connection_info( fb_handle, 
						 -1,
						 1 );
	freebob_print_connection_info( test_info );
	freebob_free_connection_info( test_info );		
		
	freebob_destroy_handle( fb_handle );
    
    } else if ( strcmp( arguments.args[0], "streamformats" ) == 0 ) {
	freebob_handle_t fb_handle = freebob_new_handle( arguments.port );
	if ( !fb_handle ) {
	    fprintf( stderr, "Could not create freebob handle\n" );
	    return -1;
	}
		
	if ( freebob_discover_devices( fb_handle, arguments.verbose ) != 0 ) {
	    fprintf( stderr, "Could not discover devices\n" );
	    freebob_destroy_handle( fb_handle );
	    return -1;
	}

	freebob_supported_stream_format_info_t* stream_info;
	if(arguments.node_id_set) {
	    printf("  port = %d, node_id = %d\n", arguments.port, arguments.node_id);
	    stream_info = freebob_get_supported_stream_format_info( fb_handle, 
								  arguments.node_id, 
								  0 );
	    freebob_print_supported_stream_format_info( stream_info );
	    freebob_free_supported_stream_format_info( stream_info );
		
	    printf("\n");
		
	    stream_info = freebob_get_supported_stream_format_info( fb_handle, 
								  arguments.node_id,
								  1 );
	    freebob_print_supported_stream_format_info( stream_info );
	    freebob_free_supported_stream_format_info( stream_info );

	} else {
	    int i=0;
			
	    int devices_on_bus = freebob_get_nb_devices_on_bus(fb_handle);
	    printf("  port = %d, devices_on_bus = %d\n", arguments.port, devices_on_bus);
			
	    for(i=0;i<devices_on_bus;i++) {
		int node_id=freebob_get_device_node_id(fb_handle, i);
		printf("  get info for device = %d, node = %d\n", i, node_id);
				
		stream_info = freebob_get_supported_stream_format_info( fb_handle, 
									node_id, 
									0 );
		freebob_print_supported_stream_format_info( stream_info );
		freebob_free_supported_stream_format_info( stream_info );
		
		printf("\n");
		
		stream_info = freebob_get_supported_stream_format_info( fb_handle, 
									node_id,
									1 );
		freebob_print_supported_stream_format_info( stream_info );
		freebob_free_supported_stream_format_info( stream_info );
	    }
	}
		
	freebob_destroy_handle( fb_handle );

    } else {
        printf( "unknown operation\n" );
    }

    return 0;
}
