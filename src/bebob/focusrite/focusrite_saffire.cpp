/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "focusrite_saffire.h"
#include "focusrite_cmd.h"

namespace BeBoB {
namespace Focusrite {

SaffireDevice::SaffireDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : FocusriteDevice( d, configRom)
    , m_MixerContainer( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Focusrite::SaffireDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );

    // the saffire doesn't seem to like it if the commands are too fast
    if (AVC::AVCCommand::getSleepAfterAVCCommand() < 1000) {
        AVC::AVCCommand::setSleepAfterAVCCommand( 1000 );
    }

    if(getConfigRom().getGuid() < 0x130e0100040000LL) {
        m_isSaffireLE = false;
    } else {
        m_isSaffireLE = true;
    }
}

bool
SaffireDevice::buildMixer()
{
    bool result=true;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Building a Focusrite Saffire mixer...\n");

    destroyMixer();

    // create the mixer object container
    m_MixerContainer = new Control::Container(this, "Mixer");

    if (!m_MixerContainer) {
        debugError("Could not create mixer container...\n");
        return false;
    }
    
    if(m_isSaffireLE) {
        // create control objects for the saffire LE
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_SPDIF_TRANSPARENT, 0,
                    "SpdifTransparent", "S/PDIF Transparent", "S/PDIF Transparent"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_MIDITHRU, 0,
                    "MidiThru", "MIDI Thru", "MIDI Thru"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_SAVE_SETTINGS, 0,
                    "SaveSettings", "Save Settings", "Save Settings"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_HIGH_GAIN_LINE3, 0,
                    "HighGainLine3", "High Gain Line-in 3", "High Gain Line-in 3"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_HIGH_GAIN_LINE4, 0,
                    "HighGainLine4", "High Gain Line-in 4", "High Gain Line-in 4"));

        // output mute controls
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_BITFIELD_OUT12, FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_MUTE,
                    "Out12Mute", "Out1/2 Mute", "Output 1/2 Mute"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_BITFIELD_OUT34, FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_MUTE,
                    "Out34Mute", "Out3/4 Mute", "Output 3/4 Mute"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_BITFIELD_OUT56, FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_MUTE,
                    "Out56Mute", "Out5/6 Mute", "Output 5/6 Mute"));

        // output front panel hw volume control
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_BITFIELD_OUT12, FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_HWCTRL,
                    "Out12HwCtrl", "Out1/2 HwCtrl", "Output 1/2 Front Panel Hardware volume control"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_BITFIELD_OUT34, FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_HWCTRL,
                    "Out34HwCtrl", "Out3/4 HwCtrl", "Output 3/4 Front Panel Hardware volume control"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_BITFIELD_OUT56, FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_HWCTRL,
                    "Out56HwCtrl", "Out5/6 HwCtrl", "Output 5/6 Front Panel Hardware volume control"));

        // output level controls
        result &= m_MixerContainer->addElement(
            new VolumeControlLowRes(*this, 
                    FR_SAFFIRELE_CMD_ID_BITFIELD_OUT12, FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_DAC,
                    "Out12Level", "Out1/2 Level", "Output 1/2 Level"));
        result &= m_MixerContainer->addElement(
            new VolumeControlLowRes(*this, 
                    FR_SAFFIRELE_CMD_ID_BITFIELD_OUT34, FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_DAC,
                    "Out34Level", "Out3/4 Level", "Output 3/4 Level"));
        result &= m_MixerContainer->addElement(
            new VolumeControlLowRes(*this, 
                    FR_SAFFIRELE_CMD_ID_BITFIELD_OUT56, FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_DAC,
                    "Out56Level", "Out5/6 Level", "Output 5/6 Level"));

    } else {
        // create control objects for the saffire
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_SPDIF_SWITCH, 0,
                    "SpdifSwitch", "S/PDIF Switch", "S/PDIF Switch"));

        // output mute controls
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT12, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_MUTE,
                    "Out12Mute", "Out1/2 Mute", "Output 1/2 Mute"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT34, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_MUTE,
                    "Out34Mute", "Out3/4 Mute", "Output 3/4 Mute"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT56, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_MUTE,
                    "Out56Mute", "Out5/6 Mute", "Output 5/6 Mute"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT78, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_MUTE,
                    "Out78Mute", "Out7/8 Mute", "Output 7/8 Mute"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT910, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_MUTE,
                    "Out910Mute", "Out9/10 Mute", "Output 9/10 Mute"));

        // output front panel hw volume control
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT12, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_HWCTRL,
                    "Out12HwCtrl", "Out1/2 HwCtrl", "Output 1/2 Front Panel Hardware volume control"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT34, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_HWCTRL,
                    "Out34HwCtrl", "Out3/4 HwCtrl", "Output 3/4 Front Panel Hardware volume control"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT56, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_HWCTRL,
                    "Out56HwCtrl", "Out5/6 HwCtrl", "Output 5/6 Front Panel Hardware volume control"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT78, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_HWCTRL,
                    "Out78HwCtrl", "Out7/8 HwCtrl", "Output 7/8 Front Panel Hardware volume control"));

        // output level dim
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT12, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_DIM,
                    "Out12Dim", "Out1/2 Dim", "Output 1/2 Level Dim"));

        // output level controls
        result &= m_MixerContainer->addElement(
            new VolumeControlLowRes(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT12, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_DAC,
                    "Out12Level", "Out1/2 Level", "Output 1/2 Level"));
        result &= m_MixerContainer->addElement(
            new VolumeControlLowRes(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT34, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_DAC,
                    "Out34Level", "Out3/4 Level", "Output 3/4 Level"));
        result &= m_MixerContainer->addElement(
            new VolumeControlLowRes(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT56, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_DAC,
                    "Out56Level", "Out5/6 Level", "Output 5/6 Level"));
        result &= m_MixerContainer->addElement(
            new VolumeControlLowRes(*this, 
                    FR_SAFFIRE_CMD_ID_BITFIELD_OUT78, FR_SAFFIRE_CMD_ID_BITFIELD_BIT_DAC,
                    "Out78Level", "Out7/8 Level", "Output 7/8 Level"));
    }
    
    // matrix mix controls
    if(m_isSaffireLE) {
        result &= m_MixerContainer->addElement(
            new SaffireMatrixMixer(*this, SaffireMatrixMixer::eMMT_LEMix48, "LEMix48"));
    
        result &= m_MixerContainer->addElement(
            new SaffireMatrixMixer(*this, SaffireMatrixMixer::eMMT_LEMix96, "LEMix96"));

        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_SWAP_OUT4_OUT1_48K, 0,
                    "Swap41_48", "Swap41_48", "Swap41_48"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, 
                    FR_SAFFIRELE_CMD_ID_SWAP_OUT4_OUT1_96K, 0,
                    "Swap41_96", "Swap41_96", "Swap41_96"));

    } else {
        result &= m_MixerContainer->addElement(
            new SaffireMatrixMixer(*this, SaffireMatrixMixer::eMMT_InputMix, "InputMix"));
    
        result &= m_MixerContainer->addElement(
            new SaffireMatrixMixer(*this, SaffireMatrixMixer::eMMT_PCMix, "PCMix"));
    }

    if (!result) {
        debugWarning("One or more control elements could not be created.");
        // clean up those that were created
        destroyMixer();
        return false;
    }

    if (!addElement(m_MixerContainer)) {
        debugWarning("Could not register mixer to device\n");
        // clean up
        destroyMixer();
        return false;
    }

    // add a direct register access element
    if (!addElement(new RegisterControl(*this, "Register", "Register Access", "Direct register access"))) {
        debugWarning("Could not create register control element.");
        // clean up those that were created
        destroyMixer();
        return false;
    }

    return true;
}

bool
SaffireDevice::destroyMixer()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "destroy mixer...\n");
    
    if (m_MixerContainer == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "no mixer to destroy...\n");
        return true;
    }
    
    if (!deleteElement(m_MixerContainer)) {
        debugError("Mixer present but not registered to the avdevice\n");
        return false;
    }
    
    // remove and delete (as in free) child control elements
    m_MixerContainer->clearElements(true);
    delete m_MixerContainer;
    return true;
}

void
SaffireDevice::showDevice()
{
    if(m_isSaffireLE) {
        debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::Focusrite::SaffireDevice (Saffire LE)\n");
    } else {
        debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::Focusrite::SaffireDevice (Saffire)\n");
    }
    FocusriteDevice::showDevice();
}

void
SaffireDevice::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );

    FocusriteDevice::setVerboseLevel(l);
}

// Saffire pro matrix mixer element

SaffireMatrixMixer::SaffireMatrixMixer(SaffireDevice& p, 
                                       enum eMatrixMixerType type)
: FocusriteMatrixMixer(p, "MatrixMixer")
, m_type(type)
{
    init();
}

SaffireMatrixMixer::SaffireMatrixMixer(SaffireDevice& p, 
                                       enum eMatrixMixerType type, std::string n)
: FocusriteMatrixMixer(p, n)
, m_type(type)
{
    init();
}

void SaffireMatrixMixer::init()
{
    if (m_type==eMMT_PCMix) {
        addSignalInfo(m_RowInfo, "PC12", "PC 1/2", "PC Channel 1/2");
        addSignalInfo(m_RowInfo, "PC34", "PC 3/4", "PC Channel 3/4");
        addSignalInfo(m_RowInfo, "PC56", "PC 5/6", "PC Channel 5/6");
        addSignalInfo(m_RowInfo, "PC78", "PC 7/8", "PC Channel 7/8");
        addSignalInfo(m_RowInfo, "PC910", "PC 9/10", "PC Channel 9/10");
        
        addSignalInfo(m_ColInfo, "OUT12", "OUT 1/2", "Output 1/2");
        addSignalInfo(m_ColInfo, "OUT34", "OUT 3/4", "Output 3/4");
        addSignalInfo(m_ColInfo, "OUT56", "OUT 5/6", "Output 5/6");
        addSignalInfo(m_ColInfo, "OUT78", "OUT 7/8", "Output 7/8");
        addSignalInfo(m_ColInfo, "OUT910", "OUT 9/10", "Output 9/10");
        
        // init the cell matrix
        #define FOCUSRITE_SAFFIRE_PCMIX_NB_COLS 5
        #define FOCUSRITE_SAFFIRE_PCMIX_NB_ROWS 5
        
        std::vector<struct sCellInfo> tmp_cols( FOCUSRITE_SAFFIRE_PCMIX_NB_COLS );
        std::vector< std::vector<struct sCellInfo> > tmp_all(FOCUSRITE_SAFFIRE_PCMIX_NB_ROWS, tmp_cols);
        m_CellInfo = tmp_all;
    
        struct sCellInfo c;
        c.row=-1;
        c.col=-1;
        c.valid=false;
        c.address=0;
        
        for (int i=0;i<FOCUSRITE_SAFFIRE_PCMIX_NB_ROWS;i++) {
            for (int j=0;j<FOCUSRITE_SAFFIRE_PCMIX_NB_COLS;j++) {
                m_CellInfo.at(i).at(j) = c;
            }
        }
    
        // now set the cells that are valid
        setCellInfo(0,0,FR_SAFFIRE_CMD_ID_PC12_TO_OUT12, true);
        setCellInfo(0,1,FR_SAFFIRE_CMD_ID_PC12_TO_OUT34, true);
        setCellInfo(0,2,FR_SAFFIRE_CMD_ID_PC12_TO_OUT56, true);
        setCellInfo(0,3,FR_SAFFIRE_CMD_ID_PC12_TO_OUT79, true);
        setCellInfo(0,4,FR_SAFFIRE_CMD_ID_PC12_TO_OUT910, true);
        setCellInfo(1,0,FR_SAFFIRE_CMD_ID_PC34_TO_OUT12, true);
        setCellInfo(1,1,FR_SAFFIRE_CMD_ID_PC34_TO_OUT34, true);
        setCellInfo(1,2,FR_SAFFIRE_CMD_ID_PC34_TO_OUT56, true);
        setCellInfo(1,3,FR_SAFFIRE_CMD_ID_PC34_TO_OUT79, true);
        setCellInfo(1,4,FR_SAFFIRE_CMD_ID_PC34_TO_OUT910, true);
        setCellInfo(2,0,FR_SAFFIRE_CMD_ID_PC56_TO_OUT12, true);
        setCellInfo(2,1,FR_SAFFIRE_CMD_ID_PC56_TO_OUT34, true);
        setCellInfo(2,2,FR_SAFFIRE_CMD_ID_PC56_TO_OUT56, true);
        setCellInfo(2,3,FR_SAFFIRE_CMD_ID_PC56_TO_OUT79, true);
        setCellInfo(2,4,FR_SAFFIRE_CMD_ID_PC56_TO_OUT910, true);
        setCellInfo(3,0,FR_SAFFIRE_CMD_ID_PC78_TO_OUT12, true);
        setCellInfo(3,1,FR_SAFFIRE_CMD_ID_PC78_TO_OUT34, true);
        setCellInfo(3,2,FR_SAFFIRE_CMD_ID_PC78_TO_OUT56, true);
        setCellInfo(3,3,FR_SAFFIRE_CMD_ID_PC78_TO_OUT79, true);
        setCellInfo(3,4,FR_SAFFIRE_CMD_ID_PC78_TO_OUT910, true);
        setCellInfo(4,0,FR_SAFFIRE_CMD_ID_PC910_TO_OUT12, true);
        setCellInfo(4,1,FR_SAFFIRE_CMD_ID_PC910_TO_OUT34, true);
        setCellInfo(4,2,FR_SAFFIRE_CMD_ID_PC910_TO_OUT56, true);
        setCellInfo(4,3,FR_SAFFIRE_CMD_ID_PC910_TO_OUT79, true);
        setCellInfo(4,4,FR_SAFFIRE_CMD_ID_PC910_TO_OUT910, true);

    } else if (m_type==eMMT_InputMix) {
        addSignalInfo(m_RowInfo, "IN1", "Input 1", "Analog Input 1");
        addSignalInfo(m_RowInfo, "IN2", "Input 2", "Analog Input 2");
        addSignalInfo(m_RowInfo, "SPDIFL", "SPDIF L", "S/PDIF Left Input");
        addSignalInfo(m_RowInfo, "SPDIFR", "SPDIF R", "S/PDIF Right Input");
        addSignalInfo(m_RowInfo, "REV1", "REVERB 1", "Reverb CH1 return");
        addSignalInfo(m_RowInfo, "REV1", "REVERB 2", "Reverb CH2 return");
        
        addSignalInfo(m_ColInfo, "OUT1", "OUT 1", "Output 1");
        addSignalInfo(m_ColInfo, "OUT2", "OUT 2", "Output 2");
        addSignalInfo(m_ColInfo, "OUT3", "OUT 3", "Output 3");
        addSignalInfo(m_ColInfo, "OUT4", "OUT 4", "Output 4");
        addSignalInfo(m_ColInfo, "OUT5", "OUT 5", "Output 5");
        addSignalInfo(m_ColInfo, "OUT6", "OUT 6", "Output 6");
        addSignalInfo(m_ColInfo, "OUT7", "OUT 7", "Output 7");
        addSignalInfo(m_ColInfo, "OUT8", "OUT 8", "Output 8");
        addSignalInfo(m_ColInfo, "OUT9", "OUT 9", "Output 9");
        addSignalInfo(m_ColInfo, "OUT10", "OUT 10", "Output 10");
        
        // init the cell matrix
        #define FOCUSRITE_SAFFIRE_INPUTMIX_NB_COLS 10
        #define FOCUSRITE_SAFFIRE_INPUTMIX_NB_ROWS 6
        
        std::vector<struct sCellInfo> tmp_cols( FOCUSRITE_SAFFIRE_INPUTMIX_NB_COLS );
        std::vector< std::vector<struct sCellInfo> > tmp_all(FOCUSRITE_SAFFIRE_INPUTMIX_NB_ROWS,tmp_cols);
        m_CellInfo = tmp_all;
    
        struct sCellInfo c;
        c.row=-1;
        c.col=-1;
        c.valid=false;
        c.address=0;
        
        for (int i=0;i<FOCUSRITE_SAFFIRE_INPUTMIX_NB_ROWS;i++) {
            for (int j=0;j<FOCUSRITE_SAFFIRE_INPUTMIX_NB_COLS;j++) {
                m_CellInfo.at(i).at(j) = c;
            }
        }

        // now set the cells that are valid
        setCellInfo(0,0,FR_SAFFIRE_CMD_ID_IN1_TO_OUT1, true);
        setCellInfo(0,2,FR_SAFFIRE_CMD_ID_IN1_TO_OUT3, true);
        setCellInfo(0,4,FR_SAFFIRE_CMD_ID_IN1_TO_OUT5, true);
        setCellInfo(0,6,FR_SAFFIRE_CMD_ID_IN1_TO_OUT7, true);
        setCellInfo(0,8,FR_SAFFIRE_CMD_ID_IN1_TO_OUT9, true);
        setCellInfo(1,1,FR_SAFFIRE_CMD_ID_IN2_TO_OUT2, true);
        setCellInfo(1,3,FR_SAFFIRE_CMD_ID_IN2_TO_OUT4, true);
        setCellInfo(1,5,FR_SAFFIRE_CMD_ID_IN2_TO_OUT6, true);
        setCellInfo(1,7,FR_SAFFIRE_CMD_ID_IN2_TO_OUT8, true);
        setCellInfo(1,9,FR_SAFFIRE_CMD_ID_IN2_TO_OUT10, true);
        setCellInfo(2,0,FR_SAFFIRE_CMD_ID_IN3_TO_OUT1, true);
        setCellInfo(2,2,FR_SAFFIRE_CMD_ID_IN3_TO_OUT3, true);
        setCellInfo(2,4,FR_SAFFIRE_CMD_ID_IN3_TO_OUT5, true);
        setCellInfo(2,6,FR_SAFFIRE_CMD_ID_IN3_TO_OUT7, true);
        setCellInfo(2,8,FR_SAFFIRE_CMD_ID_IN3_TO_OUT9, true);
        setCellInfo(3,1,FR_SAFFIRE_CMD_ID_IN4_TO_OUT2, true);
        setCellInfo(3,3,FR_SAFFIRE_CMD_ID_IN4_TO_OUT4, true);
        setCellInfo(3,5,FR_SAFFIRE_CMD_ID_IN4_TO_OUT6, true);
        setCellInfo(3,7,FR_SAFFIRE_CMD_ID_IN4_TO_OUT8, true);
        setCellInfo(3,9,FR_SAFFIRE_CMD_ID_IN4_TO_OUT10, true);

        setCellInfo(4,0,FR_SAFFIRE_CMD_ID_REV1_TO_OUT1, true);
        setCellInfo(5,1,FR_SAFFIRE_CMD_ID_REV2_TO_OUT2, true);
        setCellInfo(4,2,FR_SAFFIRE_CMD_ID_REV1_TO_OUT3, true);
        setCellInfo(5,3,FR_SAFFIRE_CMD_ID_REV2_TO_OUT4, true);
        setCellInfo(4,4,FR_SAFFIRE_CMD_ID_REV1_TO_OUT5, true);
        setCellInfo(5,5,FR_SAFFIRE_CMD_ID_REV2_TO_OUT6, true);
        setCellInfo(4,6,FR_SAFFIRE_CMD_ID_REV1_TO_OUT7, true);
        setCellInfo(5,7,FR_SAFFIRE_CMD_ID_REV2_TO_OUT8, true);
        setCellInfo(4,8,FR_SAFFIRE_CMD_ID_REV1_TO_OUT9, true);
        setCellInfo(5,9,FR_SAFFIRE_CMD_ID_REV2_TO_OUT10, true);

    } else if (m_type == eMMT_LEMix48) {
        addSignalInfo(m_RowInfo, "IN1", "Input 1", "Analog Input 1");
        addSignalInfo(m_RowInfo, "IN2", "Input 2", "Analog Input 2");
        addSignalInfo(m_RowInfo, "IN3", "Input 3", "Analog Input 3");
        addSignalInfo(m_RowInfo, "IN4", "Input 4", "Analog Input 4");
        addSignalInfo(m_RowInfo, "SPDIFL", "SPDIF L", "S/PDIF Left Input");
        addSignalInfo(m_RowInfo, "SPDIFR", "SPDIF R", "S/PDIF Right Input");

        addSignalInfo(m_RowInfo, "PC1", "PC 1", "PC Channel 1");
        addSignalInfo(m_RowInfo, "PC2", "PC 2", "PC Channel 2");
        addSignalInfo(m_RowInfo, "PC3", "PC 3", "PC Channel 3");
        addSignalInfo(m_RowInfo, "PC4", "PC 4", "PC Channel 4");
        addSignalInfo(m_RowInfo, "PC5", "PC 5", "PC Channel 5");
        addSignalInfo(m_RowInfo, "PC6", "PC 6", "PC Channel 6");
        addSignalInfo(m_RowInfo, "PC7", "PC 7", "PC Channel 7");
        addSignalInfo(m_RowInfo, "PC8", "PC 8", "PC Channel 8");

        addSignalInfo(m_ColInfo, "OUT1", "OUT 1", "Output 1");
        addSignalInfo(m_ColInfo, "OUT2", "OUT 2", "Output 2");
        addSignalInfo(m_ColInfo, "OUT3", "OUT 3", "Output 3");
        addSignalInfo(m_ColInfo, "OUT4", "OUT 4", "Output 4");

        // init the cell matrix
        #define FOCUSRITE_SAFFIRELE_48KMIX_NB_COLS 4
        #define FOCUSRITE_SAFFIRELE_48KMIX_NB_ROWS 14
        
        std::vector<struct sCellInfo> tmp_cols( FOCUSRITE_SAFFIRELE_48KMIX_NB_COLS );
        std::vector< std::vector<struct sCellInfo> > tmp_all(FOCUSRITE_SAFFIRELE_48KMIX_NB_ROWS,tmp_cols);
        m_CellInfo = tmp_all;
    
        struct sCellInfo c;
        c.row=-1;
        c.col=-1;
        c.valid=false;
        c.address=0;
        
        for (int i=0;i<FOCUSRITE_SAFFIRELE_48KMIX_NB_ROWS;i++) {
            for (int j=0;j<FOCUSRITE_SAFFIRELE_48KMIX_NB_COLS;j++) {
                m_CellInfo.at(i).at(j) = c;
            }
        }

        // now set the cells that are valid
        setCellInfo(0,0,FR_SAFFIRELE_CMD_ID_IN1_TO_OUT1, true);
        setCellInfo(0,1,FR_SAFFIRELE_CMD_ID_IN1_TO_OUT2, true);
        setCellInfo(0,2,FR_SAFFIRELE_CMD_ID_IN1_TO_OUT3, true);
        setCellInfo(0,3,FR_SAFFIRELE_CMD_ID_IN1_TO_OUT4, true);
        setCellInfo(1,0,FR_SAFFIRELE_CMD_ID_IN2_TO_OUT1, true);
        setCellInfo(1,1,FR_SAFFIRELE_CMD_ID_IN2_TO_OUT2, true);
        setCellInfo(1,2,FR_SAFFIRELE_CMD_ID_IN2_TO_OUT3, true);
        setCellInfo(1,3,FR_SAFFIRELE_CMD_ID_IN2_TO_OUT4, true);
        setCellInfo(2,0,FR_SAFFIRELE_CMD_ID_IN3_TO_OUT1, true);
        setCellInfo(2,1,FR_SAFFIRELE_CMD_ID_IN3_TO_OUT2, true);
        setCellInfo(2,2,FR_SAFFIRELE_CMD_ID_IN3_TO_OUT3, true);
        setCellInfo(2,3,FR_SAFFIRELE_CMD_ID_IN3_TO_OUT4, true);
        setCellInfo(3,0,FR_SAFFIRELE_CMD_ID_IN4_TO_OUT1, true);
        setCellInfo(3,1,FR_SAFFIRELE_CMD_ID_IN4_TO_OUT2, true);
        setCellInfo(3,2,FR_SAFFIRELE_CMD_ID_IN4_TO_OUT3, true);
        setCellInfo(3,3,FR_SAFFIRELE_CMD_ID_IN4_TO_OUT4, true);
        
        setCellInfo(4,0,FR_SAFFIRELE_CMD_ID_SPDIF1_TO_OUT1, true);
        setCellInfo(4,1,FR_SAFFIRELE_CMD_ID_SPDIF1_TO_OUT2, true);
        setCellInfo(4,2,FR_SAFFIRELE_CMD_ID_SPDIF1_TO_OUT3, true);
        setCellInfo(4,3,FR_SAFFIRELE_CMD_ID_SPDIF1_TO_OUT4, true);
        setCellInfo(5,0,FR_SAFFIRELE_CMD_ID_SPDIF2_TO_OUT1, true);
        setCellInfo(5,1,FR_SAFFIRELE_CMD_ID_SPDIF2_TO_OUT2, true);
        setCellInfo(5,2,FR_SAFFIRELE_CMD_ID_SPDIF2_TO_OUT3, true);
        setCellInfo(5,3,FR_SAFFIRELE_CMD_ID_SPDIF2_TO_OUT4, true);
        setCellInfo(6,0,FR_SAFFIRELE_CMD_ID_PC1_TO_OUT1, true);
        setCellInfo(6,1,FR_SAFFIRELE_CMD_ID_PC1_TO_OUT2, true);
        setCellInfo(6,2,FR_SAFFIRELE_CMD_ID_PC1_TO_OUT3, true);
        setCellInfo(6,3,FR_SAFFIRELE_CMD_ID_PC1_TO_OUT4, true);
        setCellInfo(7,0,FR_SAFFIRELE_CMD_ID_PC2_TO_OUT1, true);
        setCellInfo(7,1,FR_SAFFIRELE_CMD_ID_PC2_TO_OUT2, true);
        setCellInfo(7,2,FR_SAFFIRELE_CMD_ID_PC2_TO_OUT3, true);
        setCellInfo(7,3,FR_SAFFIRELE_CMD_ID_PC2_TO_OUT4, true);
        setCellInfo(8,0,FR_SAFFIRELE_CMD_ID_PC3_TO_OUT1, true);
        setCellInfo(8,1,FR_SAFFIRELE_CMD_ID_PC3_TO_OUT2, true);
        setCellInfo(8,2,FR_SAFFIRELE_CMD_ID_PC3_TO_OUT3, true);
        setCellInfo(8,3,FR_SAFFIRELE_CMD_ID_PC3_TO_OUT4, true);
        setCellInfo(9,0,FR_SAFFIRELE_CMD_ID_PC4_TO_OUT1, true);
        setCellInfo(9,1,FR_SAFFIRELE_CMD_ID_PC4_TO_OUT2, true);
        setCellInfo(9,2,FR_SAFFIRELE_CMD_ID_PC4_TO_OUT3, true);
        setCellInfo(9,3,FR_SAFFIRELE_CMD_ID_PC4_TO_OUT4, true);
        setCellInfo(10,0,FR_SAFFIRELE_CMD_ID_PC5_TO_OUT1, true);
        setCellInfo(10,1,FR_SAFFIRELE_CMD_ID_PC5_TO_OUT2, true);
        setCellInfo(10,2,FR_SAFFIRELE_CMD_ID_PC5_TO_OUT3, true);
        setCellInfo(10,3,FR_SAFFIRELE_CMD_ID_PC5_TO_OUT4, true);
        setCellInfo(11,0,FR_SAFFIRELE_CMD_ID_PC6_TO_OUT1, true);
        setCellInfo(11,1,FR_SAFFIRELE_CMD_ID_PC6_TO_OUT2, true);
        setCellInfo(11,2,FR_SAFFIRELE_CMD_ID_PC6_TO_OUT3, true);
        setCellInfo(11,3,FR_SAFFIRELE_CMD_ID_PC6_TO_OUT4, true);
        setCellInfo(12,0,FR_SAFFIRELE_CMD_ID_PC7_TO_OUT1, true);
        setCellInfo(12,1,FR_SAFFIRELE_CMD_ID_PC7_TO_OUT2, true);
        setCellInfo(12,2,FR_SAFFIRELE_CMD_ID_PC7_TO_OUT3, true);
        setCellInfo(12,3,FR_SAFFIRELE_CMD_ID_PC7_TO_OUT4, true);
        setCellInfo(13,0,FR_SAFFIRELE_CMD_ID_PC8_TO_OUT1, true);
        setCellInfo(13,1,FR_SAFFIRELE_CMD_ID_PC8_TO_OUT2, true);
        setCellInfo(13,2,FR_SAFFIRELE_CMD_ID_PC8_TO_OUT3, true);
        setCellInfo(13,3,FR_SAFFIRELE_CMD_ID_PC8_TO_OUT4, true);

    } else if (m_type == eMMT_LEMix96) {
        addSignalInfo(m_RowInfo, "IN1", "Input 1", "Analog Input 1");
        addSignalInfo(m_RowInfo, "IN2", "Input 2", "Analog Input 2");
        addSignalInfo(m_RowInfo, "IN3", "Input 3", "Analog Input 3");
        addSignalInfo(m_RowInfo, "IN4", "Input 4", "Analog Input 4");
        addSignalInfo(m_RowInfo, "SPDIFL", "SPDIF L", "S/PDIF Left Input");
        addSignalInfo(m_RowInfo, "SPDIFR", "SPDIF R", "S/PDIF Right Input");

        addSignalInfo(m_RowInfo, "PC1", "PC 1", "PC Channel 1");
        addSignalInfo(m_RowInfo, "PC2", "PC 2", "PC Channel 2");
        addSignalInfo(m_RowInfo, "PC3", "PC 3", "PC Channel 3");
        addSignalInfo(m_RowInfo, "PC4", "PC 4", "PC Channel 4");
        addSignalInfo(m_RowInfo, "PC5", "PC 5", "PC Channel 5");
        addSignalInfo(m_RowInfo, "PC6", "PC 6", "PC Channel 6");
        addSignalInfo(m_RowInfo, "PC7", "PC 7", "PC Channel 7");
        addSignalInfo(m_RowInfo, "PC8", "PC 8", "PC Channel 8");
        addSignalInfo(m_RowInfo, "RECMIXRETURN", "RECMIXRETURN", "Record mix (mono) return");

        addSignalInfo(m_ColInfo, "OUT1", "OUT 1", "Output 1");
        addSignalInfo(m_ColInfo, "OUT2", "OUT 2", "Output 2");
        addSignalInfo(m_ColInfo, "OUT3", "OUT 3", "Output 3");
        addSignalInfo(m_ColInfo, "OUT4", "OUT 4", "Output 4");
        addSignalInfo(m_ColInfo, "RECMIX", "RECMIX", "Record mix (mono)");

        // init the cell matrix
        #define FOCUSRITE_SAFFIRELE_96KMIX_NB_COLS 5
        #define FOCUSRITE_SAFFIRELE_96KMIX_NB_ROWS 15
        
        std::vector<struct sCellInfo> tmp_cols( FOCUSRITE_SAFFIRELE_96KMIX_NB_COLS );
        std::vector< std::vector<struct sCellInfo> > tmp_all(FOCUSRITE_SAFFIRELE_96KMIX_NB_ROWS,tmp_cols);
        m_CellInfo = tmp_all;
    
        struct sCellInfo c;
        c.row=-1;
        c.col=-1;
        c.valid=false;
        c.address=0;
        
        for (int i=0;i<FOCUSRITE_SAFFIRELE_96KMIX_NB_ROWS;i++) {
            for (int j=0;j<FOCUSRITE_SAFFIRELE_96KMIX_NB_COLS;j++) {
                m_CellInfo.at(i).at(j) = c;
            }
        }

        // now set the cells that are valid
        setCellInfo(0,4,FR_SAFFIRELE_CMD_ID_IN1_TO_RECMIX_96K, true);
        setCellInfo(1,4,FR_SAFFIRELE_CMD_ID_IN2_TO_RECMIX_96K, true);
        setCellInfo(2,4,FR_SAFFIRELE_CMD_ID_IN3_TO_RECMIX_96K, true);
        setCellInfo(3,4,FR_SAFFIRELE_CMD_ID_IN4_TO_RECMIX_96K, true);
        setCellInfo(4,4,FR_SAFFIRELE_CMD_ID_SPDIF1_TO_RECMIX_96K, true);
        setCellInfo(5,4,FR_SAFFIRELE_CMD_ID_SPDIF2_TO_RECMIX_96K, true);

        setCellInfo(14,0,FR_SAFFIRELE_CMD_ID_RECMIX_TO_OUT1_96K, true);
        setCellInfo(14,1,FR_SAFFIRELE_CMD_ID_RECMIX_TO_OUT2_96K, true);
        setCellInfo(14,2,FR_SAFFIRELE_CMD_ID_RECMIX_TO_OUT3_96K, true);
        setCellInfo(14,3,FR_SAFFIRELE_CMD_ID_RECMIX_TO_OUT4_96K, true);

        setCellInfo(7,0,FR_SAFFIRELE_CMD_ID_PC1_TO_OUT1_96K, true);
        setCellInfo(7,1,FR_SAFFIRELE_CMD_ID_PC1_TO_OUT2_96K, true);
        setCellInfo(7,2,FR_SAFFIRELE_CMD_ID_PC1_TO_OUT3_96K, true);
        setCellInfo(7,3,FR_SAFFIRELE_CMD_ID_PC1_TO_OUT4_96K, true);
        setCellInfo(8,0,FR_SAFFIRELE_CMD_ID_PC2_TO_OUT1_96K, true);
        setCellInfo(8,1,FR_SAFFIRELE_CMD_ID_PC2_TO_OUT2_96K, true);
        setCellInfo(8,2,FR_SAFFIRELE_CMD_ID_PC2_TO_OUT3_96K, true);
        setCellInfo(8,3,FR_SAFFIRELE_CMD_ID_PC2_TO_OUT4_96K, true);
    } else {
        debugError("Invalid mixer type\n");
    }
}

void SaffireMatrixMixer::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Saffire Matrix mixer type %d\n");
}

} // Focusrite
} // BeBoB
