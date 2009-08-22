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

#define RME_SHM_LOCKNAME "/ffado:rme_shm_lock"

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "rme_shm.h"

static signed int rme_shm_lock(void) {
signed lockfd;

    do {
        // The check for existance and shm creation are atomic so it's save
        // to use this as the basis for a global lock.
        lockfd = shm_open(RME_SHM_LOCKNAME, O_RDWR | O_CREAT | O_EXCL, 0644);
        if (lockfd < 0)
            usleep(10000);
    } while (lockfd < 0);

    return lockfd;
}

static void rme_shm_unlock(signed int lockfd) {
    close(lockfd);
    shm_unlink(RME_SHM_LOCKNAME);
}

signed int rme_shm_open(rme_shm_t **shm_data) {

    signed int shmfd, lockfd;
    rme_shm_t *data;
    signed int created = 0;

    if (shm_data == NULL) {
        return RSO_ERROR;
    }
    *shm_data = NULL;

    lockfd = rme_shm_lock();

    shmfd = shm_open(RME_SHM_NAME, O_RDWR, 0644);
    if (shmfd < 0) {
        if (errno == ENOENT) {
            shmfd = shm_open(RME_SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0644);
            if (shmfd < 0)
                return RSO_ERR_SHM;
            else {
                ftruncate(shmfd, RME_SHM_SIZE);
                created = 1;
            }
        } else
            return RSO_ERR_SHM;
    }

    data = (rme_shm_t *)mmap(NULL, RME_SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);
    close(shmfd);

    if (data == MAP_FAILED)
        return RSO_ERR_MMAP;

    if (created) {
        pthread_mutex_init(&data->lock, NULL);
    }

    pthread_mutex_lock(&data->lock);
    data->ref_count++;
    pthread_mutex_unlock(&data->lock);

    rme_shm_unlock(lockfd);

    *shm_data = data;
    return created?RSO_OPEN_CREATED:RSO_OPEN_ATTACHED;
}

signed int rme_shm_close(rme_shm_t *shm_data) {

    signed int unlink = 0;
    signed int lockfd;

    lockfd = rme_shm_lock();

    pthread_mutex_lock(&shm_data->lock);
    shm_data->ref_count--;
    unlink = (shm_data->ref_count == 0);
    pthread_mutex_unlock(&shm_data->lock);

    if (unlink) {
        // This is safe: if the reference count is zero there can't be any
        // other process using the lock at this point.
        pthread_mutex_destroy(&shm_data->lock);
    }

    munmap(shm_data, RME_SHM_SIZE);

    if (unlink)
        shm_unlink(RME_SHM_NAME);

    rme_shm_unlock(lockfd);

    return unlink?RSO_CLOSE_DELETE:RSO_CLOSE;
}
