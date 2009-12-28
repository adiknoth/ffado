/*
 * Copyright (C) 2009 by Pieter Palmers
 * Copyright (C) 2009 by Arnold Krille
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

#ifndef DICE_FOCUSRITE_FOCUSRITE_EAP_H
#define DICE_FOCUSRITE_FOCUSRITE_EAP_H

#include "dice/dice_eap.h"

#include "libieee1394/configrom.h"

namespace Dice {
namespace Focusrite {

class FocusriteEAP : public Dice::EAP
{
public:
    /**
     * @brief A standard-switch for boolean.
     *
     * If you don't like True and False for the labels, subclass and return your own.
    */
    class Switch : public Control::Boolean
    {
    public:
        Switch(Dice::Focusrite::FocusriteEAP*, std::string name, size_t offset, int activevalue);

        bool selected();
        bool select(bool);

    private:
        Dice::Focusrite::FocusriteEAP* m_eap;
        bool m_selected;
        size_t m_offset;
        int m_activevalue;
        fb_quadlet_t m_state_tmp;
    };

    class Poti : public Control::Discrete
    {
        public:
            Poti(Dice::Focusrite::FocusriteEAP*, std::string name, size_t offset);

            int getValue(int) { return getValue(); }
            bool setValue(int, int n) { return setValue(n); }

            int getMinimum() { return -127; }
            int getMaximum() { return 0; }

            int getValue();
            bool setValue(int n);
        private:
            Dice::Focusrite::FocusriteEAP* m_eap;
            size_t m_offset;
            int m_value;
            quadlet_t m_tmp;
    };

    class MonitorSection : public Control::Container
    {
    public:
        MonitorSection(Dice::Focusrite::FocusriteEAP*, std::string name);

    private:
        Dice::Focusrite::FocusriteEAP* m_eap;
        Switch *m_mute, *m_dim;
        std::vector<Switch*> m_mute_affected;
        std::vector<Switch*> m_dim_affected;
        std::vector<Switch*> m_mono;
        Poti* m_monitorlevel;
        Poti* m_dimlevel;
    };


public:
    FocusriteEAP(Dice::Device&);

    bool readApplicationReg(unsigned offset, quadlet_t*);
    bool writeApplicationReg(unsigned offset, quadlet_t);

protected:
    virtual int commandToFix(unsigned offset) =0;
    virtual Poti* getMonitorPoti(std::string) { return 0; }
    virtual Poti* getDimPoti(std::string) { return 0; }
};

}
}

#endif
// vim: et
