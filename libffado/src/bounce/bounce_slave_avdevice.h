/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */
#ifndef __FREEBOB_BOUNCESLAVEDEVICE__
#define __FREEBOB_BOUNCESLAVEDEVICE__

#include "debugmodule/debugmodule.h"
#include "bounce_avdevice.h"

#define FREEBOB_MAX_NAME_LEN 256

#define FREEBOB_BOUNCE_SERVER_VENDORNAME  "FreeBoB Server"
#define FREEBOB_BOUNCE_SERVER_MODELNAME   "freebob-server"

// NOTE: this is currently free, but it is not really allowed to use
#define FREEBOB_BOUNCE_SERVER_VENDORID    0x000B0001
#define FREEBOB_BOUNCE_SERVER_MODELID     0x000B0001
#define FREEBOB_BOUNCE_SERVER_SPECID      0x000B0001

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

#endif /* __FREEBOB_BOUNCESLAVEDEVICE__ */


