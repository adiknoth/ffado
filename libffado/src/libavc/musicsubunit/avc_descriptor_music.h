/*
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

/**
 * Implements the AV/C Descriptors/InfoBlocks for the Music Subunit as in TA2004007
 *
 */

#ifndef AVCDESCRIPTORMUSIC_H
#define AVCDESCRIPTORMUSIC_H

#include "../descriptors/avc_descriptor.h"
#include "../avc_definitions.h"

#include "../general/avc_generic.h"
#include "../general/avc_plug.h"

#include "debugmodule/debugmodule.h"

#include <vector>
#include <string>

class Ieee1394Service;

namespace AVC {

/**
 * The info blocks
 */
class AVCMusicGeneralStatusInfoBlock : public AVCInfoBlock
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    AVCMusicGeneralStatusInfoBlock( );
    virtual ~AVCMusicGeneralStatusInfoBlock() {};
    virtual const char* getInfoBlockName() const
        {return "AVCMusicGeneralStatusInfoBlock";};

    byte_t  m_current_transmit_capability;
    byte_t  m_current_receive_capability;
    quadlet_t m_current_latency_capability;
    
protected:

private:

};

class AVCMusicOutputPlugStatusInfoBlock : public AVCInfoBlock
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    AVCMusicOutputPlugStatusInfoBlock( );
    virtual ~AVCMusicOutputPlugStatusInfoBlock() {};
    virtual const char* getInfoBlockName() const
        {return "AVCMusicOutputPlugStatusInfoBlock";};

protected:

private:

};

class AVCMusicClusterInfoBlock : public AVCInfoBlock
{
public:

    struct sSignalInfo {
        uint16_t music_plug_id;
        byte_t stream_position;
        byte_t stream_location;
    };
    typedef std::vector<struct sSignalInfo> SignalInfoVector;
    typedef std::vector<struct sSignalInfo>::iterator SignalInfoVectorIterator;

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual bool clear();

    std::string getName();

    AVCMusicClusterInfoBlock( );
    virtual ~AVCMusicClusterInfoBlock();
    virtual const char* getInfoBlockName() const
        {return "AVCMusicClusterInfoBlock";};

    byte_t      m_stream_format;
    byte_t      m_port_type;
    byte_t      m_nb_signals;
    SignalInfoVector m_SignalInfos;

    AVCRawTextInfoBlock m_RawTextInfoBlock;
    AVCNameInfoBlock    m_NameInfoBlock;

    virtual void show();

protected:

private:

};
typedef std::vector<AVCMusicClusterInfoBlock *> AVCMusicClusterInfoBlockVector;
typedef std::vector<AVCMusicClusterInfoBlock *>::iterator AVCMusicClusterInfoBlockVectorIterator;

class AVCMusicSubunitPlugInfoBlock : public AVCInfoBlock
{
public:
    enum AVCMusicSubunitPlugInfoBlockPlugType {
        ePT_IsoStream   = 0x0,
        ePT_AsyncStream = 0x1,
        ePT_Midi        = 0x2,
        ePT_Sync        = 0x3,
        ePT_Analog      = 0x4,
        ePT_Digital     = 0x5,

        ePT_Unknown     = 0xff,
    };

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    AVCMusicSubunitPlugInfoBlock( );
    virtual ~AVCMusicSubunitPlugInfoBlock();
    virtual const char* getInfoBlockName() const
        {return "AVCMusicSubunitPlugInfoBlock";};
        
    virtual bool clear();
    
    std::string getName();
    
    byte_t      m_subunit_plug_id;
    uint16_t    m_signal_format;
    byte_t      m_plug_type;
    uint16_t    m_nb_clusters;
    uint16_t    m_nb_channels;

    AVCMusicClusterInfoBlockVector m_Clusters;
    AVCRawTextInfoBlock m_RawTextInfoBlock;
    AVCNameInfoBlock    m_NameInfoBlock;

protected:

private:

};
typedef std::vector<AVCMusicSubunitPlugInfoBlock *> AVCMusicSubunitPlugInfoBlockVector;
typedef std::vector<AVCMusicSubunitPlugInfoBlock *>::iterator AVCMusicSubunitPlugInfoBlockVectorIterator;

class AVCMusicPlugInfoBlock : public AVCInfoBlock
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual bool clear();

    AVCMusicPlugInfoBlock( );
    virtual ~AVCMusicPlugInfoBlock() {};
    virtual const char* getInfoBlockName() const
        {return "AVCMusicPlugInfoBlock";};

    std::string getName();

    byte_t      m_music_plug_type;
    uint16_t    m_music_plug_id;
    byte_t      m_routing_support;
    byte_t      m_source_plug_function_type;
    byte_t      m_source_plug_id;
    byte_t      m_source_plug_function_block_id;
    byte_t      m_source_stream_position;
    byte_t      m_source_stream_location;
    byte_t      m_dest_plug_function_type;
    byte_t      m_dest_plug_id;
    byte_t      m_dest_plug_function_block_id;
    byte_t      m_dest_stream_position;
    byte_t      m_dest_stream_location;
    
    AVCRawTextInfoBlock m_RawTextInfoBlock;
    AVCNameInfoBlock    m_NameInfoBlock;

    virtual void show();

protected:

private:

};
typedef std::vector<AVCMusicPlugInfoBlock *> AVCMusicPlugInfoBlockVector;
typedef std::vector<AVCMusicPlugInfoBlock *>::iterator AVCMusicPlugInfoBlockVectorIterator;

class AVCMusicRoutingStatusInfoBlock : public AVCInfoBlock
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    AVCMusicRoutingStatusInfoBlock( );
    virtual ~AVCMusicRoutingStatusInfoBlock();
    virtual const char* getInfoBlockName() const
        {return "AVCMusicRoutingStatusInfoBlock";};

    AVCMusicSubunitPlugInfoBlock *getSubunitPlugInfoBlock(Plug::EPlugDirection, plug_id_t);
    AVCMusicPlugInfoBlock *getMusicPlugInfoBlock(plug_id_t);
    
    virtual bool clear();

    byte_t      m_nb_dest_plugs;
    byte_t      m_nb_source_plugs;
    uint16_t    m_nb_music_plugs;
    
    AVCMusicSubunitPlugInfoBlockVector  mDestPlugInfoBlocks;
    AVCMusicSubunitPlugInfoBlockVector  mSourcePlugInfoBlocks;
    AVCMusicPlugInfoBlockVector         mMusicPlugInfoBlocks;

protected:

private:

};

/**
 * 
 */
class AVCMusicStatusDescriptor : public AVCDescriptor
{

public:
    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    
    AVCMusicStatusDescriptor( Unit* unit, Subunit* subunit );
    virtual ~AVCMusicStatusDescriptor() {}
    
    virtual const char* getDescriptorName() const
        {return "AVCMusicStatusDescriptor";};
        
    AVCMusicSubunitPlugInfoBlock *getSubunitPlugInfoBlock(Plug::EPlugDirection, plug_id_t);
    AVCMusicPlugInfoBlock *getMusicPlugInfoBlock(plug_id_t);
    unsigned int getNbMusicPlugs();

private:
    // the child info blocks
    AVCMusicGeneralStatusInfoBlock m_general_status_infoblock;
    AVCMusicOutputPlugStatusInfoBlock m_output_plug_status_infoblock;
    AVCMusicRoutingStatusInfoBlock m_routing_status_infoblock;
    
};

}

#endif // AVCDESCRIPTORMUSIC_H
