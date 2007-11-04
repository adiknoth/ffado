/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Jonathan Woithe
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#ifndef CONFIGROM_H
#define CONFIGROM_H

#include "fbtypes.h"
#include "csr1212.h"

#include "libutil/serialize.h"
#include "debugmodule/debugmodule.h"

#include "libcontrol/Element.h"

#include <string>

class Ieee1394Service;

class ConfigRom
    : public Control::Element
{
 public:
    ConfigRom( Ieee1394Service& ieee1394service, fb_nodeid_t nodeId );
    virtual ~ConfigRom();

    bool initialize();

    bool operator == ( const ConfigRom& rhs );

    const fb_nodeid_t getNodeId() const;
    const fb_octlet_t getGuid() const;
    const Glib::ustring getGuidString() const;
    const Glib::ustring getModelName() const;
    const Glib::ustring getVendorName() const;

    const unsigned int getModelId() const;
    // FIXME: isn't this the same as getNodeVendorId?
    const unsigned int getVendorId() const;
    const unsigned int getUnitSpecifierId() const;
    const unsigned int getUnitVersion() const;

    bool isIsoResourseManager() const
    { return m_isIsoResourceManager; }
    bool isCycleMasterCapable() const
        { return m_isCycleMasterCapable; }
    bool isSupportsIsoOperations() const
        { return m_isSupportIsoOperations; }
    bool isBusManagerCapable() const
        { return m_isBusManagerCapable; }
    fb_byte_t getCycleClockAccurancy() const
        { return m_cycleClkAcc; }
    fb_byte_t getMaxRec() const
        { return m_maxRec; }
    unsigned short getAsyMaxPayload() const;

    fb_quadlet_t getNodeVendorId() const
    { return m_nodeVendorId; }

    bool updatedNodeId();
    bool setNodeId( fb_nodeid_t nodeId );
    
    /**
     * @brief Compares the GUID of two ConfigRom's
     *
     * This function compares the GUID of two ConfigRom objects and returns true
     * if the GUID of @ref a is larger than the GUID of @ref b . This is intended
     * to be used with the STL sort() algorithm.
     * 
     * Note that GUID's are converted to integers for this.
     * 
     * @param a pointer to first ConfigRom
     * @param b pointer to second ConfigRom
     * 
     * @returns true if the GUID of @ref a is larger than the GUID of @ref b .
     */
    static bool compareGUID(  const ConfigRom& a, const ConfigRom& b );

    void printConfigRom() const;

    bool serialize( Glib::ustring path, Util::IOSerialize& ser );
    static ConfigRom* deserialize( Glib::ustring path,
                   Util::IODeserialize& deser,
                   Ieee1394Service& ieee1394Service );

    void setVerboseLevel(int level) {
        setDebugLevel(level);
        Element::setVerboseLevel(level);
    }

 protected:
    void processUnitDirectory( struct csr1212_csr*    csr,
                               struct csr1212_keyval* ud_kv,
                               unsigned int*          id );

    void processRootDirectory( struct csr1212_csr* csr );

    Ieee1394Service* m_1394Service;
    fb_nodeid_t      m_nodeId;
    bool             m_avcDevice;
    fb_octlet_t      m_guid;
    Glib::ustring    m_vendorName;
    Glib::ustring    m_modelName;
    unsigned int     m_vendorId;
    unsigned int     m_modelId;
    unsigned int     m_unit_specifier_id;
    unsigned int     m_unit_version;
    bool             m_isIsoResourceManager;
    bool             m_isCycleMasterCapable;
    bool             m_isSupportIsoOperations;
    bool             m_isBusManagerCapable;
    fb_byte_t        m_cycleClkAcc;
    fb_byte_t        m_maxRec;
    fb_quadlet_t     m_nodeVendorId;
    fb_byte_t        m_chipIdHi;
    fb_quadlet_t     m_chipIdLow;

    /* only used during parsing */
    struct csr1212_keyval* m_vendorNameKv;
    struct csr1212_keyval* m_modelNameKv;
    struct csr1212_csr*    m_csr;

private:
    ConfigRom( const ConfigRom& ); // do not allow copy ctor
    ConfigRom();                   // ctor for deserialition

    DECLARE_DEBUG_MODULE;
};

#endif /* CONFIGROM_H */
