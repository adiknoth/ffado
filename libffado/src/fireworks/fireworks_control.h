/*
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#ifndef FIREWORKS_MATRIX_CONTROL_H
#define FIREWORKS_MATRIX_CONTROL_H

#include "debugmodule/debugmodule.h"

#include "efc/efc_cmd.h"
#include "efc/efc_cmds_hardware.h"
#include "efc/efc_cmds_mixer.h"
#include "efc/efc_cmds_monitor.h"
#include "efc/efc_cmds_ioconfig.h"

#include "libcontrol/BasicElements.h"
#include "libcontrol/MatrixMixer.h"

#include <pthread.h>

class ConfigRom;
class Ieee1394Service;

namespace FireWorks {

class MonitorControl : public Control::MatrixMixer
{
public:
    enum eMonitorControl {
        eMC_Gain,
        eMC_Solo,
        eMC_Mute,
        eMC_Pan,
    };

public:
    MonitorControl(FireWorks::Device& parent, enum eMonitorControl);
    MonitorControl(FireWorks::Device& parent, enum eMonitorControl, std::string n);
    virtual ~MonitorControl() {};

    virtual void show();

    virtual std::string getRowName( const int );
    virtual std::string getColName( const int );
    virtual int canWrite( const int, const int );
    virtual double setValue( const int, const int, const double );
    virtual double getValue( const int, const int );
    virtual int getRowCount( );
    virtual int getColCount( );

protected:
    enum eMonitorControl        m_control;
    FireWorks::Device&          m_ParentDevice;
};


class SimpleControl : public Control::Continuous
{

public:
    SimpleControl(FireWorks::Device& parent,
                  enum eMixerTarget, enum eMixerCommand,
                  int channel);
    SimpleControl(FireWorks::Device& parent,
                  enum eMixerTarget, enum eMixerCommand,
                  int channel, std::string n);
    virtual ~SimpleControl();

    virtual void show();

    virtual bool setValue( const double );
    virtual double getValue( );
    virtual bool setValue(int idx, double v)
        {return setValue(v);};
    virtual double getValue(int idx)
        {return getValue();};

    virtual double getMinimum() {return -100.0;};
    virtual double getMaximum() {return 10.0;};

protected:
    EfcGenericMixerCmd*         m_Slave;
    FireWorks::Device&          m_ParentDevice;
};

// for on-off type of controls

class BinaryControl : public Control::Discrete
{

public:
    BinaryControl(FireWorks::Device& parent,
                  enum eMixerTarget, enum eMixerCommand,
                  int channel, int bit);
    BinaryControl(FireWorks::Device& parent,
                  enum eMixerTarget, enum eMixerCommand,
                  int channel, int bit, std::string n);
    virtual ~BinaryControl();

    virtual void show();

    virtual bool setValue( const int );
    virtual int getValue( );
    virtual bool setValue(int idx, int v)
        {return setValue(v);};
    virtual int getValue(int idx)
        {return getValue();};

    virtual int getMinimum() {return 0;};
    virtual int getMaximum() {return 1;};
protected:
    int                         m_bit;
    EfcGenericMixerCmd*         m_Slave;
    FireWorks::Device&          m_ParentDevice;
};

class SpdifModeControl : public Control::Discrete
{

public:
    SpdifModeControl(FireWorks::Device& parent);
    SpdifModeControl(FireWorks::Device& parent,
                    std::string n);
    virtual ~SpdifModeControl();

    virtual void show();

    virtual bool setValue( const int );
    virtual int getValue( );
    virtual bool setValue(int idx, int v)
        {return setValue(v);};
    virtual int getValue(int idx)
        {return getValue();};

    virtual int getMinimum() {return 0;};
    virtual int getMaximum() {return 0;};

protected:
    FireWorks::Device&          m_ParentDevice;
};


// for on-off type of controls

class IOConfigControl : public Control::Discrete
{

public:
    IOConfigControl(FireWorks::Device& parent,
                    enum eIOConfigRegister);
    IOConfigControl(FireWorks::Device& parent,
                    enum eIOConfigRegister,
                    std::string n);
    virtual ~IOConfigControl();

    virtual void show();

    virtual bool setValue( const int );
    virtual int getValue( );
    virtual bool setValue(int idx, int v)
        {return setValue(v);};
    virtual int getValue(int idx)
        {return getValue();};

    virtual int getMinimum() {return 0;};
    virtual int getMaximum() {return 0;};

protected:
    int                         m_bit;
    EfcGenericIOConfigCmd*      m_Slave;
    FireWorks::Device&          m_ParentDevice;
};


class HwInfoControl : public Control::Discrete
{
public:

    enum eHwInfoField {
        eHIF_PhysicalAudioOutCount,
        eHIF_PhysicalAudioInCount,
        eHIF_1394PlaybackCount,
        eHIF_1394RecordCount,
        eHIF_GroupOutCount,
        eHIF_GroupInCount,
        eHIF_PhantomPower,
    };
public:
    HwInfoControl(FireWorks::Device& parent,
                  enum eHwInfoField);
    HwInfoControl(FireWorks::Device& parent,
                  enum eHwInfoField, std::string n);
    virtual ~HwInfoControl();

    virtual void show();

    virtual bool setValue( const int ) {return false;};
    virtual int getValue( );
    virtual bool setValue(int idx, int v)
        {return setValue(v);};
    virtual int getValue(int idx)
        {return getValue();};

    virtual int getMinimum() {return 0;};
    virtual int getMaximum() {return 0;};

protected:
    FireWorks::Device&          m_ParentDevice;
    enum eHwInfoField           m_Field;
};

class MultiControl : public Control::Discrete
{
public:

    enum eType {
        eT_SaveSession,
        eT_Identify,
    };
public:
    MultiControl(FireWorks::Device& parent, enum eType);
    MultiControl(FireWorks::Device& parent, enum eType,
                        std::string n);
    virtual ~MultiControl();

    virtual void show();

    virtual bool setValue( const int );
    virtual int getValue( ) {return 0;};
    virtual bool setValue(int idx, int v)
        {return setValue(v);};
    virtual int getValue(int idx)
        {return getValue();};

    virtual int getMinimum() {return 0;};
    virtual int getMaximum() {return 0;};

protected:
    FireWorks::Device&  m_ParentDevice;
    enum eType          m_Type;
};

} // namespace FireWorks

#endif
