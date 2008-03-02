/*
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

#ifndef downloader_h
#define downloader_h

#include <argp.h>

/* ------------------------------------- */
/* has to be provided by the application */
extern char *doc;
extern struct argp_option* options;

/* ------------------------------------- */
struct arguments
{
    char* args[3];
    short verbose;
    int   port;
    int   force;
    int   no_bootloader_restart;
};

extern struct arguments* args;
extern struct argp* argp;

error_t parse_opt( int key, char* arg, struct argp_state* state );
void printDeviceList();

#endif /* downloader_h */
