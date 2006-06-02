/* configrom.cpp
 * Copyright (C) 2005 by Daniel Wagner
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

#include "configrom.h"
#include "libfreebobavc/ieee1394service.h"

#include <stdio.h>
#include <string.h>

#include <iostream>

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

ConfigRom::ConfigRom( Ieee1394Service* ieee1394service, fb_nodeid_t nodeId )
    : m_1394Service( ieee1394service )
    , m_nodeId( nodeId )
    , m_avcDevice( false )
    , m_guid( 0 )
    , m_vendorName( "" )
    , m_modelName( "" )
    , m_vendorNameKv( 0 )
    , m_modelNameKv( 0 )
    , m_csr( 0 )
{
}

ConfigRom::~ConfigRom()
{
}

bool
ConfigRom::initialize()
{
     struct config_csr_info csr_info;
     csr_info.service = m_1394Service;
     csr_info.nodeId = 0xffc0 | m_nodeId;

     m_csr = csr1212_create_csr( &configrom_csr1212_ops,
                                 5 * sizeof(fb_quadlet_t),   // XXX Why 5 ?!?
                                 &csr_info );
    if (!m_csr || csr1212_parse_csr( m_csr ) != CSR1212_SUCCESS) {
        debugError( "Could not parse config rom of node %d on port %d", m_nodeId, m_1394Service->getPort() );
        if (m_csr) {
            csr1212_destroy_csr(m_csr);
            m_csr = 0;
        }
        return false;
    }

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

const bool
ConfigRom::isAvcDevice() const
{
    return m_avcDevice;
}

// XXX This might work only for the M-Audio Audiophile
// but can easily extended.
#define VENDOR_ID_MAUDIO    0x00000d6c
#define MODEL_ID_MAUDIO_BOOTLOADER 0x00010060

const bool
ConfigRom::isBootloader() const
{
    if ( ( m_vendorId == VENDOR_ID_MAUDIO )
         && ( m_modelId == MODEL_ID_MAUDIO_BOOTLOADER ) )
    {
        return true;
    }
    return false;
}

static int
busRead( struct csr1212_csr* csr,
         u_int64_t addr,
         u_int16_t length,
         void* buffer,
         void* private_data )
{
    struct config_csr_info* csr_info = (struct config_csr_info*) private_data;

    if ( csr_info->service->read( csr_info->nodeId,
                                  addr,
                                  length,
                                  ( quadlet_t* )buffer) )
    {
        //debugOutput( DEBUG_LEVEL_VERBOSE, "ConfigRom: Read failed\n");
        return -1;
    }

    return 0;
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
    unsigned int specifier_id = 0;

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
                specifier_id = kv->value.immediate;
		break;

	    case CSR1212_KV_ID_VERSION:
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "\tversion = 0x%08x\n",
                             kv->value.immediate);
                if ( specifier_id == 0x0000a02d ) // XXX
                {
                    if ( kv->value.immediate == 0x10001 ) {
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
ConfigRom::getModelName() const
{
    return m_modelName;
}

const std::string
ConfigRom::getVendorName() const
{
    return m_vendorName;
}
