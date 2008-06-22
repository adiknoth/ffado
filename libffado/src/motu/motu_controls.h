/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2008 by Jonathan Woithe
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

#include "debugmodule/debugmodule.h"

#include "libcontrol/BasicElements.h"
#include "libcontrol/MatrixMixer.h"

namespace Motu {

class MotuDevice;

#define MOTU_CTRL_CHANNEL_FADER   0x00000001
#define MOTU_CTRL_CHANNEL_PAN     0x00000002
#define MOTU_CTRL_CHANNEL_SOLO    0x00000004
#define MOTU_CTRL_CHANNEL_MUTE    0x00000008
#define MOTU_CTRL_MIX_FADER       0x00000100
#define MOTU_CTRL_MIX_MUTE        0x00000200
#define MOTU_CTRL_MIX_DEST        0x00000400

#define MOTU_CTRL_INPUT_TRIMGAIN  0x01000000
#define MOTU_CTRL_INPUT_PAD       0x02000000
#define MOTU_CTRL_INPUT_LEVEL     0x04000000
#define MOTU_CTRL_INPUT_BOOST     0x08000000
#define MOTU_CTRL_PHONES_SRC      0x10000000
#define MOTU_CTRL_OPTICAL_MODE    0x20000000

#define MOTU_CTRL_STD_CHANNEL \
    (MOTU_CTRL_CHANNEL_FADER|MOTU_CTRL_CHANNEL_PAN|\
     MOTU_CTRL_CHANNEL_SOLO|MOTU_CTRL_CHANNEL_MUTE)

#define MOTU_CTRL_STD_MIX \
    (MOTU_CTRL_MIX_FADER|MOTU_CTRL_MIX_MUTE|\
     MOTU_CTRL_MIX_DEST)

#define MOTU_CTRL_TRAVELER_MIC_INPUT_CTRLS \
    (MOTU_CTRL_INPUT_TRIMGAIN|MOTU_CTRL_INPUT_PAD)
#define MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS \
    (MOTU_CTRL_INPUT_LEVEL|MOTU_CTRL_INPUT_BOOST)

#define MOTU_CTRL_MASK_MUTE_VALUE          0x00010000
#define MOTU_CTRL_MASK_MUTE_SETENABLE      0x01000000
#define MOTU_CTRL_MASK_SOLO_VALUE          0x00020000
#define MOTU_CTRL_MASK_SOLO_SETENABLE      0x02000000

#define MOTU_CTRL_MASK_ANA5_INPUT_LEVEL    0x00000010
#define MOTU_CTRL_MASK_ANA6_INPUT_LEVEL    0x00000020
#define MOTU_CTRL_MASK_ANA7_INPUT_LEVEL    0x00000040
#define MOTU_CTRL_MASK_ANA8_INPUT_LEVEL    0x00000080

#define MOTU_CTRL_MODE_PAD                 0x00000000
#define MOTU_CTRL_MODE_TRIMGAIN            0x00000001

#define MOTU_INFO_IS_STREAMING             0x00000001
#define MOTU_INFO_SAMPLE_RATE		   0x00000002
#define MOTU_INFO_HAS_MIC_INPUTS           0x00000003
#define MOTU_INFO_HAS_AESEBU_INPUTS        0x00000004
#define MOTU_INFO_HAS_SPDIF_INPUTS         0x00000005
#define MOTU_INFO_HAS_OPTICAL_SPDIF        0x00000006

#define MOTU_CTRL_TRIMGAINPAD_MAX_CHANNEL  3

class MotuDiscreteCtrl
    : public Control::Discrete
{
public:
    MotuDiscreteCtrl(MotuDevice &parent, unsigned int dev_reg);
    MotuDiscreteCtrl(MotuDevice &parent, unsigned int dev_reg,
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v) = 0;
    virtual int getValue() = 0;

    // default implementations
    virtual bool setValue(int idx, int v)
        {return setValue(v);};
    virtual int getValue(int idx)
        {return getValue();};

    virtual int getMinimum() {return 0;};
    virtual int getMaximum() {return 0;};

protected:
    MotuDevice    &m_parent;
    unsigned int  m_register;
};

class MotuBinarySwitch
    : public MotuDiscreteCtrl
{
public:
    MotuBinarySwitch(MotuDevice &parent, unsigned int dev_reg,
      unsigned int val_mask, unsigned int setenable_mask);
    MotuBinarySwitch(MotuDevice &parent, unsigned int dev_reg,
        unsigned int val_mask, unsigned int setenable_mask, 
        std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();

protected:
    unsigned int m_value_mask;
    unsigned int m_setenable_mask;
};

class ChannelFader
    : public MotuDiscreteCtrl
{
public:
    ChannelFader(MotuDevice &parent, unsigned int dev_reg);
    ChannelFader(MotuDevice &parent, unsigned int dev_reg,
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
};

class ChannelPan
    : public MotuDiscreteCtrl
{
public:
    ChannelPan(MotuDevice &parent, unsigned int dev_reg);
    ChannelPan(MotuDevice &parent, unsigned int dev_reg,
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
};

class MotuMatrixMixer : public Control::MatrixMixer
{
public:
    MotuMatrixMixer(MotuDevice &parent);
    MotuMatrixMixer(MotuDevice &parent, std::string name);
    virtual ~MotuMatrixMixer() {};

    void addRowInfo(std::string name, unsigned int flags, unsigned int address);
    void addColInfo(std::string name, unsigned int flags, unsigned int address);
    uint32_t getCellRegister(const unsigned int row, const unsigned int col);

    virtual void show();

    virtual std::string getRowName(const int row);
    virtual std::string getColName(const int col);
    virtual int canWrite( const int, const int ) { return true; }
    virtual int getRowCount();
    virtual int getColCount();

protected:
     struct sSignalInfo {
         std::string name;
         unsigned int flags;
         unsigned int address;
     };

     std::vector<struct sSignalInfo> m_RowInfo;
     std::vector<struct sSignalInfo> m_ColInfo;

     MotuDevice& m_parent;
};

class ChannelFaderMatrixMixer : public MotuMatrixMixer
{
public:
    ChannelFaderMatrixMixer(MotuDevice &parent);
    ChannelFaderMatrixMixer(MotuDevice &parent, std::string name);
    virtual double setValue(const int row, const int col, const double val);
    virtual double getValue(const int row, const int col);
};

class ChannelPanMatrixMixer : public MotuMatrixMixer
{
public:
    ChannelPanMatrixMixer(MotuDevice &parent);
    ChannelPanMatrixMixer(MotuDevice &parent, std::string name);
    virtual double setValue(const int row, const int col, const double val);
    virtual double getValue(const int row, const int col);
};

class ChannelBinSwMatrixMixer : public MotuMatrixMixer
{
public:
    ChannelBinSwMatrixMixer(MotuDevice &parent);
    ChannelBinSwMatrixMixer(MotuDevice &parent, std::string name, 
      unsigned int val_mask, unsigned int setenable_mask);
    virtual double setValue(const int row, const int col, const double val);
    virtual double getValue(const int row, const int col);

protected:
    unsigned int m_value_mask;
    unsigned int m_setenable_mask;
};



class MixFader
    : public MotuDiscreteCtrl
{
public:
    MixFader(MotuDevice &parent, unsigned int dev_reg);
    MixFader(MotuDevice &parent, unsigned int dev_reg,
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
};

class MixMute
    : public MotuDiscreteCtrl
{
public:
    MixMute(MotuDevice &parent, unsigned int dev_reg);
    MixMute(MotuDevice &parent, unsigned int dev_reg, 
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
};

class MixDest
    : public MotuDiscreteCtrl
{
public:
    MixDest(MotuDevice &parent, unsigned int dev_reg);
    MixDest(MotuDevice &parent, unsigned int dev_reg, 
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
};

class PhonesSrc
    : public MotuDiscreteCtrl
{
public:
    PhonesSrc(MotuDevice &parent);
    PhonesSrc(MotuDevice &parent, 
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
};

class OpticalMode
    : public MotuDiscreteCtrl
{
public:
    OpticalMode(MotuDevice &parent, unsigned int dev_reg);
    OpticalMode(MotuDevice &parent, unsigned int dev_reg, 
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
};

class InputGainPad
    : public MotuDiscreteCtrl
{
public:
    InputGainPad(MotuDevice &parent, unsigned int channel, unsigned int mode);
    InputGainPad(MotuDevice &parent, unsigned int channel, unsigned int mode, 
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
protected:
    void validate();
    unsigned int dev_register();
    unsigned int m_mode;
};

class InfoElement
    : public MotuDiscreteCtrl
{
public:
    InfoElement(MotuDevice &parent, unsigned infotype);
    InfoElement(MotuDevice &parent, unsigned infotype,
          std::string name, std::string label, std::string descr);
    virtual bool setValue(int v);
    virtual int getValue();
};

}
