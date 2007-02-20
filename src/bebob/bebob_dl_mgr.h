/* bebob_dl_mgr.h
 * Copyright (C) 2006 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */
#ifndef BEBOB_DL_MGR_H
#define BEBOB_DL_MGR_H

#include "bebob_dl_codes.h"

#include "fbtypes.h"
#include "threads.h"

#include "debugmodule/debugmodule.h"

#include <iostream>

class Ieee1394Service;
class ConfigRom;

namespace BeBoB {
    class BCD;

    class BootloaderManager {
    public:
        BootloaderManager( Ieee1394Service& ieee1349service,
                           fb_nodeid_t nodeId );
        ~BootloaderManager();

        const ConfigRom* const getConfigRom() const
            { return m_configRom; }

        void printInfoRegisters();

        bool downloadFirmware( std::string filename );
        bool downloadCnE( std::string filename );
        bool programGUID( octlet_t guid );

	void busresetHandler();

	Ieee1394Service* get1394Serivce() const
            { return m_ieee1394service; }

	bool setForceOperations( bool enabled )
            { m_forceEnabled = enabled; return true; }

	bool setStartBootloader( bool bStartBootloader )
	    { m_bStartBootloader = bStartBootloader; return true; }
    protected:
	enum EObjectType {
	    eOT_Application,
	    eOT_CnE
	};

	void waitForBusReset();
        bool writeRequest( CommandCodes& cmd );
	bool readResponse( CommandCodes& writeRequestCmd );
	bool downloadObject( BCD& bcd, EObjectType eObject );

        bool programGUIDCmd( octlet_t guid );
	bool startBootloaderCmd();
        bool startApplicationCmd();
        bool initializePersParamCmd();
        bool initializeConfigToFactorySettingCmd();
        bool checkDeviceCompatibility( BCD& bcd );

    private:
        bool cacheInfoRegisters();
        bool cacheInfoRegisters( int retries );

        struct info_register_t {
            fb_octlet_t  m_manId;
            fb_quadlet_t m_protocolVersion;
            fb_quadlet_t m_bootloaderVersion;
            fb_octlet_t  m_guid;
            fb_quadlet_t m_hardwareModelId;
            fb_quadlet_t m_hardwareRevision;
            fb_octlet_t  m_softwareDate;
            fb_octlet_t  m_softwareTime;
            fb_quadlet_t m_softwareId;
            fb_quadlet_t m_softwareVersion;
            fb_quadlet_t m_baseAddress;
            fb_quadlet_t m_maxImageLen;
            fb_octlet_t  m_bootloaderDate;
            fb_octlet_t  m_bootloaderTime;
            fb_octlet_t  m_debuggerDate;
            fb_octlet_t  m_debuggerTime;
            fb_quadlet_t m_debuggerId;
            fb_quadlet_t m_debuggerVersion;
        };

        Ieee1394Service*   m_ieee1394service;
        ConfigRom*         m_configRom;

        EBootloaderProtocolVersion m_protocolVersion;
        bool               m_isAppRunning;
        info_register_t    m_cachedInfoRegs;

	pthread_mutex_t m_mutex;
	pthread_cond_t  m_cond;

	Functor*        m_functor;

        bool            m_forceEnabled;
	bool            m_bStartBootloader;

	DECLARE_DEBUG_MODULE;
    };
}

#endif
