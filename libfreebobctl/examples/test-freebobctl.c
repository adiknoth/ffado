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
	char *docname;
	
	printf("Using freebob control library version: %s\n",freebobctl_get_version());
		
	if (argc <= 1) {
		printf("Usage: test-freebobctl docname\n", argv[0]);
		return(0);
	}

	docname = argv[1];

	freebobctl_xmlparse_file(docname);
	return (1);
}
