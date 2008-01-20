/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#include "libieee1394/configrom.h"

#include "../general/avc_subunit.h"
#include "../general/avc_unit.h"

#include "../general/avc_plug_info.h"
#include "../streamformat/avc_extended_stream_format.h"
#include "libutil/cmd_serialize.h"

#include "avc_musicsubunit.h"
#include "avc_descriptor_music.h"

#include <sstream>

namespace AVC {

////////////////////////////////////////////

SubunitMusic::SubunitMusic( Unit& unit, subunit_t id )
    : Subunit( unit, eST_Music, id )
    , m_status_descriptor ( new AVCMusicStatusDescriptor( &unit, this ) )
{

}

SubunitMusic::SubunitMusic()
    : Subunit()
    , m_status_descriptor ( NULL )
{
}

SubunitMusic::~SubunitMusic()
{
    if (m_status_descriptor) delete m_status_descriptor;
}

bool
SubunitMusic::discover()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering %s...\n", getName());

    // discover the AV/C generic part
    if ( !Subunit::discover() ) {
        return false;
    }

    // now we have the subunit plugs

    return true;
}

bool
SubunitMusic::initPlugFromDescriptor( Plug& plug )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Loading info from descriptor for plug: \n");
    bool result=true;

    // load the descriptor (if not already loaded)
    if (m_status_descriptor != NULL) {
        result &= m_status_descriptor->load();
    }

    AVCMusicSubunitPlugInfoBlock *info;
    info = m_status_descriptor->getSubunitPlugInfoBlock(plug.getDirection(), plug.getPlugId());

    if (info == NULL) {
        debugError("Could not find plug info block\n");
        return false;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "Found plug: %s\n",info->getName().c_str());

    // plug name
    result &= plug.setName(info->getName());

    // plug type
    switch (info->m_plug_type) {
        case AVCMusicSubunitPlugInfoBlock::ePT_IsoStream:
            result &= plug.setPlugType(Plug::eAPT_IsoStream);
            break;
        case AVCMusicSubunitPlugInfoBlock::ePT_AsyncStream:
            result &= plug.setPlugType(Plug::eAPT_AsyncStream);
            break;
        case AVCMusicSubunitPlugInfoBlock::ePT_Midi:
            result &= plug.setPlugType(Plug::eAPT_Midi);
            break;
        case AVCMusicSubunitPlugInfoBlock::ePT_Sync:
            result &= plug.setPlugType(Plug::eAPT_Sync);
            break;
        case AVCMusicSubunitPlugInfoBlock::ePT_Analog:
            result &= plug.setPlugType(Plug::eAPT_Analog);
            break;
        case AVCMusicSubunitPlugInfoBlock::ePT_Digital:
            result &= plug.setPlugType(Plug::eAPT_Digital);
            break;
    }

    // number of channels
    result &= plug.setNrOfChannels(info->m_nb_channels);

    int idx=1;
    for ( AVCMusicClusterInfoBlockVectorIterator it = info->m_Clusters.begin();
          it != info->m_Clusters.end();
          ++it )
    {
        struct Plug::ClusterInfo cinfo;

        AVCMusicClusterInfoBlock *c=(*it);

        cinfo.m_index=idx; //FIXME: is this correct?
        cinfo.m_portType=c->m_port_type;
        cinfo.m_nrOfChannels=c->m_nb_signals;
        cinfo.m_streamFormat=c->m_stream_format;
        cinfo.m_name=c->getName();

        debugOutput(DEBUG_LEVEL_VERBOSE, "Adding cluster idx=%2d type=%02X nbch=%2d fmt=%02X name=%s\n",
            cinfo.m_index, cinfo.m_portType, cinfo.m_nrOfChannels, cinfo.m_streamFormat, cinfo.m_name.c_str());

        for ( AVCMusicClusterInfoBlock::SignalInfoVectorIterator sig_it
                  = c->m_SignalInfos.begin();
              sig_it != c->m_SignalInfos.end();
              ++sig_it )
        {
            struct AVCMusicClusterInfoBlock::sSignalInfo s=(*sig_it);
            struct Plug::ChannelInfo sinfo;

            sinfo.m_streamPosition=s.stream_position;
            sinfo.m_location=s.stream_location;

            AVCMusicPlugInfoBlock *mplug=m_status_descriptor->getMusicPlugInfoBlock(s.music_plug_id);

            if (mplug==NULL) {
                debugWarning("No music plug found for this signal\n");
                sinfo.m_name="unknown";
            } else {
                sinfo.m_name=mplug->getName();
            }

            if (plug.getDirection() == Plug::eAPD_Input) {
                // it's an input plug to the subunit
                // so we have to check the source field of the music plug
                if(s.stream_position != mplug->m_source_stream_position) {
                    debugWarning("s.stream_position (= 0x%02X) != mplug->m_source_stream_position (= 0x%02X)\n",
                        s.stream_position, mplug->m_source_stream_position);
                    // use the one from the music plug
                    sinfo.m_streamPosition= mplug->m_source_stream_position;
                }
                if(s.stream_location != mplug->m_source_stream_location) {
                    debugWarning("s.stream_location (= 0x%02X) != mplug->m_source_stream_location (= 0x%02X)\n",
                        s.stream_location, mplug->m_source_stream_location);
                    // use the one from the music plug
                    sinfo.m_location=mplug->m_source_stream_location;
                }
            } else if (plug.getDirection() == Plug::eAPD_Output) {
                // it's an output plug from the subunit
                // so we have to check the destination field of the music plug
                if(s.stream_position != mplug->m_dest_stream_position) {
                    debugWarning("s.stream_position (= 0x%02X) != mplug->m_dest_stream_position (= 0x%02X)\n",
                        s.stream_position, mplug->m_dest_stream_position);
                    // use the one from the music plug
                    sinfo.m_streamPosition=mplug->m_dest_stream_position;
                }
                if(s.stream_location != mplug->m_dest_stream_location) {
                    debugWarning("s.stream_location (= 0x%02X) != mplug->m_dest_stream_location (= 0x%02X)\n",
                        s.stream_location, mplug->m_dest_stream_location);
                    // use the one from the music plug
                    sinfo.m_location=mplug->m_dest_stream_location;
                }
            } else {
                debugWarning("Invalid plug direction.\n");
            }

            debugOutput(DEBUG_LEVEL_VERBOSE, "Adding signal pos=%2d loc=%2d name=%s\n",
                sinfo.m_streamPosition, sinfo.m_location, mplug->getName().c_str());

            cinfo.m_channelInfos.push_back(sinfo);
        }

        idx++;
        plug.getClusterInfos().push_back(cinfo);
    }


    return result;

}

bool
SubunitMusic::loadDescriptors()
{
    bool result=true;
    if (m_status_descriptor != NULL) {
        result &= m_status_descriptor->load();
    } else {
        debugError("BUG: m_status_descriptor == NULL\n");
        return false;
    }
    return result;
}

void SubunitMusic::setVerboseLevel(int l)
{
    Subunit::setVerboseLevel(l);
    if (m_status_descriptor != NULL) {
        m_status_descriptor->setVerboseLevel(l);
    }
}

const char*
SubunitMusic::getName()
{
    return "MusicSubunit";
}

bool
SubunitMusic::serializeChild( Glib::ustring basePath,
                              Util::IOSerialize& ser ) const
{
    return true;
}

bool
SubunitMusic::deserializeChild( Glib::ustring basePath,
                                Util::IODeserialize& deser,
                                Unit& unit )
{
    return true;
}

}
