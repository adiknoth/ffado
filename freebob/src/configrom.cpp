/* configrom.cpp
 * Copyright (C) 2005 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include <stdio.h>
#include <string.h>
#include "configrom.h"

static int busRead(struct csr1212_csr* csr,
                         u_int64_t addr,
                         u_int16_t length,
                         void* buffer,
                         void* private_data);

static int getMaxRom(u_int32_t* bus_info_data,
                     void* private_data);

static struct csr1212_bus_ops configrom_csr1212_ops = {
    busRead,
    0,
    0,
    getMaxRom
};

struct config_csr_info {
    raw1394handle_t handle;
    int node_id;
};

/*-------------------------------------------------------------*/

ConfigRom::ConfigRom( raw1394handle_t raw1394Handle, int iNodeId )
    : m_raw1394Handle( raw1394Handle )
    , m_iNodeId( iNodeId )
    , m_bAvcDevice( false )
    , m_vendorName( "" )
    , m_modelName( "" )
    , m_vendorNameKv( 0 )
    , m_modelNameKv( 0 )
{
}

ConfigRom::~ConfigRom()
{
}

bool
ConfigRom::initialize()
{
    struct config_csr_info csr_info;
    csr_info.handle = m_raw1394Handle;
    csr_info.node_id = 0xffc0 | m_iNodeId;

    m_csr = csr1212_create_csr(&configrom_csr1212_ops,
			     5 * sizeof(quadlet_t),
			     &csr_info);
    if (!m_csr || csr1212_parse_csr(m_csr) != CSR1212_SUCCESS) {
	fprintf(stderr, "couldn't parse config rom\n");
	if (m_csr) {
	    csr1212_destroy_csr(m_csr);
            m_csr = 0;
	}
	return false;
    }

    processRootDirectory(m_csr);

    if ( m_vendorNameKv ) {
        int len = ( m_vendorNameKv->value.leaf.len - 2 ) * sizeof( quadlet_t );
        char* vendorName = new char[len];
        memcpy( vendorName,
                ( void* )CSR1212_TEXTUAL_DESCRIPTOR_LEAF_DATA( m_vendorNameKv ),
                len );
        vendorName[len] = '\0';
        printf( "Vendor name: %s\n", vendorName );
        m_vendorName = vendorName;
    }
    if ( m_modelNameKv ) {
        int len = ( m_modelNameKv->value.leaf.len - 2 ) * sizeof( quadlet_t );
        char* modelName = new char[len];
        memcpy( modelName,
                ( void* )CSR1212_TEXTUAL_DESCRIPTOR_LEAF_DATA( m_modelNameKv ),
                len );
        modelName[len] = '\0';
        printf( "Model name: %s\n", modelName );
        m_modelName = modelName;
    }

    m_guid =  ((u_int64_t)CSR1212_BE32_TO_CPU(m_csr->bus_info_data[3]) << 32)
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
    return m_bAvcDevice;;
}


static int
busRead(struct csr1212_csr* csr,
        u_int64_t addr,
        u_int16_t length,
        void* buffer,
        void* private_data)
{
    struct config_csr_info* csr_info = (struct config_csr_info*) private_data;

    unsigned int retval;
    retval=raw1394_read(csr_info->handle,
                        csr_info->node_id,
                        addr,
			length,
                        ( quadlet_t* )buffer);
    if (retval) {
	perror("read failed");
    }

    return retval;
}

static int
getMaxRom(u_int32_t* bus_info_data,
          void* /*private_data*/)
{
    return (CSR1212_BE32_TO_CPU(bus_info_data[2]) >> 8) & 0x3;
}


void
ConfigRom::processUnitDirectory(struct csr1212_csr* csr,
                                struct csr1212_keyval* ud_kv,
                                unsigned int *id)
{
    struct csr1212_dentry *dentry;
    struct csr1212_keyval *kv;
    unsigned int last_key_id = 0;
    unsigned int specifier_id = 0;

    printf("process unit directory:\n");
    csr1212_for_each_dir_entry(csr, kv, ud_kv, dentry) {
	switch (kv->key.id) {
	    case CSR1212_KV_ID_VENDOR:
		if (kv->key.type == CSR1212_KV_TYPE_IMMEDIATE) {
		    printf("\tvendor_id = 0x%08x\n",
			   kv->value.immediate);
		}
		break;

	    case CSR1212_KV_ID_MODEL:
		printf("\tmodel_id = 0x%08x\n",
		       kv->value.immediate);
		break;

	    case CSR1212_KV_ID_SPECIFIER_ID:
		printf("\tspecifier_id = 0x%08x\n",
		       kv->value.immediate);
                specifier_id = kv->value.immediate;
		break;

	    case CSR1212_KV_ID_VERSION:
		printf("\tversion = 0x%08x\n",
		       kv->value.immediate);
                if ( specifier_id == 0x0000a02d ) // XXX
                {
                    if ( kv->value.immediate == 0x10001 ) {
                        m_bAvcDevice = true;
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
		printf("vendor id = 0x%08x\n", kv->value.immediate);
		break;

	    case CSR1212_KV_ID_NODE_CAPABILITIES:
		printf("capabilities = 0x%08x\n", kv->value.immediate);
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

const octlet_t
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
