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

/*
 * This file implements a simple interface to a shared memory object used
 * to share device configuration between FFADO components.  This is required
 * because a significant amount of device information is not available for
 * reading from the device itself.
 *
 * The idea is that each RME FFADO process will call rme_shm_open() to
 * obtain a pointer to shared memory containing the structure of interest. 
 * If no process has yet created the shared object it will be created at
 * this point; otherwise the existing object will be used.  Each new process
 * to use the object increments a reference count.
 *
 * On exit, processes using this shared object will call rme_shm_close().
 * The reference counter is decremented and if it becomes zero the shared
 * object will be unlinked.  This way, so long as at least one RME process
 * is active (which doesn't have to be the process which created the object
 * initially) the device's configuration will be persistent.
 */

#define RME_SHM_NAME  "/ffado:rme_shm"
#define RME_SHM_SIZE  sizeof(rme_shm_t)

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "rme_shm.h"

rme_shm_t *rme_shm_open(void) {

    signed int shmfd;
    rme_shm_t *data;
    signed int created = 0;

    // There is a small race condition here - if another RME FFADO process
    // closes between the first and second shm_open() calls and in doing so
    // removes the shm object, the current process will end up without a shm
    // object since the second shm_open() call will fail.  At this point the
    // practical likelihood of this is low, so we'll live with it.
    shmfd = shm_open(RME_SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (shmfd < 0) {
        if (errno==EEXIST) {
            shmfd = shm_open(RME_SHM_NAME, O_RDWR, 0644);
            if (shmfd < 0) 
                return NULL;
        } else {
            return NULL;
        }
    } else {
        ftruncate(shmfd, RME_SHM_SIZE);
        created = 1;
    }

    data = (rme_shm_t *)mmap(NULL, RME_SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);
    close(shmfd);

    if (data == MAP_FAILED)
        return NULL;

    if (created) {
        pthread_mutex_init(&data->lock, NULL);
    }

    // There's another unlikely race condition here.  If another process has
    // done rme_shm_close() which results in a zero reference count between
    // our second shm_open() call and here, we will be mapped to a shared
    // object which is inaccessible to any subsequent processes due to the
    // shm_unlink() call in rme_shm_close().  Again though, the practical
    // changes of this occurring are low.
    pthread_mutex_lock(&data->lock);
    data->ref_count++;
    if (created)
        data->valid++;
    pthread_mutex_unlock(&data->lock);

    return data;
}

void rme_shm_close(rme_shm_t *shm_data) {

    signed int unlink = 0;

    pthread_mutex_lock(&shm_data->lock);
    shm_data->ref_count--;
    shm_data->valid = 0;
    unlink = (shm_data->ref_count == 0);
    pthread_mutex_unlock(&shm_data->lock);

    // There's nothing to gain by explicitly calling pthread_mutex_destroy()
    // on shm_data->lock, and doing so could cause issues for other processes
    // which were waiting on the lock during the previous code block.

    munmap(shm_data, RME_SHM_SIZE);

    if (unlink)
        shm_unlink(RME_SHM_NAME);
}
