/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "fireworks_device.h"
#include "efc/efc_avc_cmd.h"
#include "efc/efc_cmd.h"
#include "efc/efc_cmds_hardware.h"

#include "audiofire/audiofire_device.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "config.h"

// FireWorks is the platform used and developed by ECHO AUDIO
namespace FireWorks {

Device::Device( Ieee1394Service& ieee1394Service,
                            std::auto_ptr<ConfigRom>( configRom ))
    : GenericAVC::AvDevice( ieee1394Service, configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created FireWorks::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

Device::~Device()
{
}

void
Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a FireWorks::Device\n");
//     GenericAVC::AvDevice::showDevice();

    EfcOverAVCCmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setSubunitType( AVC::eST_Unit  );
    cmd.setSubunitId( 0xff );
    
    cmd.setVerbose( getDebugLevel() );
    cmd.setVerbose( DEBUG_LEVEL_VERY_VERBOSE );

    EfcHardwareInfoCmd *efccmd=new EfcHardwareInfoCmd();
    if (!efccmd) {
        debugError("could not allocate efc cmd\n");
        return;
    }
    cmd.m_cmd = efccmd;

    if ( !cmd.fire() ) {
        debugError( "EfcOverAVCCmd command failed\n" );
    }

    efccmd->showEfcCmd();
    delete efccmd;
    
    // test the next command
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    
    EfcPolledValuesCmd *efccmd2=new EfcPolledValuesCmd();
    if (!efccmd2) {
        debugError("could not allocate efc cmd 2\n");
        return;
    }
    cmd.m_cmd = efccmd2;

    if ( !cmd.fire() ) {
        debugError( "EfcOverAVCCmd command failed\n" );
    }

    efccmd2->showEfcCmd();
    delete efccmd2;
    
}


bool
Device::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    GenericAVC::VendorModel vendorModel( SHAREDIR "/ffado_driver_fireworks.txt" );
    if ( vendorModel.parse() ) {
        vendorModel.printTable();
        return vendorModel.isPresent( vendorId, modelId );
    }

    return false;
}

bool
Device::discover()
{
    unsigned int vendorId = m_pConfigRom->getNodeVendorId();
    unsigned int modelId = m_pConfigRom->getModelId();

    GenericAVC::VendorModel vendorModel( SHAREDIR "/ffado_driver_fireworks.txt" );
    if ( vendorModel.parse() ) {
        m_model = vendorModel.find( vendorId, modelId );
    }

    if (!GenericAVC::VendorModel::isValid(m_model)) {
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
            m_model.vendor_name.c_str(), m_model.model_name.c_str());

    if ( !GenericAVC::AvDevice::discover() ) {
        debugError( "Could not discover GenericAVC::AvDevice\n" );
        return false;
    }

    return true;
}

FFADODevice *
Device::createDevice( Ieee1394Service& ieee1394Service,
                      std::auto_ptr<ConfigRom>( configRom ))
{
    unsigned int vendorId = configRom->getNodeVendorId();

//     return NULL;

    unsigned int modelId = configRom->getModelId();

    switch(vendorId) {
        case FW_VENDORID_ECHO: return new ECHO::AudioFire(ieee1394Service, configRom );
        default: return new Device(ieee1394Service, configRom );
    }
}


} // FireWorks
