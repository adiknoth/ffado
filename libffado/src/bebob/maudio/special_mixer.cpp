/*
 * Copyright (C) 2014      by Takashi Sakamoto
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

#include <string>
#include <sstream>
#include <iostream>
#include <assert.h>
#include <vector>

#include "libieee1394/ieee1394service.h"
#include "libieee1394/configrom.h"

#include "libcontrol/BasicElements.h"
#include "libutil/ByteSwap.h"

#include "bebob/bebob_avdevice.h"
#include "bebob/bebob_avdevice_subunit.h"

#include "bebob/maudio/special_mixer.h"
#include "bebob/maudio/special_avdevice.h"

using namespace AVC;

namespace BeBoB {
namespace MAudio {
namespace Special {

class Device;

IMPL_DEBUG_MODULE(Mixer, Mixer, DEBUG_LEVEL_NORMAL);

Mixer::Mixer(Device &dev)
    : Control::Container((Device *)&dev)
    , m_dev((Device *) &dev)
{
    int i;
    for (i = 1; i < 28; i++)
        addElement(new Volume(dev, i));
    for (i = 1; i < 10; i++)
        addElement(new LRBalance(dev, i));
    for (i = 1; i < 5; i++)
        addElement(new Selector(dev, i));
    for (i = 1; i < 3; i++)
        addElement(new Processing(dev, i));

    if (!initialize(dev))
        debugWarning("Could not initialize mixer settings\n");

    if (!((Device *) &dev)->addElement(this))
        debugWarning("Could not add BeBoB::MAudio::Special::Mixer to Control::Container\n");
}

bool Mixer::initialize(Device &dev)
{
    uint32_t *data;
    unsigned int i;
    bool err;

    data = (uint32_t *)malloc(0x9c + 4);
    if (data == NULL)
        return false;

    /* inputs volumes */
    for (i = 0; i < 16; i++)
        data[i] = 0x00000000;
    /* inputs panning */
    for (i = 16; i < 25; i++)
        data[i] = 0x7ffe8000;
    /* aux inputs volume */
    for (i = 25; i < 36; i++)
        data[i] = 0x00000000;
    /* analog inputs to mixer */
    data[36] = 0x00000000;
    /* stream inputs to mixer */
    data[37] = 0x00000000;
    /* headphone sources */
    data[38] = 0x00000000;
    /* output sources */
    data[39] = 0x00000000;

    err = dev.writeBlk(0x00, 0x9c + 4, data);
    free(data);

    return err;
}


/*
 * Output source control:
 * Path               Target         Offset
 * /Mixer/Selector_1  HP 1/2 out     0x98
 * /Mixer/Selector_2  HP 3/4 out     0x98
 * /Mixer/Selector_3  Analog 1/2 out 0x9C
 * /Mixer/Selector_4  Analog 3/4 out 0x9C
 * There seems not to be for digital output.
 */
Selector::Selector(Device& dev, unsigned int id)
 : Control::Discrete((Device *)&dev)
 , m_dev(&dev)
 , m_id(id)
{
    std::ostringstream ostrm;
    ostrm << "Selector_" << id;
    Control::Discrete::setName(ostrm.str());

    ostrm.str("");
    ostrm << "Label for Selector " << id;
    setLabel(ostrm.str());

    ostrm.str("");
    ostrm << "Description for Selector " << id;
    setDescription(ostrm.str());
}

uint64_t Selector::getOffset()
{
    if (m_id < 3)
        return 0x98;
    else
        return 0x9c;
}

bool Selector::setValue(int id, int v)
{
    uint32_t data, value;

    if (!m_dev->readReg(getOffset(), &data))
        return false;

    /* for headphone out */
    if (m_id < 3) {
        if (v == 2)
            value = 0x04;
        else if (v == 1)
            value = 0x02;
        else
            value = 0x01;

        if (m_id == 1)
            data = (data & 0xffff0000) | value;
        else
            data = (value << 16) | (data & 0xffff);
    } else {
        value = 0x01 & v;
        if (m_id == 3)
            data = (data & 0x02) | value;
        else
            data = (value << 1) | (data & 0x01);
    }

    return m_dev->writeReg(getOffset(), data);
}

int Selector::getValue(int id)
{
    uint32_t data, value;

    if (!m_dev->readReg(getOffset(), &data))
        return 0;

    if (m_id < 3) {
        if (m_id == 1)
            data = data & 0xffff;
        else
            data = data >> 16;

        if (data & 0x04)
            value = 2;
        else if (data & 0x02)
            value = 1;
        else
            value = 0;
    } else if (m_id == 3)
        value = 0x01 & data;
    else
        value = (0x02 & data) >> 1;

    return value;
}

/*
 * Input volume control:
 * Path                      Target           Offset
 * /Mixer/Feature_Volume_1   Analog 1/2 in    0x10
 * /Mixer/Feature_Volume_2   Analog 3/4 in    0x14
 * /Mixer/Feature_Volume_3   Analog 5/6 in    0x18
 * /Mixer/Feature_Volume_4   Analog 7/8 in    0x1c
 * /Mixer/Feature_Volume_5   SPDIF 1/2 in     0x20
 * /Mixer/Feature_Volume_6   ADAT 1/2 in      0x24
 * /Mixer/Feature_Volume_7   ADAT 3/4 in      0x28
 * /Mixer/Feature_Volume_8   ADAT 5/6 in      0x2c
 * /Mixer/Feature_Volume_9   ADAT 7/8 in      0x30
 * /Mixer/Feature_Volume_10  Stream 1/2 in    0x00
 * /Mixer/Feature_Volume_11  Stream 3/4 in    0x04
 *
 * Output volume control:
 * /Mixer/Feature_Volume_12  Analog 1/2 out   0x08
 * /Mixer/Feature_Volume_13  Analog 3/4 out   0x0c
 * /Mixer/Feature_Volume_14  AUX 1/2 out      0x34
 * /Mixer/Feature_Volume_15  HP 1/2 out       0x38
 * /Mixer/Feature_Volume_16  HP 3/4 out       0x3c
 *
 * Aux input control:
 * Path                      Target           Offset
 * /Mixer/Feature_Volume_17  Stream 1/2 in    0x64
 * /Mixer/Feature_Volume_18  Stream 3/4 in    0x68
 * /Mixer/Feature_Volume_19  Analog 1/2 in    0x6c
 * /Mixer/Feature_Volume_20  Analog 3/4 in    0x70
 * /Mixer/Feature_Volume_21  Analog 5/6 in    0x74
 * /Mixer/Feature_Volume_22  Analog 7/8 in    0x78
 * /Mixer/Feature_Volume_23  S/PDIF 1/2 in    0x7c
 * /Mixer/Feature_Volume_24  ADAT 1/2 in      0x80
 * /Mixer/Feature_Volume_25  ADAT 3/4 in      0x84
 * /Mixer/Feature_Volume_26  ADAT 5/6 in      0x88
 * /Mixer/Feature_Volume_27  ADAT 7/8 in      0x8c
 */
Volume::Volume(Device &dev, unsigned int id)
 : Control::Continuous((Device *)&dev)
 , m_dev(&dev)
 , m_id(id)
{
    std::ostringstream ostrm;
    ostrm << "Feature_Volume_" << id;
    Control::Continuous::setName(ostrm.str());

    ostrm.str("");
    ostrm << "Label for Feature Volume" << id;
    setLabel(ostrm.str());

    ostrm.str("");
    ostrm << "Description for Feature Volume " << id;
    setDescription(ostrm.str());
}

uint64_t Volume::getOffset()
{
    if ((m_id > 0) && (m_id < 10))
        return (m_id - 1) * 4 + 0x10;
    else if (m_id < 12)
        return (m_id - 10) * 4;
    else if (m_id < 14)
        return (m_id - 12) * 4 + 0x08;
    else if (m_id < 17)
        return (m_id - 14) * 4 + 0x34;
    else
        return (m_id - 17) * 4 + 0x64;
}

bool Volume::setValue(int idx, double v)
{
    uint32_t data;

    if (!m_dev->readReg(getOffset(), &data))
        return false;

    /* mute */
    if (v == 0x8000) {
        data = 0x80008000;
    /* unmute */
    } else if (v == 0x0000) {
        data = 0x00000000;
    /* others */
    } else {
        if (idx > 1)
            data = (data & 0xffff0000) | ((uint32_t)v & 0xffff);
        else
            data = (((uint32_t)v & 0xffff) << 16) | (data & 0xffff);
    }

    return m_dev->writeReg(getOffset(), data);
}

double Volume::getValue(int idx)
{
    uint32_t data;

    if (!m_dev->readReg(getOffset(), &data))
        return 0;

    if (idx > 1)
        return data & 0xffff;
    else
        return data >> 16;
}

/*
 * Input L/R balance control:
 * Path                        Target         Offset
 * /Mixer/Feature_LRBalance_1  Analog 1/2 in  0x40
 * /Mixer/Feature_LRBalance_2  Analog 3/4 in  0x44
 * /Mixer/Feature_LRBalance_3  Analog 5/6 in  0x48
 * /Mixer/Feature_LRBalance_4  Analog 7/8 in  0x4c
 * /Mixer/Feature_LRBalance_5  S/PDIF 1/2 in  0x50
 * /Mixer/Feature_LRBalance_6  ADAT 1/2 in    0x54
 * /Mixer/Feature_LRBalance_7  ADAT 3/4 in    0x58
 * /Mixer/Feature_LRBalance_8  ADAT 5/6 in    0x5c
 * /Mixer/Feature_LRBalance_9  ADAT 7/8 in    0x60
 */

LRBalance::LRBalance(Device &dev, unsigned int id)
 : Control::Continuous((Device *)&dev)
 , m_dev(&dev)
 , m_id(id)
{
    std::ostringstream ostrm;
    ostrm << "Feature_LRBalance_" << id;
    Control::Continuous::setName(ostrm.str());

    ostrm.str("");
    ostrm << "Label for L/R Balance " << id;
    setLabel(ostrm.str());

    ostrm.str("");
    ostrm << "Description for L/R Balance " << id;
    setDescription(ostrm.str());
}

uint64_t LRBalance::getOffset()
{
    return (m_id - 1) * 4 + 0x40;
}

bool LRBalance::setValue(int idx, double v)
{
    uint32_t data;

    if (!m_dev->readReg(getOffset(), &data))
        return false;

    if (idx > 1)
        data = (data & 0xffff0000) | ((uint32_t)v & 0xffff);
    else
        data = (data & 0xffff) | (((uint32_t)v & 0xffff) << 16);

    return m_dev->writeReg(getOffset(), data);
}

double LRBalance::getValue(int idx)
{
    uint32_t data;
    int16_t val;

    if (!m_dev->readReg(getOffset(), &data))
        return 0;

    if (idx > 1)
        val = data & 0xff00;
    else
        val = (data >> 16) & 0xff00;

    return val;
}

/*
 * Mixer input control:
 * Path                    Target         Offset (Ana/Strm)
 * /Mixer/EnhancedMixer_1  Mixer 1/2 out  0x90/0x9c
 * /Mixer/EnhancedMixer_2  Mixer 3/4 out  0x90/0x9c
 */
Processing::Processing(Device& dev, unsigned int id)
 : Control::Continuous((Device *)&dev)
 , m_dev(&dev)
 , m_id(id)
{
    std::ostringstream ostrm;
    ostrm << "EnhancedMixer_" << id;
    Control::Continuous::setName(ostrm.str());

    ostrm.str("");
    ostrm << "Label for EnhancedMixer " << id;
    setLabel(ostrm.str());

    ostrm.str("");
    ostrm << "Description for EnhancedMixer " << id;
    setDescription(ostrm.str());
}

uint64_t Processing::getOffset(int iPlugNum)
{
    /* Stream inputs */
    if (iPlugNum != 2)
        return 0x90;
    /* Analog inputs */
    /* S/PDIF inputs */
    /* ADAT inputs */
    else
        return 0x94;
}

bool Processing::setValue(int idx, double v)
{
    int iPlugNum, iAChNum;
    uint64_t offset;
    uint32_t data, value, mask;

    iPlugNum = (idx >> 8) & 0x0f;
    iAChNum  = (idx >> 4) & 0x0f;

    offset = getOffset(iPlugNum);
    if (!m_dev->readReg(offset, &data))
        return false;

    if (v != 0x00)
        value = 0;
    else
        value = 1;

    /* analog inputs */
    if (iPlugNum == 1) {
        mask = 0x01 << (iAChNum / 2);
        value = value << (iAChNum / 2);

        /* mixer 3/4 out */
        if (m_id > 1) {
            mask <<= 4;
            value <<= 4;
        }
    /* stream inputs */
    } else if (iPlugNum == 2) {
        mask = 0x01;
        if (iAChNum > 1) {
            mask <<= 2;
            value <<= 2;
        }
        if (m_id > 1) {
            mask <<= 1;
            value <<= 1;
        }
    /* S/PDIF inputs */
    } else if (iPlugNum == 3) {
        mask = 0x01 << (iAChNum / 2);
        value = value << (iAChNum / 2);

        mask <<= 16;
        value <<= 16;

        /* mixer 3/4 out */
        if (m_id > 1) {
            mask <<= 1;
            value <<= 1;
        }
    /* ADAT inputs */
    } else {
        mask = 0x01 << (iAChNum / 2);
        value = value << (iAChNum / 2);

        mask <<= 8;
        value <<= 8;
        /* mixer 3/4 out */
        if (m_id > 1) {
            mask <<= 4;
            value <<= 4;
        }
    }

    data &= ~mask;
    data |= value;

    return m_dev->writeReg(offset, data);
}

double Processing::getValue(int idx)
{
    int iPlugNum, iAChNum, shift;
    uint64_t offset;
    uint32_t data;
    double value;

    iPlugNum = (idx >> 8) & 0xF;
    iAChNum  = (idx >> 4) & 0xF;

    offset = getOffset(iPlugNum);

    if (!m_dev->readReg(offset, &data))
        return false;

    /* analog inputs */
    if (iPlugNum == 1) {
        shift = iAChNum / 2;
        if (m_id > 1)
            shift += 4;
    /* stream inputs */
    } else if (iPlugNum == 2) {
        shift = 0;
        if (iAChNum > 1)
            shift += 1;
        if (m_id > 1)
            shift += 2;
    /* S/PDIF inputs */
    } else if (iPlugNum == 3) {
        shift = iAChNum / 2;
        shift += 16;
    /* ADAT inputs */
    } else {
        shift = iAChNum / 2;
        shift += 8;
        if (m_id > 1)
            shift += 4;
    }

    if ((data >> shift) & 0x01)
        value = 0x0000;
    else
        value = 0x8000;

    return value;
}

} // namespace Special
} // namespace MAudio
} // namespace BeBoB
