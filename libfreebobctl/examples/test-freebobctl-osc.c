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
	
	printf("Using freebob control library version: %s\n",freebobctl_get_version());
		
	if (argc <= 1) {
		printf("Usage: test-freebobctl-osc url\n", argv[0]);
		return(0);
	}

	url = argv[1];

	freebob_connection_info_t *test_info=freebobctl_get_connection_info_from_osc(url, 0);
	freebobctl_free_connection_info(test_info);
	
	//test_info=freebobctl_get_connection_info_from_xml_file(docname, 1);
	//freebobctl_free_connection_info(test_info);
	
	
	return (1);
}
