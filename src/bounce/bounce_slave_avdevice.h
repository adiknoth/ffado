/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __FFADO_BOUNCESLAVEDEVICE__
#define __FFADO_BOUNCESLAVEDEVICE__

#include "debugmodule/debugmodule.h"
#include "bounce_avdevice.h"

#define FFADO_MAX_NAME_LEN 256

#define FFADO_BOUNCE_SERVER_VENDORNAME  "FFADO Server"
#define FFADO_BOUNCE_SERVER_MODELNAME   "ffado-server"

// NOTE: this is currently free, but it is not really allowed to use
#define FFADO_BOUNCE_SERVER_VENDORID    0x000B0001
#define FFADO_BOUNCE_SERVER_MODELID     0x000B0001
#define FFADO_BOUNCE_SERVER_SPECID      0x000B0001

namespace Bounce {

class BounceSlaveDevice : public BounceDevice {
    class BounceSlaveNotifier;
public:

    BounceSlaveDevice( std::auto_ptr<ConfigRom>( configRom ),
          Ieee1394Service& ieee1394Service );
    virtual ~BounceSlaveDevice();

    static bool probe( ConfigRom& configRom );
    bool discover();
    bool prepare();
    bool lock();
    bool unlock();

    bool startStreamByIndex(int i);
    bool stopStreamByIndex(int i);

private:
    bool waitForRegisterNotEqualTo(nodeaddr_t offset, fb_quadlet_t v);
    bool initMemSpace();
    bool restoreMemSpace();

private: // configrom shit

    struct configrom_backup {
        quadlet_t rom[0x100];
        size_t rom_size;
        unsigned char rom_version;
    };
    struct configrom_backup m_original_config_rom;

    struct configrom_backup
        save_config_rom(raw1394handle_t handle);
    int restore_config_rom(raw1394handle_t handle, struct configrom_backup old);
    int init_config_rom(raw1394handle_t handle);

private:
    BounceSlaveNotifier *m_Notifier;
    /**
     * this class reacts on the ohter side writing to the
     * hosts address space
     */
    class BounceSlaveNotifier : public ARMHandler
    {
    public:
        BounceSlaveNotifier(BounceSlaveDevice *, nodeaddr_t start);
        virtual ~BounceSlaveNotifier();

    private:
        BounceSlaveDevice *m_bounceslavedevice;
    };
};

} // end of namespace Bounce

#endif /* __FFADO_BOUNCESLAVEDEVICE__ */


