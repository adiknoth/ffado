/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */

#include "DelayLockedLoop.h"

namespace Util {

/**
 * Creates a new Delay Locked Loop with a predefined order and
 * a predefined set of coefficients.
 *
 * Make sure coeffs is a float array containing order+1 coefficients.
 *
 * @pre order > 0
 * @param order order of the DLL
 * @param coeffs coefficients to use
 */
DelayLockedLoop::DelayLockedLoop(unsigned int order, float *coeffs) 
{
    unsigned int i;
    
    m_order=order;
    if (m_order==0) m_order=1;
    
    m_coeffs=new float[order];
    m_nodes=new float[order];
    
    for (i=0;i<order;i++) {
        m_coeffs[i]=coeffs[i];
        m_nodes[i]=0.0;
    }

}

/**
 * Creates a new Delay Locked Loop with a predefined order.
 * All coefficients are set to 0.
 *
 * @pre order > 0
 * @param order order of the DLL
 */
DelayLockedLoop::DelayLockedLoop(unsigned int order)
{
    unsigned int i;
    
    m_order=order;
    if (m_order==0) m_order=1;
    
    m_coeffs=new float[order];
    m_nodes=new float[order];
    
    for (i=0;i<order;i++) {
        m_coeffs[i]=0.0;
        m_nodes[i]=0.0;
    }
}

/**
 * Creates a new first order Delay Locked Loop.
 * The coefficient is set to 0.
 *
 */
DelayLockedLoop::DelayLockedLoop() {
    m_order=1;
    
    m_coeffs=new float[1];
    m_coeffs[0]=0.0;
    
    m_nodes=new float[1];
    m_nodes[0]=0.0;
}

DelayLockedLoop::~DelayLockedLoop() {
    if(m_coeffs) delete[] m_coeffs;
    if(m_nodes) delete[] m_nodes;
    
}
    
/**
 * Returns the coefficient with index i
 *
 * i should be smaller than the DLL order
 *
 * @param i index of the coefficient
 * @return value of the coefficient
 */
float 
DelayLockedLoop::getCoefficient(unsigned int i) {
    
    unsigned int x;
    if (x<m_order) {
        return m_coeffs[x];
    } else {
        return 0.0;
    }
}
/**
 * Sets the value of the coefficient with index i
 *
 * i should be smaller than the DLL order
 * 
 * @param i index of the coefficient
 * @param c value of the coefficient
 */
void
DelayLockedLoop::setCoefficient(unsigned int i, float c) {
    
    unsigned int x;
    if (x<m_order) {
        m_coeffs[x]=c;
    }
}

/**
 * Sets the value of the integrator output with index i.
 * This allows to set the inital values
 *
 * i should be smaller than the DLL order
 * 
 *
 * @param i index of the integrator
 * @param c value of the integrator output
 */
void
DelayLockedLoop::setIntegrator(unsigned int i, float c) {
    
    if (i<m_order) {
        m_nodes[i]=c;
    }
}

/**
 * Clears the internal state of the DLL, 
 * meaning that it will set all internal nodes to 0.0
 */
void 
DelayLockedLoop::reset() {
    
    unsigned int i;
    for (i=0;i<m_order;i++) {
        m_nodes[i]=0.0;
    }
    
}

/**
 * Returns the order of the DLL
 * @return DLL order
 */
unsigned int 
DelayLockedLoop::getOrder() {
    return m_order;
}

/**
 * Set the order of the DLL
 * with setting of new coefficients
 *
 * Make sure coeffs is a float array containing 'order' coefficients.
 *
 * resets the DLL before changing the order
 *
 * @pre order > 0
 * @param order new order for the DLL
 * @param coeffs coefficients to use
 */
void 
DelayLockedLoop::setOrder(unsigned int order, float* coeffs) {
    unsigned int i;
    
    reset();
    
    m_order=order;
    if (m_order==0) m_order=1;
    
    if(m_coeffs) delete[] m_coeffs;
    m_coeffs=new float[order];
    
    if(m_nodes) delete[] m_nodes;
    m_nodes=new float[order];
    
    for (i=0;i<order;i++) {
        m_coeffs[i]=coeffs[i];
        m_nodes[i]=0.0;
    }

}

/**
 * Set the order of the DLL,
 * the coefficients are set to 0
 *
 * resets the DLL before changing the order
 *
 * @pre order > 0
 * @param order new order for the DLL
 */
void 
DelayLockedLoop::setOrder(unsigned int order) {
    unsigned int i;
    
    reset();
    
    m_order=order;
    
    if (m_order==0) m_order=1;
    
    if(m_coeffs) delete[] m_coeffs;
    m_coeffs=new float[order];
    
    if(m_nodes) delete[] m_nodes;
    m_nodes=new float[order];
    
    for (i=0;i<order;i++) {
        m_coeffs[i]=0.0;
        m_nodes[i]=0.0;
    }

}
    
/**
 * Put a new value in the loop, updating all internal nodes and
 * the output value.
 *
 * @param v new value
 */
void 
DelayLockedLoop::put(float v) {

    // we write the calculation out
    // to make use of pipeline-ing and out of order execution
    // except for very high order loops, then we do it looped
    
    // error = newval - output
    m_error=v-m_nodes[0];

    // update the output value
    m_nodes[0]+=m_error*m_coeffs[0];
    
    if(m_order==1) return; // we are done
    
    // add the output of the second integrator
    m_nodes[0]+=m_nodes[1];
    
    // update the second integrator
    m_nodes[1]+=m_error*m_coeffs[1];
    
    if(m_order==2) return; // we are done
    
    // if the order is even higher, we are using a loop
    unsigned int i;
    for (i=2; i < m_order;i++) {
        // add the i-th integrator value
        m_nodes[i-1] += m_nodes[i];
        
        // update the i-th integrator
        m_nodes[i] = m_coeffs[i] * m_error;
    }
    
    return;
}

/**
 * Get the output value of the DLL 
 *
 * @return current output value
 */
float 
DelayLockedLoop::get() {
    return m_nodes[0];
}

/**
 * Get the current error signal value
 *
 * @return current error signal value
 */
float 
DelayLockedLoop::getError() {
    return m_error;
}

} // end of namespace FreeBobUtil
