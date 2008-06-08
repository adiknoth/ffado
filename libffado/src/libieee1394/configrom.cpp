/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2005-2008 by Jonathan Woithe
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#include "config.h"

#include "configrom.h"
#include "ieee1394service.h"

#include "vendor_model_ids.h"

#include "libutil/SystemTimeSource.h"

#include <stdio.h>
#include <string.h>

#include <iostream>
#include <iomanip>

using namespace std;

IMPL_DEBUG_MODULE( ConfigRom, ConfigRom, DEBUG_LEVEL_NORMAL );

static int busRead( struct csr1212_csr* csr,
                    u_int64_t addr,
                    u_int16_t length,
                    void* buffer,
                    void* private_data );

static int getMaxRom( u_int32_t* bus_info_data,
                      void* private_data );

static struct csr1212_bus_ops configrom_csr1212_ops = {
    busRead,
    0,
    0,
    getMaxRom
};

struct config_csr_info {
    Ieee1394Service* service;
    fb_nodeid_t      nodeId;
};

//-------------------------------------------------------------

ConfigRom::ConfigRom( Ieee1394Service& ieee1394service, fb_nodeid_t nodeId )
    : Control::Element(NULL, "ConfigRom")
    , m_1394Service( ieee1394service )
    , m_nodeId( nodeId )
    , m_avcDevice( false ) // FIXME: this does not seem veryu
    , m_guid( 0 )
    , m_vendorName( "" )
    , m_modelName( "" )
    , m_vendorId( 0 )
    , m_modelId( 0 )
    , m_unit_specifier_id( 0 )
    , m_unit_version( 0 )
    , m_isIsoResourceManager( false )
    , m_isCycleMasterCapable( false )
    , m_isSupportIsoOperations( false )
    , m_isBusManagerCapable( false )
    , m_cycleClkAcc( 0 )
    , m_maxRec( 0 )
    , m_nodeVendorId( 0 )
    , m_chipIdHi( 0 )
    , m_chipIdLow( 0 )
    , m_vendorNameKv( 0 )
    , m_modelNameKv( 0 )
    , m_csr( 0 )
{
}

ConfigRom::ConfigRom()
    : Control::Element(NULL, "ConfigRom")
    , m_1394Service( *(new Ieee1394Service()) )
    , m_nodeId( -1 )
    , m_avcDevice( false ) // FIXME: this does not seem veryu
    , m_guid( 0 )
    , m_vendorName( "" )
    , m_modelName( "" )
    , m_vendorId( 0 )
    , m_modelId( 0 )
    , m_unit_specifier_id( 0 )
    , m_unit_version( 0 )
    , m_isIsoResourceManager( false )
    , m_isCycleMasterCapable( false )
    , m_isSupportIsoOperations( false )
    , m_isBusManagerCapable( false )
    , m_cycleClkAcc( 0 )
    , m_maxRec( 0 )
    , m_nodeVendorId( 0 )
    , m_chipIdHi( 0 )
    , m_chipIdLow( 0 )
    , m_vendorNameKv( 0 )
    , m_modelNameKv( 0 )
    , m_csr( 0 )
{
}

Ieee1394Service&
ConfigRom::get1394Service()
{
    return m_1394Service;
}

bool
ConfigRom::operator == ( const ConfigRom& rhs )
{
    return m_guid == rhs.m_guid;
}

bool
ConfigRom::compareGUID( const ConfigRom& a, const ConfigRom& b ) {
    return a.getGuid() > b.getGuid();
}

bool
ConfigRom::initialize()
{
     struct config_csr_info csr_info;
     csr_info.service = &m_1394Service;
     csr_info.nodeId = 0xffc0 | m_nodeId;

     m_csr = csr1212_create_csr( &configrom_csr1212_ops,
                                 5 * sizeof(fb_quadlet_t),   // XXX Why 5 ?!?
                                 &csr_info );
    if (!m_csr || csr1212_parse_csr( m_csr ) != CSR1212_SUCCESS) {
        debugError( "Could not parse config rom of node %d on port %d\n", m_nodeId, m_1394Service.getPort() );
        if (m_csr) {
            csr1212_destroy_csr(m_csr);
            m_csr = 0;
        }
        return false;
    }

    // Process Bus_Info_Block
    m_isIsoResourceManager = CSR1212_BE32_TO_CPU(m_csr->bus_info_data[2] ) >> 31;
    m_isCycleMasterCapable = ( CSR1212_BE32_TO_CPU(m_csr->bus_info_data[2] ) >> 30 ) & 0x1;
    m_isSupportIsoOperations = ( CSR1212_BE32_TO_CPU(m_csr->bus_info_data[2] ) >> 29 ) & 0x1;
    m_isBusManagerCapable = ( CSR1212_BE32_TO_CPU(m_csr->bus_info_data[2] ) >> 28 ) & 0x1;
    m_cycleClkAcc = ( CSR1212_BE32_TO_CPU(m_csr->bus_info_data[2] ) >> 16 ) & 0xff;
    m_maxRec = ( CSR1212_BE32_TO_CPU( m_csr->bus_info_data[2] ) >> 12 ) & 0xf;
    m_nodeVendorId = ( CSR1212_BE32_TO_CPU( m_csr->bus_info_data[3] ) >> 8 );
    m_chipIdHi = ( CSR1212_BE32_TO_CPU( m_csr->bus_info_data[3] ) ) & 0xff;
    m_chipIdLow = CSR1212_BE32_TO_CPU( m_csr->bus_info_data[4] );

    // Process Root Directory
    processRootDirectory(m_csr);

    if ( m_vendorNameKv ) {
        int len = ( m_vendorNameKv->value.leaf.len - 2) * sizeof( quadlet_t );
        char* buf = new char[len+2];
        memcpy( buf,
                ( void* )CSR1212_TEXTUAL_DESCRIPTOR_LEAF_DATA( m_vendorNameKv ),
                len );

    while ((buf + len - 1) == '\0') {
            len--;
        }
        // \todo XXX seems a bit strage to do this but the nodemgr.c code does
        // it. try to figure out why this is needed (or not)
    buf[len++] = ' ';
    buf[len] = '\0';


        debugOutput( DEBUG_LEVEL_VERBOSE, "Vendor name: '%s'\n", buf );
        m_vendorName = buf;
        delete[] buf;
    }
    if ( m_modelNameKv ) {
        int len = ( m_modelNameKv->value.leaf.len - 2) * sizeof( quadlet_t );
        char* buf = new char[len+2];
        memcpy( buf,
                ( void* )CSR1212_TEXTUAL_DESCRIPTOR_LEAF_DATA( m_modelNameKv ),
                len );
    while ((buf + len - 1) == '\0') {
            len--;
        }
        // \todo XXX for edirol fa-66 it seems somehow broken. see above
        // todo as well.
    buf[len++] = ' ';
    buf[len] = '\0';

        debugOutput( DEBUG_LEVEL_VERBOSE, "Model name: '%s'\n", buf);
        m_modelName = buf;
        delete[] buf;
    }

    m_guid = ((u_int64_t)CSR1212_BE32_TO_CPU(m_csr->bus_info_data[3]) << 32)
             | CSR1212_BE32_TO_CPU(m_csr->bus_info_data[4]);

    if ( m_vendorNameKv ) {
        csr1212_release_keyval( m_vendorNameKv );
        m_vendorNameKv = 0;
    }
    if ( m_modelNameKv ) {
        csr1212_release_keyval( m_modelNameKv );
        m_modelNameKv = 0;
    }
    if ( m_csr ) {
        csr1212_destroy_csr(m_csr);
        m_csr = 0;
    }
    return true;
}

static int
busRead( struct csr1212_csr* csr,
         u_int64_t addr,
         u_int16_t length,
         void* buffer,
         void* private_data )
{
    struct config_csr_info* csr_info = (struct config_csr_info*) private_data;

    int nb_retries = 5;

    while ( nb_retries-- 
            && !csr_info->service->read( csr_info->nodeId,
                                         addr,
                                         (size_t)length/4,
                                         ( quadlet_t* )buffer)  )
    {// failed, retry
        Util::SystemTimeSource::SleepUsecRelative(IEEE1394SERVICE_CONFIGROM_READ_WAIT_USECS);
    }
    Util::SystemTimeSource::SleepUsecRelative(IEEE1394SERVICE_CONFIGROM_READ_WAIT_USECS);

    if (nb_retries > -1) return 0; // success
    else return -1; // failure
}

static int
getMaxRom( u_int32_t* bus_info_data,
           void* /*private_data*/)
{
    return (CSR1212_BE32_TO_CPU( bus_info_data[2] ) >> 8) & 0x3;
}


void
ConfigRom::processUnitDirectory( struct csr1212_csr* csr,
                                 struct csr1212_keyval* ud_kv,
                                 unsigned int *id )
{
    struct csr1212_dentry *dentry;
    struct csr1212_keyval *kv;
    unsigned int last_key_id = 0;

    debugOutput( DEBUG_LEVEL_VERBOSE, "process unit directory:\n" );
    csr1212_for_each_dir_entry(csr, kv, ud_kv, dentry) {
    switch (kv->key.id) {
        case CSR1212_KV_ID_VENDOR:
        if (kv->key.type == CSR1212_KV_TYPE_IMMEDIATE) {
                    debugOutput( DEBUG_LEVEL_VERBOSE,
                                 "\tvendor_id = 0x%08x\n",
                                 kv->value.immediate);
                    m_vendorId = kv->value.immediate;
        }
        break;

        case CSR1212_KV_ID_MODEL:
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "\tmodel_id = 0x%08x\n",
                             kv->value.immediate);
                m_modelId = kv->value.immediate;
        break;

        case CSR1212_KV_ID_SPECIFIER_ID:
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "\tspecifier_id = 0x%08x\n",
                             kv->value.immediate);
                m_unit_specifier_id = kv->value.immediate;
        break;

        case CSR1212_KV_ID_VERSION:
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "\tversion = 0x%08x\n",
                             kv->value.immediate);
                m_unit_version = kv->value.immediate;
                if ( m_unit_specifier_id == 0x0000a02d ) // XXX
                {
                    m_avcDevice = true; // FIXME: disable this check for the moment
                    if ( kv->value.immediate == 0x14001 ) {
                        m_avcDevice = true;
                    }
                }
        break;

        case CSR1212_KV_ID_DESCRIPTOR:
        if (kv->key.type == CSR1212_KV_TYPE_LEAF &&
            CSR1212_DESCRIPTOR_LEAF_TYPE(kv) == 0 &&
            CSR1212_DESCRIPTOR_LEAF_SPECIFIER_ID(kv) == 0 &&
            CSR1212_TEXTUAL_DESCRIPTOR_LEAF_WIDTH(kv) == 0 &&
            CSR1212_TEXTUAL_DESCRIPTOR_LEAF_CHAR_SET(kv) == 0 &&
            CSR1212_TEXTUAL_DESCRIPTOR_LEAF_LANGUAGE(kv) == 0)
        {
            switch (last_key_id) {
            case CSR1212_KV_ID_VENDOR:
                csr1212_keep_keyval(kv);
                            m_vendorNameKv = kv;
                break;

            case CSR1212_KV_ID_MODEL:
                            m_modelNameKv = kv;
                csr1212_keep_keyval(kv);
                break;

            }
        } /* else if (kv->key.type == CSR1212_KV_TYPE_DIRECTORY) ... */
        break;

        case CSR1212_KV_ID_DEPENDENT_INFO:
        if (kv->key.type == CSR1212_KV_TYPE_DIRECTORY) {
            /* This should really be done in SBP2 as this is
             * doing SBP2 specific parsing. */
            processUnitDirectory(csr, kv, id);
        }

        break;

        default:
        break;
    }
    last_key_id = kv->key.id;
    }
}

void
ConfigRom::processRootDirectory(struct csr1212_csr* csr)
{
    unsigned int ud_id = 0;
    struct csr1212_dentry *dentry;
    struct csr1212_keyval *kv;
    unsigned int last_key_id = 0;

    csr1212_for_each_dir_entry(csr, kv, csr->root_kv, dentry) {
    switch (kv->key.id) {
        case CSR1212_KV_ID_VENDOR:
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "vendor id = 0x%08x\n", kv->value.immediate);
        break;

        case CSR1212_KV_ID_NODE_CAPABILITIES:
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "capabilities = 0x%08x\n", kv->value.immediate);
        break;

        case CSR1212_KV_ID_UNIT:
        processUnitDirectory(csr, kv, &ud_id);
        break;

        case CSR1212_KV_ID_DESCRIPTOR:
        if (last_key_id == CSR1212_KV_ID_VENDOR) {
            if (kv->key.type == CSR1212_KV_TYPE_LEAF &&
            CSR1212_DESCRIPTOR_LEAF_TYPE(kv) == 0 &&
            CSR1212_DESCRIPTOR_LEAF_SPECIFIER_ID(kv) == 0 &&
            CSR1212_TEXTUAL_DESCRIPTOR_LEAF_WIDTH(kv) == 0 &&
            CSR1212_TEXTUAL_DESCRIPTOR_LEAF_CHAR_SET(kv) == 0 &&
            CSR1212_TEXTUAL_DESCRIPTOR_LEAF_LANGUAGE(kv) == 0)
                    {
                        m_vendorNameKv = kv;
            csr1212_keep_keyval(kv);
            }
        }
        break;
    }
    last_key_id = kv->key.id;
    }

}

const fb_nodeid_t
ConfigRom::getNodeId() const
{
    return m_nodeId;
}

const fb_octlet_t
ConfigRom::getGuid() const
{
    return m_guid;
}

const std::string
ConfigRom::getGuidString() const
{
    char* buf;
    asprintf( &buf, "%08x%08x",
              ( unsigned int ) ( getGuid() >> 32 ),
              ( unsigned int ) ( getGuid() & 0xffffffff ) );
    std::string result = buf;
    free( buf );
    return result;
}
 
const std::string
ConfigRom::getModelName() const
{
    // HACK:
    // workarounds for devices that don't fill a correct model name
    switch(m_vendorId) {
        case FW_VENDORID_MOTU:
            switch(m_unit_specifier_id) {
                case 0x00000003:
                    return "828MkII";
                case 0x00000009:
                    return "Traveler";
                case 0x0000000d:
                    return "UltraLite";
                case 0x0000000f:
                    return "8pre";
                case 0x00000001:
                    return "828MkI";
                case 0x00000005:
                    return "896HD";
                default:
                    return "unknown";
            }
            break;
        default:
            return m_modelName;
    }
}

const std::string
ConfigRom::getVendorName() const
{
    // HACK:
    // workarounds for devices that don't fill a correct vendor name
    switch(m_vendorId) {
        case FW_VENDORID_MOTU:
            return "MOTU";
        default:
            return m_vendorName;
    }
}

const unsigned int
ConfigRom::getModelId() const
{
    return m_modelId;
}

const unsigned int
ConfigRom::getVendorId() const
{
    return m_vendorId;
}

const unsigned int
ConfigRom::getUnitSpecifierId() const
{
    return m_unit_specifier_id;
}

const unsigned int
ConfigRom::getUnitVersion() const
{
    return m_unit_version;
}

bool
ConfigRom::updatedNodeId()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, 
                 "Checking for updated node id for device with GUID 0x%016llX...\n",
                 getGuid());

    struct csr1212_csr* csr = NULL;
    for ( fb_nodeid_t nodeId = 0;
          nodeId < m_1394Service.getNodeCount();
          ++nodeId )
    {
        struct config_csr_info csr_info;
        csr_info.service = &m_1394Service;
        csr_info.nodeId = 0xffc0 | nodeId;
        debugOutput( DEBUG_LEVEL_VERBOSE, "Looking at node %d...\n", nodeId);

        csr = csr1212_create_csr( &configrom_csr1212_ops,
                                  5 * sizeof(fb_quadlet_t),   // XXX Why 5 ?!?
                                  &csr_info );

        if (!csr || csr1212_parse_csr( csr ) != CSR1212_SUCCESS) {
            debugWarning( "Failed to get/parse CSR\n");
            if (csr) {
                csr1212_destroy_csr(csr);
                csr = NULL;
            }
            continue;
        }

        octlet_t guid =
            ((u_int64_t)CSR1212_BE32_TO_CPU(csr->bus_info_data[3]) << 32)
            | CSR1212_BE32_TO_CPU(csr->bus_info_data[4]);

        debugOutput( DEBUG_LEVEL_VERBOSE,
                        " Node has GUID 0x%016llX\n",
                        guid);

        if ( guid == getGuid() ) {
            debugOutput( DEBUG_LEVEL_VERBOSE, "GUID matches ours\n");
            if ( nodeId != getNodeId() ) {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "Device with GUID 0x%016llX changed node id "
                             "from %d to %d\n",
                             getGuid(),
                             getNodeId(),
                             nodeId );
                m_nodeId = nodeId;
            } else {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "Device with GUID 0x%016llX kept node id %d\n",
                             getGuid(),
                             getNodeId());
            }
            if (csr) {
                csr1212_destroy_csr(csr);
                csr = NULL;
            }
            return true;
        }
    }

    if (csr) {
        csr1212_destroy_csr(csr);
    }

    debugOutput( DEBUG_LEVEL_NORMAL,
                 "Device with GUID 0x%08x%08x could not be found on "
                 "the bus anymore (removed?)\n",
                 m_guid >> 32,
                 m_guid & 0xffffffff );
    return false;
}

void
ConfigRom::printConfigRomDebug() const
{
    using namespace std;
    debugOutput(DEBUG_LEVEL_NORMAL, "Config ROM\n" );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tCurrent Node Id:\t%d\n",       getNodeId() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tGUID:\t\t\t0x%016llX\n",       getGuid());
    debugOutput(DEBUG_LEVEL_NORMAL, "\tVendor Name:\t\t%s\n",         getVendorName().c_str() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tModel Name:\t\t%s\n",          getModelName().c_str() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tNode Vendor ID:\t\t0x%06x\n",  getNodeVendorId() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tModel Id:\t\t0x%08x\n",        getModelId() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tUnit Specifier ID:\t0x%06x\n", getUnitSpecifierId() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tUnit version:\t\t0x%08x\n",    getUnitVersion() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tISO resource manager:\t%d\n",  isIsoResourseManager() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tCycle master capable:\t%d\n",  isSupportsIsoOperations() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tBus manager capable:\t%d\n",   isBusManagerCapable() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tCycle clock accuracy:\t%d\n",  getCycleClockAccurancy() );
    debugOutput(DEBUG_LEVEL_NORMAL, "\tMax rec:\t\t%d (max asy payload: %d bytes)\n",
            getMaxRec(), getAsyMaxPayload() );
}

void
ConfigRom::printConfigRom() const
{
    using namespace std;
    printMessage("Config ROM\n" );
    printMessage("\tCurrent Node Id:\t%d\n",       getNodeId() );
    printMessage("\tGUID:\t\t\t0x%016llX\n",       getGuid());
    printMessage("\tVendor Name:\t\t%s\n",         getVendorName().c_str() );
    printMessage("\tModel Name:\t\t%s\n",          getModelName().c_str() );
    printMessage("\tNode Vendor ID:\t\t0x%06x\n",  getNodeVendorId() );
    printMessage("\tModel Id:\t\t0x%08x\n",        getModelId() );
    printMessage("\tUnit Specifier ID:\t0x%06x\n", getUnitSpecifierId() );
    printMessage("\tUnit version:\t\t0x%08x\n",    getUnitVersion() );
    printMessage("\tISO resource manager:\t%d\n",  isIsoResourseManager() );
    printMessage("\tCycle master capable:\t%d\n",  isSupportsIsoOperations() );
    printMessage("\tBus manager capable:\t%d\n",   isBusManagerCapable() );
    printMessage("\tCycle clock accuracy:\t%d\n",  getCycleClockAccurancy() );
    printMessage("\tMax rec:\t\t%d (max asy payload: %d bytes)\n", getMaxRec(), getAsyMaxPayload() );
}

unsigned short
ConfigRom::getAsyMaxPayload() const
{
    // XXX use pow instead?
    return 1 << ( m_maxRec + 1 );
}

bool
ConfigRom::serialize( std::string path, Util::IOSerialize& ser )
{
    bool result;
    result  = ser.write( path + "m_nodeId", m_nodeId );
    result &= ser.write( path + "m_avcDevice",  m_avcDevice );
    result &= ser.write( path + "m_guid", m_guid );
    result &= ser.write( path + "m_vendorName", std::string( m_vendorName ) );
    result &= ser.write( path + "m_modelName", std::string( m_modelName ) );
    result &= ser.write( path + "m_vendorId", m_vendorId );
    result &= ser.write( path + "m_modelId", m_modelId );
    result &= ser.write( path + "m_unit_specifier_id", m_unit_specifier_id );
    result &= ser.write( path + "m_unit_version",  m_unit_version );
    result &= ser.write( path + "m_isIsoResourceManager", m_isIsoResourceManager );
    result &= ser.write( path + "m_isCycleMasterCapable", m_isCycleMasterCapable );
    result &= ser.write( path + "m_isSupportIsoOperations", m_isSupportIsoOperations );
    result &= ser.write( path + "m_isBusManagerCapable", m_isBusManagerCapable );
    result &= ser.write( path + "m_cycleClkAcc", m_cycleClkAcc );
    result &= ser.write( path + "m_maxRec", m_maxRec );
    result &= ser.write( path + "m_nodeVendorId", m_nodeVendorId );
    result &= ser.write( path + "m_chipIdHi", m_chipIdHi );
    result &= ser.write( path + "m_chipIdLow", m_chipIdLow );
    return result;
}

ConfigRom*
ConfigRom::deserialize( std::string path, Util::IODeserialize& deser, Ieee1394Service& ieee1394Service )
{
    ConfigRom* pConfigRom = new ConfigRom;
    if ( !pConfigRom ) {
        return 0;
    }

    pConfigRom->m_1394Service = ieee1394Service;

    bool result;
    result  = deser.read( path + "m_nodeId", pConfigRom->m_nodeId );
    result &= deser.read( path + "m_avcDevice", pConfigRom->m_avcDevice );
    result &= deser.read( path + "m_guid", pConfigRom->m_guid );
    result &= deser.read( path + "m_vendorName", pConfigRom->m_vendorName );
    result &= deser.read( path + "m_modelName", pConfigRom->m_modelName );
    result &= deser.read( path + "m_vendorId", pConfigRom->m_vendorId );
    result &= deser.read( path + "m_modelId", pConfigRom->m_modelId );
    result &= deser.read( path + "m_unit_specifier_id", pConfigRom->m_unit_specifier_id );
    result &= deser.read( path + "m_unit_version", pConfigRom->m_unit_version );
    result &= deser.read( path + "m_isIsoResourceManager", pConfigRom->m_isIsoResourceManager );
    result &= deser.read( path + "m_isCycleMasterCapable", pConfigRom->m_isCycleMasterCapable );
    result &= deser.read( path + "m_isSupportIsoOperations", pConfigRom->m_isSupportIsoOperations );
    result &= deser.read( path + "m_isBusManagerCapable", pConfigRom->m_isBusManagerCapable );
    result &= deser.read( path + "m_cycleClkAcc", pConfigRom->m_cycleClkAcc );
    result &= deser.read( path + "m_maxRec", pConfigRom->m_maxRec );
    result &= deser.read( path + "m_nodeVendorId", pConfigRom->m_nodeVendorId );
    result &= deser.read( path + "m_chipIdHi", pConfigRom->m_chipIdHi );
    result &= deser.read( path + "m_chipIdLow", pConfigRom->m_chipIdLow );

    if ( !result ) {
        delete pConfigRom;
        return 0;
    }

    return pConfigRom;
}

bool
ConfigRom::setNodeId( fb_nodeid_t nodeId )
{
    m_nodeId = nodeId;
    return true;
}
