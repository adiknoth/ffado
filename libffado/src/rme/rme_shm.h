/*
 * Copyright (C) 2009 by Jonathan Woithe
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef _RME_SHM_H
#define _RME_SHM_H

#include <stdint.h>
#include <pthread.h>
#include "fireface_def.h"

/* Structure used within shared memory object */

typedef struct rme_shm_t {
    signed int ref_count;
    signed int settings_valid, tco_settings_valid;
    FF_software_settings_t settings;
    FF_TCO_settings_t tco_settings;

    pthread_mutex_t lock;
} rme_shm_t;

/* Return values from rme_shm_open().  RSO = Rme Shared Object. */
#define RSO_ERR_MMAP      -3
#define RSO_ERR_SHM       -2
#define RSO_ERROR         -1
#define RSO_OPEN_CREATED   0
#define RSO_OPEN_ATTACHED  1

/* Return values from rme_shm_close() */
#define RSO_CLOSE          0
#define RSO_CLOSE_DELETE   1

/* Functions */

signed int rme_shm_open(rme_shm_t **shm_data);
signed int rme_shm_close(rme_shm_t *shm_data);

#endif
