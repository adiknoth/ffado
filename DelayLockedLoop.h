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
#ifndef __FREEBOB_DELAYLOCKEDLOOP__
#define __FREEBOB_DELAYLOCKEDLOOP__

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

} // end of namespace FreeBobUtil

#endif /* __FREEBOB_DELAYLOCKEDLOOP__ */


