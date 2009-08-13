/*
 * Copyright (C) 2009 by Jonathan Woithe
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

namespace Rme {

/* Control types for an RmeSettingsCtrl object */
#define RME_CTRL_NONE                  0x0000
#define RME_CTRL_PHANTOM_SW            0x0001
#define RME_CTRL_SPDIF_INPUT_MODE      0x0002
#define RME_CTRL_SPDIF_OUTPUT_OPTIONS  0x0003
#define RME_CTRL_CLOCK_MODE            0x0004
#define RME_CTRL_SYNC_REF              0x0005
#define RME_CTRL_DEV_OPTIONS           0x0006
#define RME_CTRL_LIMIT_BANDWIDTH       0x0007
#define RME_CTRL_INPUT_LEVEL           0x0008
#define RME_CTRL_OUTPUT_LEVEL          0x0009
#define RME_CTRL_INSTRUMENT_OPTIONS    0x000a
#define RME_CTRL_WCLK_SINGLE_SPEED     0x000b
#define RME_CTRL_PHONES_LEVEL          0x000c
#define RME_CTRL_INPUT0_OPTIONS        0x000d
#define RME_CTRL_INPUT1_OPTIONS        0x000e
#define RME_CTRL_INPUT2_OPTIONS        0x000f
#define RME_CTRL_FF400_PAD_SW          0x0010
#define RME_CTRL_FF400_INSTR_SW        0x0011

#define RME_CTRL_INFO_MODEL            0x0100
#define RME_CTRL_INFO_TCO_PRESENT      0x0200

/* Control types for an RmeSettingsMatrixCtrl object */
#define RME_MATRIXCTRL_NONE            0x0000
#define RME_MATRIXCTRL_GAINS           0x0001

class Device;

class RmeSettingsCtrl
    : public Control::Discrete
{
public:
    RmeSettingsCtrl(Device &parent, unsigned int type, unsigned int info);
    RmeSettingsCtrl(Device &parent, unsigned int type, unsigned int info,
        std::string name, std::string label, std::string descr);
    virtual bool setValue(int v);
    virtual int getValue();

    virtual bool setValue(int idx, int v) { return setValue(v); };
    virtual int getValue(int idx) { return getValue(); };
    virtual int getMinimum() {return 0; };
    virtual int getMaximum() {return 0; };

protected:
    Device &m_parent;
    unsigned int m_type;
    unsigned int m_value, m_info;

};

class RmeSettingsMatrixCtrl
    : public Control::MatrixMixer
{
public:
    RmeSettingsMatrixCtrl(Device &parent, unsigned int type);
    RmeSettingsMatrixCtrl(Device &parent, unsigned int type,
        std::string name);

    virtual void show();

    virtual std::string getRowName(const int row);
    virtual std::string getColName(const int col);
    virtual int canWrite( const int, const int ) { return true; }
    virtual int getRowCount();
    virtual int getColCount();

    virtual double setValue(const int row, const int col, const double val);
    virtual double getValue(const int row, const int col);

protected:
    Device &m_parent;
    unsigned int m_type;
};

}
