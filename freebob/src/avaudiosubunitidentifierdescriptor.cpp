/* avaudiosubunitidentifierdescriptor.cpp
 * Copyright (C) 2004 by Pieter Palmers
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
#include "avdevice.h"
#include "avdescriptor.h"
#include "avaudiosubunitidentifierdescriptor.h"

AvAudioSubunitIdentifierDescriptor::AvAudioSubunitIdentifierDescriptor(AvDevice *parent, unsigned char id) : AvDescriptor(parent, AVC1394_SUBUNIT_TYPE_AUDIO | (id<<16),0x00) {
	if (!(this->AvDescriptor::isPresent())) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvAudioSubunitIdentifierDescriptor: Descriptor not present!\n");
		return;
	}
	
	if (!(this->AvDescriptor::isOpen())) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvAudioSubunitIdentifierDescriptor: Opening descriptor...\n");
		this->AvDescriptor::OpenReadOnly();
		if (!(this->AvDescriptor::isOpen()) ) {
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvAudioSubunitIdentifierDescriptor:   Failed!\n");
			return;
		}
	}
	
	if (!(this->AvDescriptor::isLoaded())) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvAudioSubunitIdentifierDescriptor: Loading descriptor...\n");
		this->AvDescriptor::Load();
		if (!(this->AvDescriptor::isLoaded())) {
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvAudioSubunitIdentifierDescriptor:   Failed!\n");
			return;
		}
	}
}

AvAudioSubunitIdentifierDescriptor::~AvAudioSubunitIdentifierDescriptor()  {

}

void AvAudioSubunitIdentifierDescriptor::printCapabilities() {

	int offset=0; // update offset when beginning at a new table in the specs for easy reading
	int i=0;
	
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvAudioSubunitIdentifierDescriptor: \n");
	hexDump(aContents, iLength);
	// PP: calculate the offset to accomodate for the presence of root lists [not implemented]
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t generation_ID:          0x%04X %03d\n",readByte(offset+0),readByte(offset+0));

	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t size_of_list_ID:        0x%04X %03d\n",readByte(offset+1),readByte(offset+1));
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t size_of_object_ID:      0x%04X %03d\n",readByte(offset+2),readByte(offset+2));
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t size_of_oject_position: 0x%04X %03d\n",readByte(offset+3),readByte(offset+3));
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t nb_root_object_lists:   0x%04X %03d\n",readWord(offset+4),readWord(offset+4));

	int nb_root_object_lists=readWord(offset+4);
	int size_of_list_ID=readByte(offset+1);
	offset=6;
	for (i=0;i<nb_root_object_lists;i++) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t root list id nr %d: %X\n",i,readByte(offset));	
		offset+=size_of_list_ID;
	}
	
	int audio_depinfo_length=readWord(offset);
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t audio_depinfo_length:   0x%04X %03d\n",audio_depinfo_length,audio_depinfo_length);
	
	if (audio_depinfo_length>2) { //present
		offset=offset+2;
		int audio_depinfo_fields_length=readWord(offset);
		
 		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t audio_depinfo_fields_length:   0x%04X %03d\n",audio_depinfo_fields_length,audio_depinfo_fields_length);
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t audio_subunit_version:          0x%04X %03d\n",readByte(offset+2),readByte(offset+2));
		int nb_configurations=readByte(offset+3);
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t nb configurations:          0x%04X %03d\n",nb_configurations,nb_configurations);
		offset+=4;
		for (i=0;i<nb_configurations;i++) {
			int nb_channels;
			int config_len=readWord(offset);
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t configuration %d length %04X\n",i,config_len);
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   configuration id: %04X\n", readWord(offset+2));
			int master_cluster_info_len=readWord(offset+4);
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   master cluster info len: %04X\n", master_cluster_info_len);
			if(master_cluster_info_len>2) { //present
				nb_channels=readByte(offset+6);
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   master cluster nb channels: %d\n", nb_channels);
				int config_type=readByte(offset+7);
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   master cluster config type: %d\n", config_type);
				int predefined_channelconfig=readWord(offset+8);
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   master cluster predefined_channelconfig: %04X\n", predefined_channelconfig);
				int c=0;
				for(c=0;c<nb_channels;c++) {
					debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t    channel %d name id: %04X\n", c, readWord(offset+10+c*2));
				}
			}
			int nb_of_source_plug_link_info=readByte(offset+10+nb_channels*2);
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t  nb_of_source_plug_link_info %d\n", nb_of_source_plug_link_info);
			int p=0;
			for(p=0;p<nb_of_source_plug_link_info;p++) {
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   ..._plug_link_info %04X\n", readWord(offset+10+nb_channels*2+1+p));
			}
			
			int bn_fb_dep_info=readByte(offset+10+nb_channels*2+1+nb_of_source_plug_link_info+1);
			int fb=0;
			int fb_offset=offset+10+nb_channels*2+1+nb_of_source_plug_link_info+1+1;
			for(fb=0;fb<bn_fb_dep_info;fb++) {
				int fb_info_length=readWord(fb_offset);
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t  Function block info %d length %04X\n",fb,fb_info_length);
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   type:    %02X\n",readByte(fb_offset+2));
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   id:      %02X\n",readByte(fb_offset+3));
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   name id: %04X\n",readWord(fb_offset+4));
				int nb_fb_input_plugs=readByte(fb_offset+6);
				int fb_p=0;
				for (fb_p=0;fb_p<nb_fb_input_plugs;fb_p++) {
					debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t    plug %d source id: %04X\n",fb_p,readWord(fb_offset+7+2*fb_p));
				}
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   Cluster information length %04X\n",readWord(readWord(fb_offset+7+2*nb_fb_input_plugs)));
				
				
				
				fb_offset+=fb_info_length+2;
			}			
			
			offset+=config_len+2;
		}
	}
	
#if 0	
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t music_subunit_dependant_info_fields_length:     0x%04X %03d\n",readWord(offset+0),readWord(offset+0));
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t attributes:                                       0x%02X %03d\n",readByte(offset+2),readByte(offset+2));
	
	// PP: read the optional extra attribute bytes [not implemented]
	
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t music_subunit_version:                            0x%02X %03d\n",readByte(offset+3),readByte(offset+3));
	
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t music_subunit_specific_information_length:      0x%04X %03d\n",readWord(offset+4),readWord(offset+4));
	
	offset=14;
	unsigned char capability_attributes=readByte(offset+0);
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t capability_attributes:                            0x%02X %03d\n",capability_attributes,capability_attributes);
	
	// PP: #defines in ieee1394service.h for the moment
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   -> capabilities: ");
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_GENERAL)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," general ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIO)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," audio ");
	}
	
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_MIDI)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," midi ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SMPTE)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," smtpe ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SAMPLECOUNT)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," samplecount ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," audiosync ");
	}
	debugPrintShort(DEBUG_LEVEL_DESCRIPTOR,"\n");
	
	// start parsing the optional capability stuff
	offset=15;
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_GENERAL)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   general capabilities:\n");
		
		unsigned char transmit_capability=readByte(offset+1);
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      transmit: %s%s\n",
			(transmit_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_BLOCKING)?"blocking ":"",
			(transmit_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_NONBLOCKING)?"non-blocking ":"");
		
		unsigned char receive_capability=readByte(offset+2);
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      receive:  %s%s\n",
			(receive_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_BLOCKING)?"blocking ":"",
			(receive_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_NONBLOCKING)?"non-blocking ":"");

		// PP: skip undefined latency cap
			
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIO)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   audio capabilities: (%d entries)\n",readByte(offset+1));
		for (int i=0;i<readByte(offset+1);i++) {
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      %d: #inputs=%02d, #outputs=%02d, format=0x%04X)\n",i+1,readWord(offset+2+i*6),readWord(offset+4+i*6),readWord(offset+6+i*6));
		}
		
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_MIDI)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   midi capabilities:\n");
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      midi version:             %d.%d\n",((readByte(offset+1) & 0xF0) >> 4),(readByte(offset+1) & 0x0F));
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      adaptation layer version: %d.%d\n",readByte(offset+2));
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      #inputs=%02d, #outputs=%02d\n",readWord(offset+3),readWord(offset+5));
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SMPTE)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   smtpe capabilities:\n");
		// PP: not present on my quatafire
		
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SAMPLECOUNT)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   samplecount capabilities:\n");
		// PP: not present on my quatafire
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   audiosync capabilities:\n");
		
		unsigned char audiosync_capability=readByte(offset+1);
		
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      sync from:  %s%s\n",
			(audiosync_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC_BUS)?" 1394 bus ":"",
			(audiosync_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC_EXTERNAL)?"  external source ":"");
		
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
#endif
}
