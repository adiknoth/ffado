/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __FFADO_DELAYLOCKEDLOOP__
#define __FFADO_DELAYLOCKEDLOOP__

namespace Util {

class DelayLockedLoop {

public:

    DelayLockedLoop(unsigned int order, float *coeffs);
    DelayLockedLoop(unsigned int order);
    DelayLockedLoop();

    virtual ~DelayLockedLoop();

    float getCoefficient(unsigned int i);
    void setCoefficient(unsigned int i, float c);

    void setIntegrator(unsigned int i, float c);

    void reset();

    unsigned int getOrder();
    void setOrder(unsigned int i);
    void setOrder(unsigned int order, float* coeffs);

    void put(float v);
    float get();
    float getError();

protected:

    unsigned int m_order;

    float *m_coeffs;
    float *m_nodes;
    float m_error;
};

} // end of namespace FFADOUtil

#endif /* __FFADO_DELAYLOCKEDLOOP__ */


