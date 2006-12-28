// test-freebobctl.c
//
/****************************************************************************
   Copyright (C) 2005, Pieter Palmers. All rights reserved.
   <pieterpalmers@users.sourceforge.net>   
*****************************************************************************/

#include <stdio.h>

#include "freebobctl/freebobctl.h"

int
main(int argc, char **argv) {
	char *url;
	freebob_connection_info_t *test_info;
	
	printf("Using freebob control library version: %s\n",freebobctl_get_version());
		
	if (argc <= 1) {
		printf("Usage: test-freebobctl-osc url\n", argv[0]);
		return(0);
	}

	url = argv[1];

	// capture
	printf("CAPTURE DIRECTION\n"); 
	test_info=freebobctl_get_connection_info_from_osc(url, 0);
	freebobctl_print_connection_info(test_info);
	freebobctl_free_connection_info(test_info);

	printf("PLAYBACK DIRECTION\n"); 
	test_info=freebobctl_get_connection_info_from_osc(url, 1);
	freebobctl_print_connection_info(test_info);
	freebobctl_free_connection_info(test_info);

	
	
	return (1);
}
