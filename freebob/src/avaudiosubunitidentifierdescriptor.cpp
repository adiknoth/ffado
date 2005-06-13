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
	printf("--------------- %d\n",aContents);
}

AvAudioSubunitIdentifierDescriptor::~AvAudioSubunitIdentifierDescriptor()  {

}
/**
 * Returns the type name of a function block
 */

char * AvAudioSubunitIdentifierDescriptor::getFunctionBlockTypeName(int type) {
	switch (type) {
	case 0x80:
		return "Selector";
	case 0x81:
		return "Feature";
	case 0x82:
		return "Processing";
	case 0x83:
		return "Codec";
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87:
	case 0x88:
	case 0x89:
	case 0x8A:
	case 0x8B:
	case 0x8C:
	case 0x8D:
	case 0x8E:
	case 0x8F:
		return "Reserved for audioblock extention";
	default:
		return "Reserved";
	}
}
/**
 * Retrieves the function block name
 * 
 */
char * AvAudioSubunitIdentifierDescriptor::getFunctionBlockName(int type) {
	return "";
}
void AvAudioSubunitIdentifierDescriptor::printCapabilities() {

	int offset=0; // update offset when beginning at a new table in the specs for easy reading
	int i=0;
	
	int size_of_list_ID=readByte(offset+1);
	int size_of_object_ID=0; // removes compiler warning
	size_of_object_ID=readByte(offset+2);
	int size_of_oject_position=readByte(offset+3);
	int nb_root_object_lists=readWord(offset+4);
	int root_list_id=0;
	
	
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvAudioSubunitIdentifierDescriptor: \n");
	hexDump(aContents, iLength);
	// PP: calculate the offset to accomodate for the presence of root lists [not implemented]
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t generation_ID:          0x%04X %03d\n",readByte(offset+0),readByte(offset+0));

	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t size_of_list_ID:        0x%04X %03d\n",readByte(offset+1),readByte(offset+1));
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t size_of_object_ID:      0x%04X %03d\n",readByte(offset+2),readByte(offset+2));
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t size_of_oject_position: 0x%04X %03d\n",readByte(offset+3),readByte(offset+3));
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t nb_root_object_lists:   0x%04X %03d\n",readWord(offset+4),readWord(offset+4));

	offset=6;
	for (i=0;i<nb_root_object_lists;i++) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t root list id nr %d: %X\n",i,readWord(offset));	
		root_list_id=readWord(offset);
		
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t  trying to open & read list \n");	
		
		AvDescriptor desc(cParent, qTarget, AVC_DESCRIPTOR_SPECIFIER_TYPE_LIST_BY_ID,
			root_list_id,size_of_list_ID,0,0,0,0);
		desc.OpenReadOnly();
		if (desc.isOpen()) {
			desc.Load();
			unsigned char buff[desc.getLength()];
			hexDump(buff, desc.getLength());
			desc.Close();
		}		
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
			int nb_channels=0;
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
					debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t    channel %d name entry number: %04X\n", c, readWord(offset+10+c*2));
					AvDescriptor desc(cParent, qTarget, AVC_DESCRIPTOR_SPECIFIER_TYPE_ENTRY_BY_POSITION_IN_LIST,
						root_list_id,size_of_list_ID,0,0,readWord(offset+10+c*2),size_of_oject_position);
					desc.OpenReadOnly();
					if (desc.isOpen()) {
						desc.Load();
						desc.Close();
					}
				}
			}
			int nb_of_source_plug_link_info=readByte(offset+10+nb_channels*2);
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t  nb_of_source_plug_link_info %d\n", nb_of_source_plug_link_info);
			int p=0;
			for(p=0;p<nb_of_source_plug_link_info;p++) {
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   ..._plug_link_info %04X\n", readWord(offset+10+nb_channels*2+1+p*2));
			}
			
			int nb_fb_dep_info=readByte(offset+10+nb_channels*2+1+nb_of_source_plug_link_info*2);
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t  Nb of Function block info's: %d\n",nb_fb_dep_info);
			
			int fb=0;
			int fb_offset=offset+10+nb_channels*2+1+nb_of_source_plug_link_info*2+1;
			for(fb=0;fb<nb_fb_dep_info;fb++) {
				int fb_info_length=readWord(fb_offset);
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t  Function block info %d length %04X\n",fb,fb_info_length);
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   type:    %02X (%s)\n",readByte(fb_offset+2), getFunctionBlockTypeName(readByte(fb_offset+2)));
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   id:      %02X\n",readByte(fb_offset+3));
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   name id: %04X (%s)\n",readWord(fb_offset+4), getFunctionBlockName(readWord(fb_offset+4)));
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
}
